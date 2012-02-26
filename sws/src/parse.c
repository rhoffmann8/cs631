#define _XOPEN_SOURCE 1000

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#include "defines.h"
#include "parse.h"
#include "utils.h"

char*
http_realpath(char *path, char *serve_dir) {

	int username_len;
	char *tmp, *newpath;

	if (path[1] == '~') {
		//homedir
		tmp = path;
		tmp += 2;
		for (username_len = 0;
			*tmp != '/' && *tmp != '\0' && *tmp != ' ';
			 tmp++, username_len++);
		if (*tmp == '\0' || *tmp == ' ') {
			newpath = calloc(1, strlen("/home/")
				+ username_len + strlen("/sws") + 1);
			conncat(newpath, 3, "/home/", strlen("/home/"),
				path+2, username_len,
				"/sws", strlen("/sws"));
			return newpath;
		}

		if ((tmp = my_realpath(path+2+username_len)) == NULL) {
			//error
		}
		newpath = calloc(1, strlen(tmp) + username_len + 11);
		conncat(newpath, 4, "/home/", 6,
			path+2, username_len,
			"/sws", 4,
			tmp, strlen(tmp));
		free(tmp);
		return newpath;
	} else {
		tmp = my_realpath(path);
		newpath = calloc(1, strlen(tmp)+strlen(serve_dir)+1);
		conncat(newpath, 2, serve_dir, strlen(serve_dir),
			tmp, strlen(tmp));
		if (newpath[strlen(newpath)-1] == '/')
			newpath[strlen(newpath)-1] = '\0';
		free(tmp);
		return newpath;
	}

}

int
sws_parse_method(struct request *req, char *buf, char *serve_dir) {
	printf("entering parse_method\n");
	int i;
	char *tmp;

	if (strncmp("GET ", buf, 4) == 0) {
		i = 3;
		req->method = 0;
	} else if (strncmp("HEAD ", buf, 5) == 0) {
		i = 4;
		req->method = 1;
	} else if (strncmp("POST ", buf, 5) == 0) {
		i = 4;
		req->method = 2;
	} else {
		http_status = STATUS_501;
		return -1;
	}

	for (;i > 0; i--, buf++);
	for (;*buf == ' '; buf++);

	if (*buf != '/') {
		http_status = STATUS_400;
		return -1;
	}

	for (i = 0; buf[i] != ' ' && buf[i] != '\r'
		&& i < strlen(buf); i++)
		;
	if ((tmp = calloc(1, i+1)) == NULL) {
		fprintf(stderr, "calloc error\n");
		http_status = STATUS_500;
		return -1;
	}
	strncpy(tmp, buf, i);
	req->path = tmp;

	if ((req->realpath = http_realpath(req->path, serve_dir)) == NULL) {
		http_status = STATUS_500;
		return -1;
	}

	for (;i > 0; i--, buf++);
	for (;*buf == ' '; buf++);

	if ((strncmp(buf, CRLF, 2) == 0) && req->method == 0) {
		req->simple = 1;
	} else if (strncmp(buf, "HTTP/1.0", 8) == 0) {
	//make HTTP/1.1 for now for browser testing
	//} else if (strncmp(buf, "HTTP/1.1", 8) == 0) {
		req->simple = 0;
	} else {
		http_status = STATUS_400;
		return -1;
	}
	printf("leaving parse_method\n");
	return 0;
}

int
sws_parse_header(struct request *req, char *buf) {
	printf("entering parse_header\n");
	struct tm time;
	int i;
	char *tmp;

	if (strchr(buf, ':') == NULL) {
		http_status = STATUS_400;
		return -1;
	}

	for (;*buf == ' '; buf++);
	for (i = 0; buf[i] != ':'; i++);

	if (strncasecmp(buf, "If-Modified-Since", i) == 0) {
		for (; i > 0; i--, buf++);
		if (*buf != ':' && (*(buf+1) != ' ')) {
			http_status = STATUS_400;
			return -1;
		}
		buf += 2;

		if ((tmp = calloc(1, strlen(buf))) == NULL) {
			fprintf(stderr, "calloc error\n");
			http_status = STATUS_500;
			return -1;
		}
		strncpy(tmp, buf, strlen(buf));
		req->if_mod_since = tmp;

		/* Trim CRLF */
		for (; *tmp != '\r' && *tmp != '\n'; tmp++);
		tmp = '\0';

		if (strptime(req->if_mod_since, RFC1123_DATE, &time) != NULL)
			req->date_format = RFC1123_DATE;
		else if (strptime(req->if_mod_since, RFC850_DATE, &time) != NULL)
			req->date_format = RFC850_DATE;
		else if (strptime(req->if_mod_since, ASCTIME_DATE, &time) != NULL)
			req->date_format = ASCTIME_DATE;
		else {
			free(req->if_mod_since);
			req->date_format = req->if_mod_since = NULL;
		}
	}

	printf("leaving parse_header\n");
	return 0;
}
