#define _XOPEN_SOURCE
#define _SVID_SOURCE

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <features.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

#include "sws.h"
#include "utils.h"

#define LOGFILE "sws.log"
#define BUFF_SIZE 8096

#define STATUS_200 "200 OK"
#define STATUS_301 "301 Moved Permanently"
#define STATUS_302 "302 Moved Temporarily"
#define STATUS_304 "304 Not Modified"
#define STATUS_400 "400 Bad Request"
#define STATUS_403 "403 Forbidden"
#define STATUS_404 "404 Not Found"
#define STATUS_500 "500 Internal Server Error"
#define STATUS_501 "501 Not Implemented"

#define RFC1123_DATE "%a, %d %b %Y %T GMT"
#define RFC850_DATE "%A, %d-%b-%y %T GMT"
#define ASCTIME_DATE "%a %b %e %T %Y"

#define TEXT_HTML "text/html"
#define X_BLOWFISH "x-blowfish"

#define CRLF "\r\n"

#define TIMEOUT_SECS 60

char *__sws_cgidir;
char *__sws_dir;
int __sws_debug = 0;
char *__sws_ip;
char *__sws_logfile;
int __sws_port = 8080;
char *__sws_secdir;
char *__sws_key;

DIR *dir_dp;
DIR *secdir_dp;
int logfile_fd;

/*
 * Like errno, http_status is a global error container.
 * At any point where the status could be != 200 OK,
 * http_status is checked. If it does not start with a
 * '2', the response header indicating the status is
 * created and sent, and the connection is closed.
 */
char *http_status;

void
timeout(int sig) {
	if (sig == SIGALRM)
		//connection timed out, exit
		exit(EXIT_FAILURE);
}

