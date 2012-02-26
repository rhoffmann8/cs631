#include <sys/stat.h>
#include <sys/types.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "defines.h"
#include "log.h"

int
init_logfile(char *path) {

	struct stat stat_buf;
	int fd;
	char *tmppath;

	tmppath = NULL;
	if (stat(path, &stat_buf) < 0 && errno != ENOENT) {
		perror("couldn't stat logfile");
		return -1;
	}

	if (S_ISDIR(stat_buf.st_mode)) {
		if ((tmppath = calloc(1, strlen(path)
			+ strlen(LOGFILE) + 2)) == NULL) {
			fprintf(stderr, "calloc error\n");
			return -1;
		}
		strncpy(tmppath, path, strlen(path));
		strncat(tmppath, "/", 1);
		strncat(tmppath, LOGFILE, strlen(LOGFILE));
	}

	if ((fd = open(path, O_APPEND | O_CREAT | O_RDWR,
		S_IRUSR | S_IWUSR | S_IRGRP)) < 0) {
		perror("opening logfile");
		return -1;
	}

	if (tmppath != NULL)
		free(tmppath);

	return fd;
}

void
sws_log(int fd, const struct request *req,
	const struct response *resp, int debug) {

	char buf[1024];
	char timestr[50];
	time_t now;

	bzero(buf, sizeof(buf));
	if ((now = time(NULL)) == (time_t)-1) {
		/* Reponse already done, just exit */
		perror("time");
		exit(EXIT_FAILURE);
		/* NOTREACHED */
	}

	strftime(timestr, sizeof(timestr),
		RFC1123_DATE, gmtime(&now));

	sprintf(buf, "%s %s %s %s %lu\n", req->ip,
		timestr, req->method_line, http_status,
		resp->length);

	if (debug) {
		if (write(fd, buf, strlen(buf)) < 0)
			perror("writing to logfile");
	} else
		printf("%s", buf);
}
