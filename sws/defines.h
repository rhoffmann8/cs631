#ifndef _DEFINES_H_
#define _DEFINES_H_

#define BUFF_SIZE 8096

#define RFC1123_DATE "%a, %d %b %Y %T GMT"
//#define RFC1123_DATE "%a, %d %b"
#define RFC850_DATE "%A, %d-%b-%y %T GMT"
#define ASCTIME_DATE "%a %b %e %T %Y"

#define CRLF "\r\n"

#define STATUS_200 "200 OK"
#define STATUS_304 "304 Not Modified"
#define STATUS_400 "400 Bad Request"
#define STATUS_403 "403 Forbidden"
#define STATUS_404 "404 Not Found"
#define STATUS_500 "500 Internal Server Error"
#define STATUS_501 "501 Not Implemented"

extern char *http_status;

#endif