void
sws_init(const struct swsopts opts) {

	struct stat stat_buf;
	ino_t root_ino, cgi_ino;
	int i, slash;
	char *tmpptr, *tmppath;

	__sws_cgidir = opts.cgidir;
	__sws_debug = opts.debug;
	__sws_dir = opts.dir;
	__sws_ip = opts.ip;
	__sws_logfile = opts.logfile;
	__sws_port = opts.port;
	__sws_secdir = opts.secdir;
	__sws_key = opts.key;
	tmppath = NULL;

	while (__sws_dir[strlen(__sws_dir)-1] == '/') {
		fprintf(stderr, "inloop\n");
		__sws_dir[strlen(__sws_dir)-1] = '\0';
	}

	/* Stat http directory */
	if (stat(__sws_dir, &stat_buf) < 0) {
		perror("couldn't stat serve directory");
		exit(EXIT_FAILURE);
		/* NOTREACHED */
	}

	if (S_ISREG(stat_buf.st_mode)) {
		fprintf(stderr, "sws requires directory to serve files\n");
		exit(EXIT_FAILURE);
		/* NOTREACHED */
	}
	root_ino = stat_buf.st_ino;

	/* Stat CGI directory */
	if (__sws_cgidir) {
		if (stat(__sws_cgidir, &stat_buf) < 0) {
			perror("couldn't stat cgi directory");
			exit(EXIT_FAILURE);
			/* NOTREACHED */
		}
	}
	cgi_ino = stat_buf.st_ino;

	if (S_ISREG(stat_buf.st_mode)) {
		fprintf(stderr, "must use directory for -c flag\n");
		exit(EXIT_FAILURE);
		/* NOTREACHED */
	}

	/* Check if CGI dir is in root dir */
	if (__sws_cgidir) {
		if ((dir_dp = opendir(__sws_dir)) == NULL) {
			perror("opendir");
			exit(EXIT_FAILURE);
			/* NOTREACHED */
		}
		if (file_in_dir(dir_dp, root_ino, cgi_ino, __sws_dir, 1) == 0) {
			fprintf(stderr,
				"cgi directory must be inside root\n");
			exit(EXIT_FAILURE);
			/* NOTREACHED */
		}
		if (closedir(dir_dp) < 0) {
			perror("closedir");
			exit(EXIT_FAILURE);
			/* NOTREACHED */
		}
	}

	/* Stat logfile */
	if (__sws_logfile) {
		if (stat(__sws_logfile, &stat_buf) < 0 && errno != ENOENT) {
			perror("couldn't stat logfile");
			exit(EXIT_FAILURE);
			/* NOTREACHED */
		}

		/*
		 * If no such file, check to see if the path leading to
		 * it exists. If it does, create the file in that directory later.
		 */
		if (errno == ENOENT) {
			tmpptr = &__sws_logfile[strlen(__sws_logfile)-1];
			/* If no slashes in path, directory is cwd */
			if (strrchr(__sws_logfile, '/') != NULL) {
				if (*tmpptr == '/')
					tmpptr--;
				for (i = 0; *tmpptr != '/'; tmpptr--, i++)
					;
				if ((tmppath = malloc(strlen(__sws_logfile) - i)) == NULL) {
					fprintf(stderr, "malloc error\n");
					exit(EXIT_FAILURE);
					/* NOTREACHED */
				}
				strncpy(tmppath, __sws_logfile, strlen(__sws_logfile)-i);
				if (stat(tmppath, &stat_buf) < 0) {
					perror("couldn't stat logfile directory");
					exit(EXIT_FAILURE);
					/* NOTREACHED */
				}
				free(tmppath);
				tmppath = NULL;
			}
		}

		/*
		 * If the file exists, check if it is a directory. If it is,
		 * create a default file.
		 */
		else if (S_ISDIR(stat_buf.st_mode)) {
			slash = (__sws_logfile[strlen(__sws_logfile)-1] != '/') ? 1 : 0;

			if ((tmppath = malloc(strlen(__sws_logfile) + strlen(LOGFILE)+slash+1)) == NULL) {
				fprintf(stderr, "malloc error\n");
				exit(EXIT_FAILURE);
				/* NOTREACHED */
			}
			strncpy(tmppath, __sws_logfile, strlen(__sws_logfile));
			if (slash)
				strncat(tmppath, "/", 1);
			strncat(tmppath, LOGFILE, strlen(LOGFILE));
			__sws_logfile = tmppath;
		}
	}

	/* Check if both -s and -k are specified */
	if (__sws_key && !__sws_secdir) {
		fprintf(stderr, "Key specified without secure directory\n");
		exit(EXIT_FAILURE);
		/* NOTREACHED */
	}

	if (!__sws_key && __sws_secdir) {
		fprintf(stderr, "Secure mode specified without key\n");
		exit(EXIT_FAILURE);
		/* NOTREACHED */
	}

	/* Stat secure directory if -s */
	if (__sws_secdir) {
		if (stat(__sws_secdir, &stat_buf) < 0 && errno != ENOENT) {
			perror("couldn't stat secure directory");
			exit(EXIT_FAILURE);
			/* NOTREACHED */
		}
	}

	/* Open logfile if -l */
	if (__sws_logfile) {
		if ((logfile_fd = open(__sws_logfile, O_APPEND | O_CREAT | O_RDWR,
			S_IRUSR | S_IWUSR | S_IRGRP)) < 0) {
			perror("opening logfile");
			exit(EXIT_FAILURE);
			/* NOTREACHED */
		}
	}

	if (tmppath != NULL)
		free(tmppath);
}

void
sws_log(const char *msg) {
	/* write me */
}

