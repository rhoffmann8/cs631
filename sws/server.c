#define _XOPEN_SOURCE 1000
#define _BSD_SOURCE

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <errno.h>
#include <dirent.h>
#include <features.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "content_type.h"
#include "defines.h"
#include "files.h"
#include "list.h"
#include "log.h"
#include "parse.h"
#include "request.h"
#include "response.h"
#include "server.h"

char *__sws_cgidir;
char *__sws_dir;
int __sws_debug = 0;
char *__sws_ip;
char *__sws_logfile;
int __sws_port = 8080;
char *__sws_secdir;
char *__sws_key;

int logfile_fd;

char *http_status;
struct list *ctypes;

void
sws_cleanup(int sig) {

	fprintf(stderr, "Exiting...\n");

	free_content_types(ctypes);

	if (__sws_dir)
		free(__sws_dir);
	if (__sws_cgidir)
		free(__sws_cgidir);
	if (__sws_logfile)
		free(__sws_logfile);
	if (__sws_secdir)
		free(__sws_secdir);
	close(logfile_fd);

	exit(EXIT_SUCCESS);
	/* NOTREACHED */
}

void
sws_init(const struct swsopts opts) {

	struct stat stat_buf;
	struct sigaction sig;

	__sws_cgidir = opts.cgidir;
	__sws_debug = opts.debug;
	__sws_dir = opts.dir;
	__sws_ip = opts.ip;
	__sws_logfile = opts.logfile;
	__sws_port = opts.port;
	__sws_secdir = opts.secdir;
	__sws_key = opts.key;

	if ((__sws_dir = realpath(__sws_dir, NULL)) == NULL) {
		perror("realpath");
		exit(EXIT_FAILURE);
		/* NOTREACHED */
	}

	if (__sws_cgidir) {
		if ((__sws_cgidir = realpath(__sws_cgidir, NULL)) == NULL) {
			perror("realpath");
			exit(EXIT_FAILURE);
		}
	}

	if (__sws_secdir) {
		if ((__sws_secdir = realpath(__sws_secdir, NULL)) == NULL) {
			perror("realpath");
			exit(EXIT_FAILURE);
			/* NOTREACHED */
		}
	}

	if (__sws_logfile) {
		if ((__sws_logfile = realpath(__sws_logfile, NULL)) == NULL) {
			perror("realpath");
			exit(EXIT_FAILURE);
			/* NOTREACHED */
		}
	}

	if (stat(__sws_dir, &stat_buf) < 0 || !S_ISDIR(stat_buf.st_mode)) {
		if (errno)
			perror("stat");
		else
			fprintf(stderr, "serve path must be dir\n");
		exit(EXIT_FAILURE);
		/* NOTREACHED */
	}

	if (__sws_cgidir) {
		if (stat(__sws_cgidir, &stat_buf) < 0 || !S_ISDIR(stat_buf.st_mode)) {
			if (errno)
				perror("stat");
			else
				fprintf(stderr, "-c option must be dir\n");
			exit(EXIT_FAILURE);
			/* NOTREACHED */
		}

		if (strncmp(__sws_dir, __sws_cgidir, strlen(__sws_dir)) != 0) {
			fprintf(stderr, "cgi dir must be inside serve root\n");
			exit(EXIT_FAILURE);
			/* NOTREACHED */
		}
	}

	if (__sws_logfile && !__sws_debug) {
		if ((logfile_fd = init_logfile(__sws_logfile)) < 0) {
			exit(EXIT_FAILURE);
			/* NOTREACHED */
		}
	}

	if (__sws_key && !__sws_secdir) {
		fprintf(stderr, "key specified without secure dir\n");
		exit(EXIT_FAILURE);
		/* NOTREACHED */
	}

	if (!__sws_key && __sws_secdir) {
		fprintf(stderr, "secure dir specified without key\n");
		exit(EXIT_FAILURE);
		/* NOTREACHED */
	}

	if (__sws_secdir) {
		if (stat(__sws_secdir, &stat_buf) < 0 || !S_ISDIR(stat_buf.st_mode)) {
			if (errno)
				perror("couldn't stat secure dir");
			else
				fprintf(stderr, "-s option must be dir\n");
			exit(EXIT_FAILURE);
			/* NOTREACHED */
		}

		if (strncmp(__sws_dir, __sws_secdir, strlen(__sws_dir)) != 0) {
			fprintf(stderr, "secure dir must be inside serve root\n");
			exit(EXIT_FAILURE);
			/* NOTREACHED */
		}
	}

	ctypes = create_list();

	if (load_content_types(ctypes) < 0) {
		exit(EXIT_FAILURE);
		/* NOTREACHED */
	}

	sig.sa_handler = sws_cleanup;
	sigemptyset(&sig.sa_mask);

	if ((sigaction(SIGINT, &sig, NULL) < 0)
		|| (sigaction(SIGQUIT, &sig, NULL) < 0)
		|| (sigaction(SIGHUP, &sig, NULL) < 0)
		|| (sigaction(SIGTERM, &sig, NULL) < 0)) {
		perror("sigaction");
		exit(EXIT_FAILURE);
		/* NOTREACHED */
	}
}

