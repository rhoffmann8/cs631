#define _XOPEN_SOURCE
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <features.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
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

#define RFC1123_DATE "%a, %d %b %Y %T GMT"
#define RFC850_DATE "%A, %d-%b-%y %T GMT"
#define ASCTIME_DATE "%a %b %e %T %Y"

#define CRLF "\r\n"

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

	/* Open file descriptors (why?) */
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
	struct request request;
	struct sockaddr_storage client;
	char ipstr[INET6_ADDRSTRLEN];
	char buf[BUFF_SIZE];
	char timestr[50];
	int port, rval;

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

	/* Get method/path/protocol  */
	if ((rval = sws_get_line(sock, buf, BUFF_SIZE)) < 0) {
		//construct & send error status
		return;
	}

	if (sws_parse_method(&request, buf) < 0) {
		//respond with error status
	}

	/*
	 * If simple request (HTTP/0.9), skip headers and respond
	 * with entity body only (RFC 1945 Sec. 6, 6.1)
	 */
	if (request.simple) {
		//call file function & respond
		return;
	}

	fprintf(stderr, "parsing headers\n");

	/* Get headers, read until CRLF */
	//TODO: SIGALRM
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

	/* Request has been validated, attempt to serve file */
	if (sws_serve_file(&request) < 0) {
		//send error
		return;
	}

	now = time(NULL);
	strftime(timestr, sizeof(timestr),
		"%a, %e %b %Y %T %Z", gmtime(&now));

	sprintf(buf, "HTTP/1.0 %s\r\n"
		     "Date: %s\r\n"
		     "Server: SWS\r\n"
		     "Content-Type: text/html\r\n"
		     "\r\n", http_status, timestr);

	write(sock, buf, strlen(buf));

	/* Free memory allocated in struct request */
	if (request.path != NULL)
		free(request.path);
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
	} else if (strncmp(buf, "HTTP/1.0", 8) == 0) {
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
sws_serve_file(struct request* req) {

	struct stat buf;
	int i, len;
	char *abs_path, *tmp;

	/*
	 * Check if requested path is inside root/user dir -- ideally
	 * chroot would be here but it is not possible on linuxlab.
	 */
	tmp = req->path;

	i = 0;
	/* If request is a user's dir, skip the first slash */
	if (tmp[1] == '~') {
		tmp++;

		/*
		 * If the user's directory root was requested without
		 * a trailing slash, increment i so it will pass
		 * the verification later.
		 */
		if (strchr(tmp, '/') == NULL)
			i++;
	}

	while (*tmp != '\0') {

		/*
		 * If we find a slash (that is not part of
		 * multiple slashes), increment i
		 */
		if ((*tmp == '/') && (*(tmp-1) != '/')) {
			i++;
			tmp++;
		}
		/*
		 * If we find a parent dir traversal sequence,
		 * decrement i
		 */
		else if (strncmp(tmp, "../", 3) == 0) {
			i--;
			tmp += 3;
		}
		/* For all other characters, do nothing */
		else {
			tmp++;
		}
	}

	/* If i is not >= 1, we are outside the root */
	if (i < 1) {
		http_status = STATUS_403;
		return -1;
	}

	tmp = req->path;
	/* If user's web directory, get the length of user string */
	if (tmp[1] == '~') {
		tmp += 2;
		if (strchr(tmp, '/') != NULL)
			for (i = 0; *tmp != '/'; i++, tmp++);
		else
			i = strlen(tmp);
		len = strlen("/home/") + i + strlen("/sws");
	} else {
		len = strlen(__sws_dir);
	}

	/* Construct absolute path to requested file/directory */
	if ((abs_path = malloc(len +
		(strlen(req->path) - i - 2) + 1)) == NULL) {
		fprintf(stderr, "malloc error\n");
		http_status = STATUS_500;
		return -1;
	}

	if (req->path[1] == '~') {
		tmp = req->path + 2;
		strncpy(abs_path, "/home/", 6);
		strncat(abs_path, tmp, i);
		for (; i > 0; tmp++, i--);
		strncat(abs_path, "/sws", 4);
		strncat(abs_path, tmp, strlen(tmp));
	} else {
		strncpy(abs_path, __sws_dir, strlen(__sws_dir));
		strncat(abs_path, req->path, strlen(req->path));
	}

	fprintf(stderr, "%s\n", abs_path);

	/* Stat the file */
	if (stat(abs_path, &buf) < 0) {
		if (errno == EACCES)
			http_status = STATUS_403;
		else if (errno == ENOENT || errno == ENOTDIR)
			http_status = STATUS_404;
		else
			http_status = STATUS_501;
		perror("stat");
		return -1;
	}

	if (abs_path != NULL)
		free(abs_path);

	return 0;
}