void
sws_request(const int sock) {

	socklen_t length;
	struct request request;
	struct response resp;
	struct stat stat_buf;
	struct sockaddr_storage client;
	ino_t cgi_ino, path_ino;
	int port, rval;
	int n;
	char *full_path;
	char ipstr[INET6_ADDRSTRLEN];
	char buf[BUFF_SIZE];

	request.path = NULL;
	request.if_mod_since = NULL;
	resp.length = 0;
	resp.content_type = NULL;

	full_path = NULL;
	bzero(&client, sizeof(struct sockaddr_storage));
	bzero(ipstr, INET6_ADDRSTRLEN);
	length = sizeof(client);

	if (getpeername(sock, (struct sockaddr *)&client, &length) == -1) {
		fprintf(stderr, "Unable to get socket name: %s\n",
				strerror(errno));
		return;
	}

	if (client.ss_family == AF_INET) {
		struct sockaddr_in *s = (struct sockaddr_in *)&client;
		port = ntohs(s->sin_port);
		if (!inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof(ipstr))) {
			fprintf(stderr,"inet_ntop error: %s\n", strerror(errno));
			return;
		}
	} else { // AF_INET6
		struct sockaddr_in6 *s = (struct sockaddr_in6 *)&client;
		port = ntohs(s->sin6_port);
		if (!inet_ntop(AF_INET6, &s->sin6_addr, ipstr, sizeof(ipstr))) {
			fprintf(stderr,"inet_ntop error: %s\n", strerror(errno));
			return;
		}
	}

	fprintf(stderr, "Connection from %s remote port %d\n",
			ipstr, port);

	/* Begin with 200 OK as HTTP status code. */
	http_status = STATUS_200;

	bzero(buf, sizeof(buf));

	signal(SIGALRM, timeout);
	alarm(TIMEOUT_SECS);

	/* Get method/path/protocol  */
	if ((rval = sws_get_line(sock, buf, BUFF_SIZE)) < 0) {
		sws_response_header(sock, &resp);
		return;
	}
	if (rval == 0) {
		//send head response
		/* Close connection */
		return;
	}

	if (sws_parse_method(&request, buf) < 0) {
		//respond with error status
		sws_response_header(sock, &resp);
		return;
	}

	/*
	 * If simple request (HTTP/0.9), skip headers and respond
	 * with entity body only (RFC 1945 Sec. 6, 6.1)
	 */
	if (request.simple) {
		//call file function & respond
		return;
	}

	/* Get headers, read until CRLF */
	while (1) {
		bzero(buf, sizeof(buf));
		if ((rval = sws_get_line(sock, buf, BUFF_SIZE)) < 0) {
			//send error
			return;
		} else if (rval == 0) {
			fprintf(stderr, "Connection closed by client\n");
			return;
		} else if ((strlen(buf) == 2) &&
				(strcmp(buf, CRLF) == 0)) {
			break;
		} else {
			if (sws_parse_header(&request, buf) < 0) {
				//send error
				return;
			}
		}
	}

	if ((n = sws_file_path(__sws_dir, request.path, &full_path)) < 0) {
		if (n == -1) {
			http_status = STATUS_404;
			sws_response_header(sock, &resp);
			return;
		} else {
			http_status = STATUS_500;
			//send error
			return;
		}
	}

	free(request.path);
	request.path = full_path;

	/* Check for CGI request */

	if (__sws_cgidir) {
		/* Get inodes of request path and cgidir */
		if (stat(full_path, &stat_buf) < 0) {
			perror("can't stat request path");
			if (errno == ENOENT || errno == ENOTDIR)
				http_status = STATUS_404;
			else
				http_status = STATUS_500;
			//send error
			return;
		}
		path_ino = stat_buf.st_ino;

		//free(full_path);
		//full_path = NULL;

		if (stat(__sws_cgidir, &stat_buf) < 0) {
			perror("can't stat cgidir");
			http_status = STATUS_500;
			//send error
			return;
		}
		cgi_ino = stat_buf.st_ino;

		if ((dir_dp = opendir(__sws_cgidir)) == NULL) {
			perror("opendir");
			http_status = STATUS_500;
			//send error
			return;
		}

		/* Check if requested file is in cgidir */
		if ((rval = file_in_dir(dir_dp, cgi_ino,
			path_ino, __sws_cgidir, 0)) < 0) {
			//send error
			return;
		}

		if (closedir(dir_dp) < 0) {
			perror("closedir");
			//send error
			return;
		}
	} else
		rval = 0;

	if (rval == 0) {
		//file not in cgidir, serve it normally
		if (sws_serve_file(sock, &request) < 0) {
			//send error
			sws_response_header(sock, &resp);
			return;
		}
	} else {
		//file in cgidir, execute it
		fprintf(stderr, "cgi request\n");
		sws_execute_cgi(sock, &request);
	}
	if (full_path != NULL)
		free(full_path);

	/* Free memory allocated in struct request */
	//if (request.path != NULL)
	//	free(request.path);
	if (request.if_mod_since != NULL)
		free(request.if_mod_since);
}

