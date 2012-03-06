#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "content_type.h"
#include "defines.h"

struct content_type*
new_content_type() {

	struct content_type *ctypeptr;

	if ((ctypeptr = malloc(sizeof(struct content_type))) == NULL) {
		fprintf(stderr, "malloc error\n");
		return NULL;
	}
	ctypeptr->ctype = NULL;
	ctypeptr->exts = NULL;
	return ctypeptr;
}

void
delete_content_type(struct content_type *ctypeptr) {

	if (ctypeptr->ctype)
		free(ctypeptr->ctype);
	clear_list(ctypeptr->exts);

	free(ctypeptr);
	ctypeptr = NULL;
}

int
load_content_types(struct list *ctypes) {

	FILE *file;
	struct content_type *ctypeptr;
	struct node *nodeptr;
	size_t len;
	int n;
	char *buf, *buf2;
	char *tmp;
	char *saveptr, *token;

	len = BUFF_SIZE;
	if ((buf = malloc(BUFF_SIZE)) == NULL) {
		fprintf(stderr, "malloc error\n");
		return -1;
	}

	if ((file = fopen(CTYPES_FILE, "r")) == NULL) {
		perror("fopen");
		return -1;
	}

	while ((n = getline(&buf, &len, file)) > 0) {
		if ((ctypeptr = new_content_type()) == NULL)
			return -1;
		if ((nodeptr = create_node(ctypeptr)) == NULL)
			return -1;
		buf2 = buf;
		token = strtok_r(buf2, " ", &saveptr);
		if ((ctypeptr->ctype = malloc(strlen(token)+1)) == NULL) {
			fprintf(stderr, "malloc error\n");
			return -1;
		}
		strncpy(ctypeptr->ctype, token, strlen(token));
		append_to_list(ctypes, nodeptr);
		ctypeptr->exts = create_list();
		while (1) {
			buf2 = NULL;
			token = strtok_r(buf2, " ", &saveptr);
			if (token == NULL)
				break;
			//add token to extension list
			if ((tmp = malloc(strlen(token)+1)) == NULL) {
				fprintf(stderr, "malloc error\n");
				return -1;
			}
			strncpy(tmp, token, strlen(token));
			if (tmp[strlen(tmp)-1] == '\n')
				tmp[strlen(tmp)-1] = '\0';
			if ((nodeptr = create_node(tmp)) == NULL)
				return -1;
			append_to_list(ctypeptr->exts, nodeptr);
		}
	}

	if (n == -1 && errno) {
		perror("getline");
		return -1;
	}

	return 0;
}

char*
get_content_type(struct list *ctypes, char *file) {

	struct content_type *ctmp;
	struct node *nodeptr, *nodeptr2;

	if (file == NULL)
		return "text/plain";

	nodeptr = ctypes->head;
	while (nodeptr != NULL) {
		ctmp = (struct content_type*)nodeptr->data;
		nodeptr2 = ctmp->exts->head;
		while (nodeptr2 != NULL) {
			//printf("%s|%s\n", (char*)nodeptr2->data, file);
			if (strcmp((char*)nodeptr2->data,
				file) == 0)
				return ctmp->ctype;
			nodeptr2 = nodeptr2->next;
		}
		nodeptr = nodeptr->next;
	}

	return "text/plain";
}

void
free_content_types(struct list *ctypes) {

	struct content_type *ctmp;
	struct node *nodeptr;

	nodeptr = ctypes->head;
	while (nodeptr != NULL) {
		ctmp = (struct content_type*)nodeptr->data;
		delete_content_type(ctmp);
		nodeptr = nodeptr->next;
	}

	clear_list(ctypes);
}
