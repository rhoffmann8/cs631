#define _XOPEN_SOURCE 1000

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

#include "content_type.h"
#include "defines.h"
#include "files.h"
#include "server.h"
#include "utils.h"

int
sws_serve_file(int sock, struct request *req, struct response *resp) {
	printf("entering serve_file\n");
	FILE *file, *s;
	struct stat stat_buf;
	struct tm time;
	time_t req_time;
	int n, lastmod_size;
	char *tz;
	char buf[BUFF_SIZE];

	//file existence already checked in server.c
	//stat just for mtime
	if (stat(req->realpath, &stat_buf) < 0) {
		perror("stat");
		return -1;
	}

	if ((file = fopen(req->realpath, "r")) == NULL) {
		perror("fopen");
		http_status = STATUS_500;
		return -1;
	}

	if (req->if_mod_since != NULL) {
		strptime(req->if_mod_since, req->date_format, &time);
		tz = getenv("TZ");
		if (setenv("TZ", "", 1) < 0) {
			http_status = STATUS_500;
			perror("setenv");
			return -1;
		}
		tzset();
		req_time = mktime(&time);
		if (tz) {
			if (setenv("TZ", tz, 1) < 0) {
				perror("setenv");
				http_status = STATUS_500;
				return -1;
			}
		} else {
			if (unsetenv("TZ") < 0) {
				perror("setenv");
				http_status = STATUS_500;
				return -1;
			}
		}
		tzset();
		if (req_time > stat_buf.st_mtime)
			http_status = STATUS_304;
	}

	lastmod_size = 64;
	if ((resp->last_modified = malloc(lastmod_size)) == NULL) {
		fprintf(stderr, "malloc error\n");
		http_status = STATUS_500;
		return -1;
	}

	strftime(resp->last_modified, lastmod_size,
		RFC1123_DATE, gmtime(&stat_buf.st_mtime));
	//TODO: change to 0 if 304
	resp->length = stat_buf.st_size;
	printf("getting content type\n");
	resp->mime_type = get_content_type(ctypes,
		strrchr(req->realpath, '.'));
	//resp.content_type = ...

	sws_response_headers(sock, req, resp);

	if ((s = fdopen(sock, "w")) == NULL) {
		perror("fdopen");
		exit(EXIT_FAILURE);
		/* NOTREACHED */
	}

	if (req->method == 0 &&
		strcmp(http_status, STATUS_200) == 0) {
		memset(buf, 0, sizeof(buf));
		while ((n = fread(buf, sizeof(buf), 1, file)) > 0) {
			//TODO: error check
			fwrite(buf, sizeof(buf), 1, s);
		}
		//flush
		fwrite(buf, sizeof(buf), 1, s);
	}
	fclose(s);
	fclose(file);
	printf("leaving serve_file\n");
	return 0;
}

int
sws_execute_cgi(int sock, struct request *req, struct response *resp) {

	struct stat stat_buf;
	pid_t pid;
	int i, n, status, flag;
	int content_fd[2];
	int size_fd[2];
	char server_name[HOST_NAME_MAX];
	char buf[BUFF_SIZE];
	//char last_mod[64];
	char c[2];

	if (stat(req->realpath, &stat_buf) < 0) {
		perror("stat");
		http_status = STATUS_500;
		return -1;
	}

	if (pipe(size_fd) < 0) {
		perror("pipe");
		http_status = STATUS_500;
		return -1;
	}
	if (pipe(content_fd) < 0) {
		perror("pipe");
		http_status = STATUS_500;
		return -1;
	}

	if ((pid = fork()) < 0) {
		perror("forking for cgi");
		http_status = STATUS_500;
		return -1;
	}

	if (pid == 0) {

		//set env
		if (putenv("SERVER_SOFTWARE=SWS/1.0") != 0) {
			perror("putenv");
			http_status = STATUS_500;
			return -1;
		}
		if (gethostname(server_name, sizeof(server_name)) < 0) {
			perror("gethostname");
			http_status = STATUS_500;
			return -1;
		}
		sprintf(buf, "SERVER_NAME=%s", server_name);
		if (putenv(buf) != 0) {
			perror("putenv");
			http_status = STATUS_500;
			return -1;
		}
		/*bzero(buf, sizeof(buf));
		sprintf(buf, "SERVER_PORT=%d", __sws_port);
		if (putenv(buf) != 0) {
			perror("putenv");
			http_status = STATUS_500;
			return -1;
		}*/
		//...

		close(size_fd[0]);
		close(content_fd[0]);

		if (dup2(size_fd[1], STDOUT_FILENO) < 0) {
			perror("dup2");
			http_status = STATUS_500;
			return -1;
		}

		if (execl(req->realpath, req->realpath, NULL) < 0) {
			perror("execl");
			close(size_fd[1]);
			exit(EXIT_FAILURE);
			/* NOTREACHED */
		}
		close(size_fd[1]);

		if (dup2(content_fd[1], STDOUT_FILENO) < 0) {
			perror("dup2");
			http_status = STATUS_500;
			return -1;
		}

		if (execl(req->realpath, req->realpath, NULL) < 0) {
			perror("execl");
			close(content_fd[1]);
			exit(EXIT_FAILURE);
			/* NOTREACHED */
		}
		close(content_fd[1]);

		exit(EXIT_SUCCESS);
		/* NOTREACHED */
	} else {
		//bzero(buf, sizeof(buf));
		//bzero(c, sizeof(c));

		flag = 0;
		//consume headers
		while (n > 0) {
			if ((n = read(content_fd[0], c, 1)) < 0) {
				perror("reading cgi headers");
				return -1;
			}
			if (c[0] == '\n') {
				if (flag)
					break;
				flag = 1;
				if (strncasecmp(buf, "Content-Type:", 13) == 0) {
					if (resp->mime_type != NULL) {
						free(resp->mime_type);
						resp->mime_type = NULL;
					}
					if ((resp->mime_type = malloc(strlen(buf)+13+1)) == NULL) {
						perror("malloc");
						return -1;
					}
					for (i = 14; buf[i] == ' '; i++);
					strncpy(resp->mime_type, &buf[i], strlen(buf));
				}
				if (strncasecmp(buf, "Content-Length:", 15) == 0) {
					for (i = 15; buf[i] == ' '; i++);
					buf[strlen(buf)-1] = '\0';
					resp->length = atoi(&buf[i]);
				}
				//bzero(buf, sizeof(buf));
				memset(buf, 0, sizeof(buf));
			} else {
				strncat(buf, c, 1);
				flag = 0;
			}
		}
		flag = 0;

		n = 0;
		//TODO: Read cgi output in pieces
		while ((n += read(size_fd[0], buf, sizeof(buf))) > 0);
		if (errno) {
			perror("read");
			http_status = STATUS_500;
			return -1;
		}

		if (resp->length == 0)
			resp->length = n;

		if (resp->mime_type == NULL) {
			resp->mime_type = "text/html";
			flag = 1;
		}
		sws_response_headers(sock, req, resp);

		//read contents
		while (read(content_fd[0], buf, sizeof(buf)) > 0) {
			if (send(sock, buf, strlen(buf), 0) < 0) {
				perror("send");
				exit(EXIT_FAILURE);
				/* NOTREACHED */
			}
		}
		if (errno)
			perror("read");

		if (resp->mime_type != NULL && flag != 1) {
			free(resp->mime_type);
			resp->mime_type = NULL;
		}
		waitpid(pid, &status, 0);
	}

	return 0;
}

