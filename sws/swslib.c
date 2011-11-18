#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "sws.h"

#define LOGFILE "sws.log"
#define BUFF_SIZE 8096

#define STATUS_200 "200 OK"
#define STATUS_201 "201 Created"
#define STATUS_202 "202 Accepted"
#define STATUS_204 "204 No Content"
#define STATUS_301 "301 Moved Permanently"
#define STATUS_302 "302 Moved Temporarily"
#define STATUS_304 "304 Not Modified"
#define STATUS_400 "400 Bad Request"
#define STATUS_401 "401 Unauthorized"
#define STATUS_403 "403 Forbidden"
#define STATUS_404 "404 Not Found"
#define STATUS_500 "500 Internal Server Error"
#define STATUS_501 "501 Not Implemented"
#define STATUS_502 "502 Bad Gateway"
#define STATUS_503 "503 Service Unavailable"

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

void
sws_init(const struct swsopts opts) {

	struct stat stat_buf;
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

	/* Open file descriptors */
	if ((dir_dp = opendir(__sws_dir)) == NULL) {
		perror("opening serve directory");
		exit(EXIT_FAILURE);
		/* NOTREACHED */
	}

	if (__sws_logfile) {
		if ((logfile_fd = open(__sws_logfile, O_APPEND | O_CREAT | O_RDWR,
			S_IRUSR | S_IWUSR | S_IRGRP)) < 0) {
			perror("opening logfile");
			exit(EXIT_FAILURE);
			/* NOTREACHED */
		}
	}

	if (__sws_secdir && __sws_key) {
		if ((secdir_dp = opendir(__sws_secdir)) == NULL) {
			perror("opening secure directory");
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
	time_t now;
	struct sockaddr_storage client;
	char ipstr[INET6_ADDRSTRLEN];
	char buf[BUFF_SIZE];
	char timestr[50];
	char *status;
	int port, rval;

	bzero(&client, sizeof(struct sockaddr_storage));
	bzero(ipstr, INET6_ADDRSTRLEN);
	length = sizeof(client);

	if (__sws_debug) {
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

		/* Read from socket */
		if ((rval = recv(sock, buf, BUFF_SIZE, 0)) < 0) {
			perror("recv");
			return;
		}
		else if (rval == 0) {
			fprintf(stderr, "failure reading browser request\n");
			return;
		}
		else if (rval > 0 && rval < BUFF_SIZE) {
			buf[rval] = '\0';
		}


		now = time(NULL);

		strftime(timestr, sizeof(timestr),
			"%a, %e %b %Y %T %Z", gmtime(&now));

		if (!strcmp((status = sws_process_request(buf)), STATUS_200)) {
			sprintf(buf, "HTTP/1.0 %s\r\n"
				     "Date: %s\r\n"
				     "Server: SWS\r\n"
				     "Content-Type: text/html\r\n"
				     "\r\n", status, timestr);

			write(sock, buf, strlen(buf));
		} else {
			sprintf(buf, "HTTP/1.0 %s\r\n"
				     "\r\n", status);
			write(sock, buf, strlen(buf));
		}
	}
}

/* change to get_status_code? */
char*
sws_process_request(const char *request) {

	int path_len;// request_type;
	if (!strncmp(request, "GET ", 4)) {
		//request_type = 1;
		request += 4;
	/*} else if (!strncmp(request, "HEAD", 4)) {
		request_type = 2;
		request += 5;
	} else if (!strncmp(request, "POST", 4)) {
		request_type = 3;
		request += 5;
	*/
	} else {
		return STATUS_400;
	}

	for (path_len = 0; request[path_len] != ' '; path_len++)
		if (request[path_len] == '\r')
			return STATUS_400;

	if (path_len == 1) {
		//Root was requested

	} else {
		//path is request[0] to request[path_len]
	}
	return STATUS_200;
}
