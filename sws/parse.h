#ifndef _PARSE_H_
#define _PARSE_H_

#include "request.h"

char* http_realpath(char*, char*);
int sws_parse_method(struct request*, char*, char*);
int sws_parse_header(struct request*, char*);

#endif