void
sws_handle_request(int sock) {

	DIR *dp;
	struct dirent *dir;
	socklen_t client_len;
	struct request *req;
	struct response *resp;
	struct sockaddr_storage client;
	struct stat stat_buf;
	int port, rval;
	char ipstr[INET6_ADDRSTRLEN];
	char buf[BUFF_SIZE];

	if ((req = create_request()) == NULL)
		return;
	if ((resp = create_response()) == NULL)
		return;

	bzero(&client, sizeof(struct sockaddr_storage));
	bzero(ipstr, INET6_ADDRSTRLEN);
	bzero(buf, sizeof(buf));
	client_len = sizeof(client);

	if (getpeername(sock, (struct sockaddr*)&client, &client_len) == -1) {
		fprintf(stderr, "Unable to get socket name: %s\n",
			strerror(errno));
		return;
	}

	if (client.ss_family == AF_INET) {
		struct sockaddr_in *s = (struct sockaddr_in *)&client;
		port = ntohs(s->sin_port);
		if (!inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof(ipstr))) {
			perror("inet_ntop");
			return;
		}
	} else {
		struct sockaddr_in6 *s = (struct sockaddr_in6*)&client;
		port = ntohs(s->sin6_port);
		if (!inet_ntop(AF_INET6, &s->sin6_addr, ipstr, sizeof(ipstr))) {
			perror("inet_ntop");
			return;
		}
	}

	fprintf(stderr, "Connection from %s to remote port %d\n",
		ipstr, port);

	http_status = STATUS_200;

	if ((req->ip = calloc(1, strlen(ipstr))) == NULL) {
		fprintf(stderr, "calloc error\n");
		http_status = STATUS_500;
		sws_response_headers(sock, req, resp);
	}
	strcpy(req->ip, ipstr);

	if ((rval = sws_recv_line(sock, buf, BUFF_SIZE)) < 0) {
		sws_response_headers(sock, req, resp);
		return;
	}

	if ((req->method_line = calloc(1, strlen(buf)+1)) == NULL) {
		fprintf(stderr, "calloc error\n");
		http_status = STATUS_500;
		sws_response_headers(sock, req, resp);
		return;
	}
	strncpy(req->method_line, buf, strlen(buf));

	//parse method
	if (sws_parse_method(req, buf, __sws_dir) < 0) {
		sws_response_headers(sock, req, resp);
		return;
	}

	//parse headers
	while(1) {
		memset(buf, 0, sizeof(buf));
		//bzero(buf, sizeof(buf));
		if (req->simple)
			break;
		if ((rval = sws_recv_line(sock, buf, BUFF_SIZE)) < 0) {
			sws_response_headers(sock, req, resp);
			return;
		} else if (rval == 0) {
			fprintf(stderr, "Connection closed by client\n");
			return;
		} else if ((strlen(buf) == 2) &&
				(strcmp(buf, CRLF) == 0)) {
			break;
		} else {
			if (sws_parse_header(req, buf) < 0) {
				sws_response_headers(sock, req, resp);
				return;
			}
		}

	}
	rval = 0;

	if (stat(req->realpath, &stat_buf) < 0) {
		perror("stat");
		if (errno == EACCES)
			http_status = STATUS_403;
		else if (errno == ENOENT || errno == ENOTDIR)
			http_status = STATUS_404;
		else
			http_status = STATUS_500;
		sws_response_headers(sock, req, resp);
		return;
	}

	//sws_verify_file(path);

	//TODO: check +x on path parts

	/*if ((stat_buf.st_mode & S_IXOTH) != S_IXOTH) {
		http_status = STATUS_403;
		sws_response_headers(sock, req, resp);
		return;
	}*/

	if (S_ISDIR(stat_buf.st_mode)) {
		int index = 1;
		char *index_path;
		/* Check for index.html */
		if ((dp = opendir(req->realpath)) == NULL) {
			perror("opendir");
			http_status = STATUS_500;
			return;
		}

		while ((dir = readdir(dp)) != NULL) {
			if (strcmp(dir->d_name, "index.html") == 0) {
				if ((index_path =
					malloc(strlen(req->realpath)+strlen("index.html")+1)) == NULL) {
					fprintf(stderr, "malloc error\n");
					http_status = STATUS_500;
					sws_response_headers(sock, req, resp);
					return;
				}
				index = 0;
				sprintf(index_path, "%s/index.html", req->realpath);
				free(req->realpath);
				req->realpath = index_path;
				sws_serve_file(sock, req, resp);
				break;
			}
		}

		if (index)
			sws_create_index(sock, req, resp, __sws_dir);
	} else {
		if (__sws_cgidir &&
			strncmp(req->realpath, __sws_cgidir, strlen(__sws_cgidir)) == 0)
			sws_execute_cgi(sock, req, resp);
		else
			sws_serve_file(sock, req, resp);
	}

	printf("destroying req\n");
	destroy_request(req);
	printf("destroying resp\n");
	destroy_response(resp);
}

