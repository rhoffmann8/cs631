#ifndef _LIST_H_
#define _LIST_H_

struct list {
	struct node *head;
};

struct node {
	struct node *next;
	void *data;
};

struct list* create_list();
struct node* create_node(void*);
void append_to_list(struct list*, struct node*);
void clear_list(struct list*);

#endif