/*
 * Function to retrieve a single line from a socket. Reads characters one
 * by one into the passed buffer. Returns the number of characters read
 * (not including null terminator) or -1 on error.
 */
int
sws_get_line(int sock, char *buf, int len) {

	int i, n;
	char c;

	i = 0;
	for (; i < len - 1;) {
		/* Read a byte from socket */
		if ((n = recv(sock, &c, 1, 0)) < 0) {
			perror("recv");
			http_status = STATUS_500;
			return -1;
		}
		if (n > 0) {
			if (c == '\r') {
				buf[i] = c;
				i++;
				/* If a carriage return, check for newline */
				if ((n = recv(sock, &c, 1, MSG_PEEK)) < 0) {
					perror("recv");
					http_status = STATUS_500;
					return -1;
				}
				/* If char is a newline, read it */
				if ((n > 0) && (c == '\n')) {
					if (recv(sock, &c, 1, 0) < 0) {
						perror("recv");
						http_status = STATUS_500;
						return -1;
					}
				} else {
				/* If not, return bad request */
					http_status = STATUS_400;
					return -1;
				}
			}
			buf[i] = c;
			i++;
			if(c == '\n') {
				break;
			}
		}
		else
			break;
	}
	buf[i] = '\0';

	return i;
}

int
sws_parse_method(struct request *req, char *buf) {

	int i;
	char *tmp;

	/* Check method */
	if (strncmp("GET", buf, 3) == 0) {
		i = 3;
		req->method = 0;
	} else if (strncmp("HEAD", buf, 4) == 0) {
		i = 4;
		req->method = 1;
	} else if (strncmp("POST", buf, 4) == 0) {
		i = 4;
		req->method = 2;
	} else {
		http_status = STATUS_501;
		return -1;
	}

	/* Push buf pointer past method */
	for (;i > 0; i--, buf++);

	/* Skip whitespace */
	for (;*buf == ' '; buf++);

	/* Parse path */
	if (*buf != '/') {
		http_status = STATUS_400;
		return -1;
	}

	/*
	 * Get the length of the path requested. Right now all we care
	 * about is loading the path string into the request object --
	 * we'll deal with stat-ing the file later.
	 */
	for (i = 0; i < strlen(buf); i++)
		if (buf[i] == ' ' || buf[i] == '\r')
			break;

	if ((tmp = malloc(i+1)) == NULL) {
		fprintf(stderr, "malloc error\n");
		http_status = STATUS_500;
		return -1;
	}

	strncpy(tmp, buf, i);
	tmp[i] = '\0';
	req->path = tmp;

	for(;i > 0; i--)
		buf++;

	/* Skip more whitespace */
	for (;*buf == ' '; buf++);

	/* Get protocol */
	if ((strncmp(buf, CRLF, 2) == 0) && req->method == 0) {
		/*HTTP 0.9 Simple-Request */
		req->simple = 1;
	} else if (strncmp(buf, "HTTP/1.1", 8) == 0) {
		/* HTTP 1.0 Full-Request */
		req->simple = 0;
	} else {
		/* Invalid request */
		http_status = STATUS_400;
		return -1;
	}

	return 0;
}

