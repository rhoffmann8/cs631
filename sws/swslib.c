#define _XOPEN_SOURCE 800
#define _BSD_SOURCE

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

/*
 * Function to initialize server properties
 */
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

	/* Trim '/' characters */
	while (__sws_dir[strlen(__sws_dir)-1] == '/') {
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
		 * it exists. If it does, create the file in that directory
		 * later.
		 */
		if (errno == ENOENT) {
			tmpptr = &__sws_logfile[strlen(__sws_logfile)-1];
			/* If no slashes in path, directory is cwd */
			if (strrchr(__sws_logfile, '/') != NULL) {
				if (*tmpptr == '/')
					tmpptr--;
				for (i = 0; *tmpptr != '/'; tmpptr--, i++)
					;
				if ((tmppath = 	malloc(strlen(__sws_logfile)
					- i)) == NULL) {
					fprintf(stderr, "malloc error\n");
					exit(EXIT_FAILURE);
					/* NOTREACHED */
				}
				strncpy(tmppath, __sws_logfile,
					strlen(__sws_logfile)-i);
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

			if ((tmppath = malloc(strlen(__sws_logfile)
				+ strlen(LOGFILE)+slash+1)) == NULL) {
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

	/* Open logfile if -l (and not -d) */
	if (__sws_logfile && !__sws_debug) {
		if ((logfile_fd = open(__sws_logfile,
			O_APPEND | O_CREAT | O_RDWR,
			S_IRUSR | S_IWUSR | S_IRGRP)) < 0) {
			perror("opening logfile");
			exit(EXIT_FAILURE);
			/* NOTREACHED */
		}
	}

	if (tmppath != NULL)
		free(tmppath);
}

/*
 * Function to log a request to a file or STDOUT.
 */
void
sws_log(struct request *req, struct response *resp) {

	char buf[1024];
	char timestr[50];
	time_t now;

	fprintf(stderr, "%lu\n", resp->length);

	bzero(buf, sizeof(buf));
	now = time(NULL);
	strftime(timestr, sizeof(timestr),
		RFC1123_DATE, gmtime(&now));

	sprintf(buf, "%s %s %s %s %lu\n", req->ip,
		timestr, req->first_line, http_status,
		resp->length);

	if (!__sws_debug) {
		if (write(logfile_fd, buf, strlen(buf)) < 0) {
			perror("writing to logfile");
			exit(EXIT_FAILURE);
		}
	} else {
		printf("%s %s %s %s %lu\n", req->ip,
		timestr, req->first_line, http_status, resp->length);
	}
}

/*
 * Function to read and handle requests. Calls helper functions
 * to get method and headers, then checks for file existence
 * and whether or not it should be executed.
 */
void
sws_request(const int sock) {

	socklen_t length;
	struct request request;
	struct response resp;
	struct stat stat_buf;
	struct sockaddr_storage client;
	ino_t cgi_ino, path_ino;
	int port, rval;
	char *full_path, *tmp;
	char ipstr[INET6_ADDRSTRLEN];
	char buf[BUFF_SIZE];

	request.path = NULL;
	request.if_mod_since = NULL;
	request.first_line = NULL;
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
	} else { /* AF_INET6 */
		struct sockaddr_in6 *s = (struct sockaddr_in6 *)&client;
		port = ntohs(s->sin6_port);
		if (!inet_ntop(AF_INET6, &s->sin6_addr, ipstr, sizeof(ipstr))) {
			fprintf(stderr,"inet_ntop error: %s\n", strerror(errno));
			return;
		}
	}

	fprintf(stderr, "Connection from %s remote port %d\n",
			ipstr, port);

	request.ip = malloc(strlen(ipstr));
	strcpy(request.ip, ipstr);

	/* Begin with 200 OK as HTTP status code. */
	http_status = STATUS_200;

	bzero(buf, sizeof(buf));

	/* Get method/path/protocol  */
	if ((rval = sws_get_line(sock, buf, BUFF_SIZE)) < 0) {
		sws_response_header(sock, &request, &resp);
		return;
	}

	if ((request.first_line = malloc(strlen(buf)+1)) == NULL) {
		http_status = STATUS_500;
		sws_response_header(sock, &request, &resp);
		return;
	}
	strncpy(request.first_line, buf, strlen(buf));
	for (tmp = request.first_line; *tmp != '\r'; tmp++);
	*tmp = '\0';

	if (sws_parse_method(&request, buf) < 0) {
		sws_response_header(sock, &request, &resp);
		return;
	}

	/* Get headers, read until CRLF */
	while (1) {
		/* If simple request (HTTP/0.9), ignore headers */
		if (request.simple)
			break;
		bzero(buf, sizeof(buf));
		if ((rval = sws_get_line(sock, buf, BUFF_SIZE)) < 0) {
			sws_response_header(sock, &request, &resp);
			return;
		} else if (rval == 0) {
			fprintf(stderr, "Connection closed by client\n");
			return;
		} else if ((strlen(buf) == 2) &&
				(strcmp(buf, CRLF) == 0)) {
			break;
		} else {
			if (sws_parse_header(&request, buf) < 0) {
				sws_response_header(sock, &request, &resp);
				return;
			}
		}
	}

	if ((full_path =
		sws_file_path(__sws_dir, request.path)) == NULL) {
		http_status = STATUS_404;
		sws_response_header(sock, &request, &resp);
		return;
	}

	request.newpath = full_path;

	/* Check for CGI request */
	if (__sws_cgidir) {
		/* Get inodes of request path and cgidir */
		if (stat(request.newpath, &stat_buf) < 0) {
			perror("can't stat request path");
			if (errno == ENOENT || errno == ENOTDIR)
				http_status = STATUS_404;
			else
				http_status = STATUS_500;
			sws_response_header(sock, &request, &resp);
			return;
		}
		path_ino = stat_buf.st_ino;

		if (stat(__sws_cgidir, &stat_buf) < 0) {
			perror("can't stat cgidir");
			http_status = STATUS_500;
			sws_response_header(sock, &request, &resp);
			return;
		}
		cgi_ino = stat_buf.st_ino;

		if ((dir_dp = opendir(__sws_cgidir)) == NULL) {
			perror("opendir");
			http_status = STATUS_500;
			sws_response_header(sock, &request, &resp);
			return;
		}

		/* Check if requested file is in cgidir */
		if ((rval = file_in_dir(dir_dp, cgi_ino,
			path_ino, __sws_cgidir, 0)) < 0) {
			http_status = STATUS_500;
			sws_response_header(sock, &request, &resp);
			return;
		}

		if (closedir(dir_dp) < 0) {
			perror("closedir");
			http_status = STATUS_500;
			sws_response_header(sock, &request, &resp);
			return;
		}
	} else
		rval = 0;

	if (rval == 0) {
		/* File not in cgidir, serve it normally */
		if (sws_serve_file(sock, &request) < 0) {
			sws_response_header(sock, &request, &resp);
			return;
		}
	} else {
		/* File in cgidir, execute it */
		sws_execute_cgi(sock, &request);
	}

	/* Cleanup */
	if (request.path != NULL)
		free(request.path);
	if (full_path != NULL)
		free(full_path);
	if (request.first_line != NULL)
		free(request.first_line);
	if (request.if_mod_since != NULL)
		free(request.if_mod_since);
	if (request.ip != NULL)
		free(request.ip);
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

/*
 * Function to parse the first line of a request.
 * Returns -1 on error and sets http_status
 * appropriately.
 */
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
	if (strncasecmp(buf, "If-Modified-Since", i) == 0) {

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

/*
 * Function to serve a file. Returns -1 on error and sets
 * http_status appropriately. If an error occurs after
 * the response header has been sent then the function
 * just calls exit().
 */
int
sws_serve_file(int sock, struct request* req) {

	DIR *dp;
	struct dirent *dir;
	struct response resp;
	struct stat stat_buf;
	struct tm tm;
	time_t req_time;
	int fd, n;
	int index;
	char *tz;
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
	full_path = req->newpath;

	fprintf(stderr, "%s\n", full_path);

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
		/* Check for index.html */
		if ((dp = opendir(full_path)) == NULL) {
                	http_status = STATUS_500;
                	perror("opendir");
                	return -1;
		}

		index = 0;
		while ((dir = readdir(dp)) != NULL) {
			if (strcmp(dir->d_name, "index.html") == 0) {
				full_path = malloc(strlen(req->newpath)+11);
				strcpy(full_path, req->newpath);
				strncat(full_path, "index.html", 10);
				index = 1;
			}
		}

		if (index == 0) {
			sws_create_index(sock, req, &resp, req->newpath);
			return 0;
		}
	}

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

	if (req->if_mod_since != NULL) {
		strptime(req->if_mod_since, req->date_format, &tm);

		tz = getenv("TZ");
		setenv("TZ", "", 1);
		tzset();
		req_time = mktime(&tm);
		if (tz)
		setenv("TZ", tz, 1);
		else
			unsetenv("TZ");
		tzset();
		if (req_time > stat_buf.st_mtime)
			http_status = STATUS_304;
			n = 0;
	}
	strftime(last_mod, 50, RFC1123_DATE,
		gmtime(&stat_buf.st_mtime));

	resp.length = n;
	resp.content_type = TEXT_HTML;
	resp.last_modified = last_mod;

	sws_response_header(sock, req, &resp);

	/* If GET request and not 304, write file to client */
	if (req->method == 0 &&
		(strcmp(http_status, STATUS_200) == 0)) {
		bzero(buf, sizeof(buf));
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

	/* If a new char* was malloc'd for index.html, free */
	if (full_path != req->newpath) {
		free(full_path);
		full_path = NULL;
	}

	return 0;
}

/*
 * Function to send a response header.
 */
int
sws_response_header(int sock, struct request *req, struct response *resp) {

	time_t now;
	char buf[1024];
	char timestr[50];
	char html_msg[BUFF_SIZE];

	now = time(NULL);
	strftime(timestr, sizeof(timestr),
		RFC1123_DATE, gmtime(&now));

	if (strcmp(http_status, STATUS_200) == 0 ||
		strcmp(http_status, STATUS_304) == 0) {
		sprintf(buf, "HTTP/%s %s\r\n"
			"Date: %s\r\n"
			"Server: SWS\r\n"
			"Last-Modified: %s\r\n"
			"Content-Type: %s\r\n"
			"Content-Length: %lu\r\n"
			"\r\n", (req->simple == 1)? "0.9" : "1.0",
			http_status, timestr, resp->last_modified,
			resp->content_type, resp->length);
	} else {
		sprintf(html_msg, "<html><h1>%s</h1></html>",
			http_status);
		sprintf(buf, "HTTP/1.0 %s\r\n"
			"Date: %s\r\n"
			"Server: SWS\r\n"
			"Content-Type: text/html\r\n"
			"Content-Length: %lu\r\n"
			"\r\n", http_status, timestr,
			(unsigned long)strlen(html_msg));
		resp->length = (unsigned long)strlen(html_msg);
	}

	if (__sws_logfile)
		sws_log(req, resp);

	if (send(sock, buf, strlen(buf), 0) < 0) {
		perror("send");
		return -1;
	}

	if (!(strcmp(http_status, STATUS_200) == 0)) {
		if (send(sock, html_msg, strlen(html_msg), 0) < 0) {
			perror("send");
			return -1;
		}
	}

	return 0;
}

/*
 * Function to execute a CGI script. Environment variables set
 * are only the ones required by RFC 3875.
 * Returns -1 on error and http_status is set appropriately.
 */
int
sws_execute_cgi(int sock, struct request *req) {

	struct response resp;
	struct stat stat_buf;
	pid_t pid;
	int i, n, status, flag;
	int pipe_fd[2];
	char *tmp;
	char server_name[HOST_NAME_MAX];
	char buf[BUFF_SIZE];
	char last_mod[50];
	char c[2];

	resp.content_type = NULL;
	tmp = req->newpath;
	tmp += strlen(__sws_dir);
	while(*tmp == '/') tmp++;

	/* stat for mtime */
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

	if (pid == 0) {
		/* Set required env vars */
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

		bzero(buf, sizeof(buf));
		sprintf(buf, "SERVER_PORT=%d", __sws_port);
		if (putenv(buf) != 0) {
			perror("putenv");
			http_status = STATUS_500;
			return -1;
		}

		if (putenv("GATEWAY_INTERFACE=CGI/1.1") != 0) {
			perror("putenv");
			http_status = STATUS_500;
			return -1;
		}

		bzero(buf, sizeof(buf));
		sprintf(buf, "REMOTE_ADDR=%s", req->ip);
		if (putenv(buf) != 0) {
			perror("putenv");
			http_status = STATUS_500;
			return -1;
		}

		bzero(buf, sizeof(buf));
		if (putenv("REQUEST_METHOD=GET") != 0) {
			perror("putenv");
			http_status = STATUS_500;
			return -1;
		}

		if (req->if_mod_since != NULL) {
			bzero(buf, sizeof(buf));
			sprintf(buf, "HTTP_IF_MOD_SINCE=%s", req->if_mod_since);
			if (putenv(buf) != 0) {
				http_status = STATUS_500;
				return -1;
			}
		}

		close(pipe_fd[0]);

		/* Redirect CGI output to pipe */
		dup2(pipe_fd[1], STDOUT_FILENO);

		/* Execute CGI script */
		if (execl(tmp, tmp, NULL) < 0) {
			perror("execl");
			close(pipe_fd[1]);
			exit(EXIT_FAILURE);
			/* NOTREACHED */
		}
		exit(EXIT_SUCCESS);
	} else {
		bzero(buf, sizeof(buf));
		bzero(c, sizeof(c));

		flag = 0;
		/* Read 1 byte at a time until all headers consumed */
		while (n > 0) {
			if ((n = read(pipe_fd[0], c, 1)) < 0) {
				perror("reading cgi headers");
				return -1;
			}
			if (c[0] == '\n') {
				if (flag)
					break;
				flag = 1;
				if (strncasecmp(buf, "Content-Type:", 13) == 0) {
					if (resp.content_type != NULL) {
						free(resp.content_type);
						resp.content_type = NULL;
					}
					if ((resp.content_type =
						malloc(strlen(buf+13+1))) ==
							NULL) {
						perror("malloc");
						return -1;
					}
					for(i = 14; buf[i] == ' '; i++);
					strncpy(resp.content_type,
						&buf[i], strlen(buf));
				}
				if (strncasecmp(buf, "Content-Length:", 15)
					== 0) {
					for(i = 15; buf[i] == ' '; i++);
					buf[strlen(buf)-1] = '\0';
					resp.length = atoi(&buf[i]);
				}
				bzero(buf, sizeof(buf));
			} else {
				strncat(buf, c, 1);
				flag = 0;
			}
		}

		flag = 0;
		/* Read the entire output into buffer */
		if ((n = read(pipe_fd[0], buf, sizeof(buf))) < 0) {
			perror("read");
			return -1;
		}

		/* Default content type if none in GCI script */
		if (resp.content_type == NULL) {
			resp.content_type = TEXT_HTML;
			flag = 1;
		}

		/* If no length header, use read length) */
		if (!resp.length)
			resp.length = n;
		strftime(last_mod, sizeof(last_mod),
			RFC1123_DATE, gmtime(&stat_buf.st_mtime));
		resp.last_modified = last_mod;
		sws_response_header(sock, req, &resp);
		close(pipe_fd[1]);

		if (req->method == 0) {
			if (send(sock, buf, strlen(buf), 0) < 0) {
				perror("send");
				exit(EXIT_FAILURE);
				/* NOTREACHED */
			}
		}

		if (resp.content_type != NULL && flag != 1) {
			free(resp.content_type);
			resp.content_type = NULL;
		}
		waitpid(pid, &status, 0);
	}

	return 0;
}

/*
 * Function to create an index page when a folder is requested.
 * Returns -1 on error and http_status is set appropriately.
 */
int
sws_create_index(int sock, struct request* req,
	struct response *resp, char *path) {

	DIR *dp;
	struct dirent **dirlist;
	struct stat stat_buf;
	int i, n;
	int username_len;
	int home_path_len;
	int homedir;
	char buf[PATH_MAX];
	char *tmp;
	char *username;
	char index[BUFF_SIZE];

	bzero(index, sizeof(index));

	if ((dp = opendir(path)) == NULL) {
		http_status = STATUS_500;
		perror("opendir");
		return -1;
	}
	tmp = malloc(PATH_MAX);
	bzero(tmp, sizeof(tmp));

	/* Check if home directory was requested */
	homedir = 0;
	home_path_len = 0;
	if (strncmp(path, "/home/", 6) == 0) {
		homedir = 1;
		username = path + 6;
		/* Get username length */
		for (i = 0; username[i] != '/'; i++);
		username_len = i;
		home_path_len = strlen("/home/")+username_len+strlen("/sws");
	}

	if (homedir)
		strncpy(tmp, path + home_path_len,
			strlen(path+home_path_len) + 1);
	else
		strncpy(tmp, path + strlen(__sws_dir),
			strlen(path + strlen(__sws_dir))+1);

	/* Construct index */
	strncpy(index, "<html><body><h1>Index of ", 25);
	if (homedir) {
		strncat(index, "~", 1);
		strncat(index, username, username_len);
	}
	strncat(index, tmp, strlen(tmp));
	strncat(index, "</h1></br />", 12);

	/* Scan files in directory and sort them alphabetically */
	if ((n = scandir(path, &dirlist, NULL, alphasort)) < 0) {
		perror("scandir");
		return -1;
	}

	for (i = 0; i < n; i++) {
		bzero(buf, sizeof(buf));
		strncpy(buf, path, strlen(path));
		strncat(buf, dirlist[i]->d_name, strlen(dirlist[i]->d_name));
		stat(buf, &stat_buf);

		/* If file is not . or .. */
		if (strcmp(dirlist[i]->d_name, ".") != 0 &&
			strcmp(dirlist[i]->d_name, "..") != 0) {
			strncat(index, "<a href=\"", 9);
			if (homedir) {
				strncat(index, "/~", 2);
				strncat(index, username, username_len);
				strncat(index, tmp, strlen(tmp));
				strncat(index, dirlist[i]->d_name,
					strlen(dirlist[i]->d_name));
			} else {
				strncat(index, tmp, strlen(tmp));
				strncat(index, dirlist[i]->d_name,
					strlen(dirlist[i]->d_name));
			}
			if (S_ISDIR(stat_buf.st_mode))
				strncat(index, "/", 1);
		}

		if ((strcmp(dirlist[i]->d_name, ".") != 0) &&
			(strcmp(dirlist[i]->d_name, "..") != 0)) {
			strncat(index, "\">", 2);
			if (strcmp(dirlist[i]->d_name, "..") == 0)
				strncat(index, "Parent Directory", 16);
			else
				strncat(index, dirlist[i]->d_name,
					strlen(dirlist[i]->d_name));
			if (S_ISDIR(stat_buf.st_mode))
				strncat(index, "/", 1);
			strncat(index, "</a><br />", 10);
		}
		free(dirlist[i]);
	}
	free(dirlist);
	free(tmp);
	tmp = NULL;

	strncat(index, "<p style=\"font-style:italic\">SWS 1.0</p>", 40);
	strncat(index, "</body></html>", 14);
	resp->length = strlen(index);
	resp->content_type = TEXT_HTML;
	resp->last_modified = "";
	sws_response_header(sock, req, resp);

	if (req->method == 0) {
		if (send(sock, index, strlen(index), 0) < 0) {
			perror("send");
			exit(EXIT_FAILURE);
			/* NOTREACHED */
		}
	}

	return 0;
}
