#ifndef _LOG_H_
#define _LOG_H_

#define LOGFILE "sws.log"

#include "request.h"
#include "response.h"

int init_logfile(char*);
void sws_log(int, const struct request*, const struct response*, int);

#endif
