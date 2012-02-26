#include <stdio.h>
#include <stdlib.h>

#include "response.h"

struct response*
create_response(void) {

	struct response *resp;

	if ((resp = malloc(sizeof(struct response))) == NULL) {
		fprintf(stderr, "malloc error\n");
		return NULL;
	}

	resp->length = -1;
	resp->last_modified = resp->content_type = NULL;

	return resp;
}

void
destroy_response(struct response *resp) {

	if (resp->last_modified)
		free(resp->last_modified);
	free(resp);
	resp = NULL;
}
