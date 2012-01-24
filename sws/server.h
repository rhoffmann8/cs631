#ifndef _SERVER_H_
#define _SERVER_H_

#include "request.h"
#include "response.h"

struct swsopts {
	char *cgidir;
	int debug;
	char *dir;
	char *ip;
	char *logfile;
	int port;
	char *secdir;
	char *key;
} opts;

void sws_cleanup(int);

void sws_init(const struct swsopts);

int sws_recv_line(int, char*, int);

void sws_handle_request(const int);

int sws_response_headers(int, struct request*, struct response*);

#endif
