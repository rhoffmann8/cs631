#ifndef _FILES_H_
#define _FILES_H_

#include "request.h"
#include "response.h"

int sws_create_index(int, struct request*, struct response*, char*);
int sws_serve_file(int, struct request*, struct response*);
int sws_execute_cgi(int, struct request*, struct response*);

#endif
