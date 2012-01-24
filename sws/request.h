#ifndef _REQUEST_H_
#define _REQUEST_H_

struct request {
	int method;
	int simple;
	char *date_format;
	char *if_mod_since;
	char *ip;
	char *method_line;
	char *path;
	char *realpath;
};

struct request* create_request(void);
void destroy_request(struct request*);

#endif
