#ifndef _RESPONSE_H_
#define _RESPONSE_H_

struct response {
	unsigned long length;
	char *last_modified;
	char *content_type;
};

struct response* create_response(void);
void destroy_response(struct response*);

#endif
