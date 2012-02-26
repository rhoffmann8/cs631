#include <stdio.h>
#include <stdlib.h>

#include "request.h"

struct request*
create_request(void) {

	struct request *req;
	if ((req = malloc(sizeof(struct request))) == NULL) {
		fprintf(stderr, "malloc error\n");
		return NULL;
	}

	req->length = -1;
	req->date_format = req->if_mod_since
		= req->ip = req->method_line
		= req->path = req->realpath
		= NULL;

	return req;
}

void
destroy_request(struct request *req) {

	if (req->ip)
		free(req->ip);
	if (req->method_line)
		free(req->method_line);
	if (req->path)
		free(req->path);
	if (req->realpath)
		free(req->realpath);

	free(req);
	req = NULL;
}