int
sws_create_index(int sock, struct request *req, struct response *resp, char *serve_dir) {
	printf("index\n");
	DIR *dp;
	struct dirent **dirlist;
	struct stat stat_buf;
	int i, j, n, pos;
	int username_len, home_path_len;
	int homedir;
	char buf[PATH_MAX];
	char *tmp, *username;
	char index[BUFF_SIZE];

	//bzero(index, sizeof(index));
	memset(index, 0, sizeof(index));
	if ((dp = opendir(req->realpath)) == NULL) {
		perror("opendir");
		http_status = STATUS_500;
		return -1;
	}

	homedir = 0;
	home_path_len = 0;

	if (req->path[1] == '~') {
		homedir = 1;
		username = req->path + 2;
		for (username_len = 0; username[username_len] != '/'
			&& username[username_len] != '\0'; username_len++)
			;
		home_path_len = strlen("/home/")+username_len+strlen("/sws");
		tmp = req->realpath + home_path_len;
	} else {
		tmp = req->realpath + strlen(serve_dir);
	}

	printf("got home path props\n");

	strncpy(index, "<html><body><h1>Index of ", 25);
	if (homedir) {
		strncat(index, "~", 1);
		strncat(index, username, username_len);
	}
	if (strlen(tmp) == 0)
		strncat(index, "/", 1);
	else
		strncat(index, tmp, strlen(tmp));
	strncat(index, "</h1><br />", 11);

	if ((n = scandir(req->realpath, &dirlist, NULL, alphasort)) < 0) {
		perror("scandir");
		return -1;
	}

	for (i = 0; i < n; i++) {
		//bzero(buf, sizeof(buf));
		memset(buf, 0, sizeof(buf));
		strncpy(buf, req->realpath, strlen(req->realpath));
		strncat(buf, "/", 1);
		strncat(buf, dirlist[i]->d_name, strlen(dirlist[i]->d_name));
		if (stat(buf, &stat_buf) < 0) {
			perror("stat");
			http_status = STATUS_500;
			return -1;
		}

		if (strcmp(dirlist[i]->d_name, ".") != 0 &&
			strcmp(dirlist[i]->d_name, "..") != 0) {
			strncat(index, "<a href=\"", 9);
			if (homedir) {
				strncat(index, "/~", 2);
				strncat(index, username, username_len);
			}
			strncat(index, tmp, strlen(tmp));
			strncat(index, "/", 1);
			strncat(index, dirlist[i]->d_name,
				strlen(dirlist[i]->d_name));
			if (S_ISDIR(stat_buf.st_mode))
				strncat(index, "/", 1);
			strncat(index, "\">", 2);
			strncat(index, dirlist[i]->d_name,
				strlen(dirlist[i]->d_name));
			if (S_ISDIR(stat_buf.st_mode))
				strncat(index, "/", 1);
			strncat(index, "</a><br />", 10);
		}

		if (strcmp(dirlist[i]->d_name, "..") == 0) {
			strncat(index, "<a href=\"", 9);
			pos = strrchr_pos(tmp, '/', strlen(tmp));
			if (homedir) {
				strncat(index, "/~", 2);
				strncat(index, username, username_len);
			}
			for (j = 0; j < pos; j++) {
				strncat(index, &tmp[j], 1);
			}
			strncat(index, "/", 1);
			strncat(index, "\">", 2);
			strncat(index, "Parent Directory/</a><br />", 27);
		}
		free(dirlist[i]);
	}
	free(dirlist);

	strncat(index, "<p style=\"font-style:italic\">SWS 1.0</p>", 40);
	strncat(index, "</body></html>", 14);
	resp->length = strlen(index);
	resp->mime_type = "text/html";
	//resp->mime_type =
	resp->last_modified = NULL;
	sws_response_headers(sock, req, resp);

	if (req->method == 0) {
		if (send(sock, index, strlen(index), 0) < 0) {
			perror("send");
			exit(EXIT_FAILURE);
			/* NOTREACHED */
		}
	}

	return 0;
}
