#include <stdio.h>
#include <stdlib.h>

#include "list.h"

struct list*
create_list() {

	struct list *l;

	if ((l = malloc(sizeof(struct list))) == NULL) {
		fprintf(stderr, "malloc error\n");
		return NULL;
	}
	l->head = NULL;

	return l;
}

struct node*
create_node(void *data) {

	struct node *n;

	if ((n = malloc(sizeof(struct node))) == NULL) {
		fprintf(stderr, "malloc error\n");
		return NULL;
	}
	n->next = NULL;
	n->data = data;

	return n;
}

void
append_to_list(struct list *l, struct node *n) {

	struct node *tmp;

	if (l->head == NULL) {
		l->head = n;
	} else {
		tmp = l->head;
		while (tmp->next != NULL)
			tmp = tmp->next;
		tmp->next = n;
	}
}

void
clear_list(struct list *l) {

	struct node *tmp, *tmp2;

	if (l == NULL)
		return;
	tmp = l->head;
	while (tmp != NULL) {
		tmp2 = tmp;
		tmp = tmp->next;
		free(tmp2);
	}

	free(l);
}