int
sws_parse_header(struct request *req, char *buf) {

	struct tm tm;
	int i;
	char *tmp;

	if (strchr(buf, ':') == NULL) {
		http_status = STATUS_400;
		return -1;
	}

	/* Trim leading whitespace */
	for (;*buf == ' '; buf++);

	/* Get length of header name */
	for (i = 0; buf[i] != ':'; i++);

	/* Only important header is If-Modified-Since */
	if (strncmp(buf, "If-Modified-Since", i) == 0) {

		/* Iterate over header name */
		for (; i > 0; i--, buf++);

		/*
		 * Next 2 characters must be ':' and SP.
		 * (RFC 1945, Sec 4.2)
		 */
		if (*buf != ':' && (*(buf+1) != ' ')) {
			http_status = STATUS_400;
			return -1;
		}
		buf += 2;

		/* Retrieve field value */
		if ((tmp = malloc(strlen(buf))) == NULL) {
			fprintf(stderr, "malloc error\n");
			http_status = STATUS_500;
			return -1;
		}
		strncpy(tmp, buf, strlen(buf));

		/* Trim whitespace and CRLF from end */
		for (i = strlen(tmp)-3; tmp[i] == ' '; i--);
		tmp[i+1] = '\0';
		req->if_mod_since = tmp;

		/* Get date format, if valid */
		if (strptime(tmp, RFC1123_DATE, &tm) != NULL)
			req->date_format = RFC1123_DATE;
		else if (strptime(tmp, RFC850_DATE, &tm) != NULL)
			req->date_format = RFC850_DATE;
		else if (strptime(tmp, ASCTIME_DATE, &tm) != NULL)
			req->date_format = ASCTIME_DATE;
		else
			req->date_format = NULL;
	}

	return 0;
}

int
sws_serve_file(int sock, struct request* req) {

	struct response resp;
	struct stat stat_buf;
	int fd, n;
	char *full_path;
	char last_mod[50];
	char buf[BUFF_SIZE];

	/*
	 * Check if requested path is inside root/user dir -- ideally
	 * chroot would be here but it is not possible on linuxlab.
	 */
	if ((n = file_in_root(req->path)) == 0) {
		http_status = STATUS_403;
		return -1;
	} else if (n == -1) {
		http_status = STATUS_500;
		return -1;
	}
	full_path = req->path;

	/* Stat the file */
	if (stat(full_path, &stat_buf) < 0) {
		if (errno == EACCES)
			http_status = STATUS_403;
		else if (errno == ENOENT || errno == ENOTDIR)
			http_status = STATUS_404;
		else
			http_status = STATUS_500;
		perror("stat");
		return -1;
	}

	if (S_ISDIR(stat_buf.st_mode)) {
		sws_create_index(sock, &resp, full_path);
	} else {
		/* Open file */
		if ((fd = open(full_path, O_RDONLY)) < 0) {
			http_status = STATUS_500;
			perror("open");
			return -1;
		}

		/* Get file size */
		if ((n = lseek(fd, 0, SEEK_END)) < 0) {
			http_status = STATUS_500;
			perror("lseek");
			return -1;
		}

		/* Reset offset */
		if (lseek(fd, 0, SEEK_SET) < 0) {
			http_status = STATUS_500;
			perror("lseek");
			return -1;
		}

		strftime(last_mod, 50, RFC1123_DATE, gmtime(&stat_buf.st_mtime));

		resp.length = n;
		resp.content_type = TEXT_HTML;
		resp.last_modified = last_mod;
		sws_response_header(sock, &resp);

		while ((n = read(fd, buf, BUFF_SIZE)) > 0) {
			if (send(sock, buf, strlen(buf), 0) < 0) {
				perror("send");
				exit(EXIT_FAILURE);
				/* NOTREACHED */
			}
		}

		if (n < 0) {
			perror("read");
			exit(EXIT_FAILURE);
			/* NOTREACHED */
		}
	}

	return 0;
}

