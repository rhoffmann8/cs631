#ifndef _CONTENT_TYPE_H_
#define _CONTENT_TYPE_H_

#define CTYPES_FILE "content_types"

#include "list.h"

/*struct extension {
	char *ext;
} extension;*/

struct content_type {
	char *ctype;
	struct list *exts;
	//struct extension *ext;
} content_type;

struct content_type* new_content_type();
void delete_content_type(struct content_type*);
int load_content_types(struct list*);
char *get_content_type(struct list*, char*);
void free_content_types(struct list*);

extern struct list *ctypes;

#endif