int
sws_recv_line(int sock, char *buf, int len) {

	int i, n;
	char c;

	i = 0;
	for (; i < len - 1;) {
		if ((n = recv(sock, &c, 1, 0)) < 0) {
			perror("recv");
			http_status = STATUS_500;
			return -1;
		}
		if (n > 0) {
			if (c == '\r') {
				buf[i] = c;
				i++;
				if ((n = recv(sock, &c, 1, MSG_PEEK)) < 0) {
					perror("recv");
					http_status = STATUS_500;
					return -1;
				}
				if ((n > 0) && (c == '\n')) {
					if (recv(sock, &c, 1, 0) < 0) {
						perror("recv");
						http_status = STATUS_500;
						return -1;
					}
				} else {
					http_status = STATUS_400;
					return -1;
				}
			}
			buf[i] = c;
			i++;
			if (c == '\n')
				break;
		} else {
			break;
		}
	}
	buf[i] = '\0';

	return i;
}

int
sws_response_headers(int sock, struct request *req, struct response *resp) {

	time_t now;
	char buf[BUFF_SIZE];
	char timestr[64];
	char html_msg[BUFF_SIZE];
	char len[32];

	bzero(html_msg, sizeof(html_msg));
	now = time(NULL);
	strftime(timestr, sizeof(timestr), RFC1123_DATE, gmtime(&now));

	if (strcmp(http_status, STATUS_200) == 0 ||
		strcmp(http_status, STATUS_304) == 0) {
		sprintf(buf, "HTTP/%s %s\r\n"
			"Date: %s\r\n"
			"Server: SWS\r\n"
			"%s%s%s"
			"Content-Type: %s\r\n",
			//"%s%lu%s"
			//"Content-Length: %lu\r\n"
			//"\r\n",
			(req->simple == 1)? "0.9" : "1.0",
			http_status, timestr,
			(resp->last_modified != NULL)? "Last-Modified: " : "",
			(resp->last_modified != NULL)? resp->last_modified : "",
			(resp->last_modified != NULL)? "\r\n" : "",
			resp->mime_type);
			/*(strcmp(http_status, STATUS_200) == 0)? "Content-Length: " : "",
			(strcmp(http_status, STATUS_200) == 0)? resp->length : atoi(""),
			(strcmp(http_status, STATUS_200) == 0)? "\r\n" : "");*/
			if (strcmp(http_status, STATUS_200) == 0) {
				sprintf(len, "Content-Length: %lu\r\n\r\n", resp->length);
				strncat(buf, len, strlen(len));
			} else {
				strncat(buf, "\r\n", strlen("\r\n"));
			}
	} else {
		sprintf(html_msg, "<html><h1>%s</h1></html>", http_status);
		sprintf(buf, "HTTP/%s %s\r\n"
			"Date: %s\r\n"
			"Server: SWS\r\n"
			"Content-Type: text/html\r\n"
			"Content-Length: %lu\r\n"
			"\r\n", (req->simple == 1)? "0.9" : "1.0",
			http_status, timestr,
			(unsigned long)strlen(html_msg));
		resp->length = (unsigned long)strlen(html_msg);
	}

	if (__sws_logfile)
		sws_log(logfile_fd, req, resp, __sws_debug);

	if (send(sock, buf, strlen(buf), 0) < 0) {
		perror("send");
		return -1;
	}

	if (strlen(html_msg) > 0) {
		if (send(sock, html_msg, strlen(html_msg), 0) < 0) {
			perror("send");
			return -1;
		}
	}

	return 0;
}