int
sws_response_header(int sock, struct response *resp) {

	time_t now;
	char buf[1024];
	char timestr[50];
	//char lastmodtime[50];

	now = time(NULL);
	strftime(timestr, sizeof(timestr),
		"%a, %e %b %Y %T %Z", gmtime(&now));

	if (strcmp(http_status, STATUS_200) == 0) {
		sprintf(buf, "HTTP/1.0 %s\r\n"
			"Date: %s\r\n"
			"Server: SWS\r\n"
			"Last-Modified: %s\r\n"
			"Content-Type: text/html\r\n"
			"Content-Length: %lu\r\n"
			"\r\n", http_status, timestr, resp->last_modified, resp->length);
	} else {
		sprintf(buf, "HTTP/1.0 %s\r\n"
			"Date: %s\r\n"
			"Server: SWS\r\n"
			"\r\n", http_status, timestr);
	}

	fprintf(stderr, "%s\n", buf);
	if (send(sock, buf, strlen(buf), 0) < 0) {
		perror("send");
		return -1;
	}

	return 0;
}

int
sws_execute_cgi(int sock, struct request *req) {

	struct response resp;
	struct stat stat_buf;
	pid_t pid;
	int n;
	int pipe_fd[2];
	int status;
	char *tmp;
	char buf[BUFF_SIZE];
	char last_mod[50];

	tmp = req->path;
	tmp += strlen(__sws_dir);
	while(*tmp == '/') tmp++;
	//tmp++; //remove leading '/'
	//file already validated

	//stat for mtime
	stat(tmp, &stat_buf);

	if (pipe(pipe_fd) < 0) {
		perror("pipe");
		http_status = STATUS_500;
		return -1;
	}

	if ((pid = fork()) < 0) {
		perror("forking for cgi");
		http_status = STATUS_500;
		return -1;
	}

	if (pid == 0) { //child

		//char query[BUFF_SIZE];

		close(pipe_fd[0]);

		/* Redirect CGI output to pipe */
		dup2(pipe_fd[1], STDOUT_FILENO);

		fprintf(stderr, "%s\n", tmp);

		/* Execute CGI script */
		if (execl(tmp, tmp, NULL) < 0) {
			perror("execl");
			close(pipe_fd[1]);
			exit(EXIT_FAILURE);
			/* NOTREACHED */
		}
		exit(EXIT_SUCCESS);
	} else { //parent
		bzero(buf, sizeof(buf));

		if ((n = read(pipe_fd[0], buf, sizeof(buf))) < 0) {
			perror("read");
			return -1;
		}

		resp.content_type = TEXT_HTML;
		resp.length = n;
		strftime(last_mod, sizeof(last_mod), RFC1123_DATE, gmtime(&stat_buf.st_mtime));
		resp.last_modified = last_mod;
		sws_response_header(sock, &resp);
		close(pipe_fd[1]);

		if (send(sock, buf, strlen(buf), 0) < 0) {
			perror("send");
			exit(EXIT_FAILURE);
			/* NOTREACHED */
		}

		waitpid(pid, &status, 0);
	}

	return 0;
}

