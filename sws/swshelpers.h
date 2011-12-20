#ifndef _SWSHELPERS_H_
#define _SWSHELPERS_H_

/* Struct to hold properties of a request. "Path" refers to the
 * original path in the request, "newpath" is a converted path
 * created by the server. */
struct request {
	char *first_line;
	char *ip;
	int method;
	int simple;
	char *path;
	char *newpath;
	char *if_mod_since;
	char *date_format;
} request;

/* Struct to hold properties of a response. */
struct response {
	unsigned long length;
	char *last_modified;
	char *content_type;
} response;

/* Wrapper for sws_log. Extracts components from req/resp and
 * creates the string to send to the log. */
void sws_log_wrapper(struct request*, struct response*);

int sws_get_line(int, char*, int);

int sws_parse_method(struct request*, char*);

int sws_parse_header(struct request*, char*);

int sws_serve_file(int, struct request*);

int sws_response_header(int, struct request*, struct response*);

int sws_execute_cgi(int, struct request*);

int sws_create_index(int, struct request*, struct response*, char*);

#endif
