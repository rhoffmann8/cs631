#define _XOPEN_SOURCE 1000

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#include "defines.h"
#include "parse.h"

char*
http_realpath(char *path, char *serve_dir) {
	printf("entering http_realpath()\n");
	int alloc_size, i, homedir, username_len;
	char *tmp, *tmp2, *username;

	if (path[1] == '~') {
		homedir = 1;
		username = path+2; //move past '/~'
		for (username_len = 0; username[username_len] != '/'
			&& username[username_len] != ' '
			&& username[username_len] != '\0'; username_len++)
			;
		path += (2 + username_len);
		alloc_size = strlen(path) + strlen("/home/") + strlen("/sws");
	} else {
		homedir = 0;
		alloc_size = strlen(path)+strlen(serve_dir)+1;
	}

	if ((tmp = calloc(1, alloc_size)) == NULL) {
		fprintf(stderr, "calloc error\n");
		return NULL;
	}

	while (*path != '\0') {
		if (strlen(path) == 1)
			break;
		if (strncmp(path, "//", 2) == 0) {
			path++;
		} else if ((strncmp(path, "/.", 2) == 0)
			&& (path[2] == '/' || path[2] == '\0')) {
			path += 2;
		} else if ((strncmp(path, "/..", 3) == 0)
			&& (path[3] == '/' || path[3] == '\0')) {
			if ((tmp2 = strrchr(tmp, '/')) != NULL)
				*tmp2 = '\0';
			path += 3;
		} else {
			for (i = 1; path[i] != '/' && path[i] != '\0'; i++);
			strncat(tmp, path, i);
			path += i;
		}
	}
	//if (strlen(tmp) == 0)
	//	strncpy(tmp, "/", 1);

	tmp2 = calloc(1, sizeof(tmp)+1);
	strncpy(tmp2, tmp, strlen(tmp));

	if (homedir) {
		memset(tmp, 0, alloc_size);
		strncpy(tmp, "/home/", 6);
		strncat(tmp, username, username_len);
		strncat(tmp, "/sws", 4);
	} else {
		strncpy(tmp, serve_dir, strlen(serve_dir));
	}

	strncat(tmp, tmp2, strlen(tmp2));
	free(tmp2);

	printf("leaving http_realpath()\n");
	return tmp;
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
	} else {
		printf("returning\n");
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
	} else if (strncmp(buf, "HTTP/1.1", 8) == 0) {
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
	printf("%s\n", buf);
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

/*
 * Wrapper for strrchr which returns the numerical position
 * of the character found in the string.
 */
int
strrchr_pos(char *str, char c, int len) {

	int n;
	char *ptr, *ptr2;

	if ((ptr2 = strrchr(str, c)) == NULL)
		return -1;
	ptr = str;
	for (n = 0; n < len && ptr != ptr2; n++, ptr++);

	if (ptr != ptr2)
		return -1;

	return n;
}