int
sws_create_index(int sock, struct response *resp, char *path) {

	DIR *dp;
	struct dirent **dirlist;
	struct stat stat_buf;
	int i, n;
	int username_len;
	int home_path_len;
	int homedir;
	char buf[PATH_MAX];
	char *tmp;
	char* username;
	char index[BUFF_SIZE];

	bzero(index, sizeof(index));

	if ((dp = opendir(path)) == NULL) {
		http_status = STATUS_500;
		perror("opendir");
		return -1;
	}
	tmp = malloc(PATH_MAX);
	bzero(tmp, sizeof(tmp));

	//folder has trailing slash by default

	//fprintf(stderr, "path:%s\n", path);
	homedir = 0;
	home_path_len = 0;
	if (strncmp(path, "/home/", 6) == 0) {
		//black magic
		fprintf(stderr, "homedir\n");
		homedir = 1;
		username = path + 6;
		//get username length
		for (i = 0; username[i] != '/'; i++);
		username_len = i;
		home_path_len = strlen("/home/")+username_len+strlen("/sws");
		fprintf(stderr, "usernamelen: %d\n", username_len);
		fprintf(stderr, "homepathlen: %d\n", home_path_len);
	}

	if (homedir) {
		strncpy(tmp, path + home_path_len, strlen(path+home_path_len) + 1);
	} else
		strncpy(tmp, path + strlen(__sws_dir), strlen(path + strlen(__sws_dir))+1);

	fprintf(stderr, "tmp: %s\n", tmp);

	strncpy(index, "<html><body><h1>Index of ", 25);
	if (homedir) {
		strncat(index, "~", 1);
		strncat(index, username, username_len);
	}
	strncat(index, tmp, strlen(tmp));
	strncat(index, "</h1></br />", 12);

	if ((n = scandir(path, &dirlist, NULL, alphasort)) < 0) {
		perror("scandir");
		return -1;
	}

	int slashpos;
	slashpos = 0;
	fprintf(stderr, "%s\n", path);
	for (i = 0; i < n; i++) {
		bzero(buf, sizeof(buf));
		strncpy(buf, path, strlen(path));
		strncat(buf, dirlist[i]->d_name, strlen(dirlist[i]->d_name));
		stat(buf, &stat_buf);
		//if file is not . or ..
		if (strcmp(dirlist[i]->d_name, ".") != 0 &&
			strcmp(dirlist[i]->d_name, "..") != 0) {
			strncat(index, "<a href=\"", 9);
			if (homedir) {
				strncat(index, "/~", 6);
				strncat(index, username, username_len);
				strncat(index, tmp, strlen(tmp));
				strncat(index, dirlist[i]->d_name + home_path_len,
					strlen(dirlist[i]->d_name + home_path_len));
				strncat(index, dirlist[i]->d_name, strlen(dirlist[i]->d_name));
			} else {
				strncat(index, tmp, strlen(tmp));
				strncat(index, dirlist[i]->d_name, strlen(dirlist[i]->d_name));
			}
			if (S_ISDIR(stat_buf.st_mode))
				strncat(index, "/", 1);
		//if it is ..
		} else if (strcmp(dirlist[i]->d_name, "..") == 0) {
			strncat(index, "<a href=\"", 9);
			slashpos = strrchr_pos(tmp+1, '/', strlen(tmp+1)-1);
			fprintf(stderr, "tmp+1: %s\n", tmp+1);
			fprintf(stderr, "%d\n", slashpos+1);
			if (slashpos == -1)
				slashpos = 0;
			if (homedir) {
				strncat(index, "/~", 6);
				strncat(index, username, username_len);
			}
			strncat(index, tmp, slashpos+1);
		//if it is .
		} else {
			strncat(index, "<a href=\"", 9);
			if (homedir) {
				strncat(index, "/~", 6);
				strncat(index, username, username_len);
				strncat(index, tmp, strlen(tmp));
			} else
	                        strncat(index, tmp, strlen(tmp));
		}
		strncat(index, "\">", 2);
		strncat(index, dirlist[i]->d_name, strlen(dirlist[i]->d_name));
		if (S_ISDIR(stat_buf.st_mode))
			strncat(index, "/", 1);
		strncat(index, "</a><br />", 10);
		free(dirlist[i]);
	}
	free(dirlist);
	free(tmp);
	tmp = NULL;

	strncat(index, "</body></html>", 14);
	resp->length = strlen(index);
	resp->content_type = TEXT_HTML;
	sws_response_header(sock, resp);

	fprintf(stderr, "%s\n", index);

	if (send(sock, index, strlen(index), 0) < 0) {
		perror("send");
		exit(EXIT_FAILURE);
		/* NOTREACHED */
	}

	return 0;
}
