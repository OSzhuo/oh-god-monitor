#include <stdlib.h>

#include "og_stack.h"
/*for debug*/
#include <stdio.h>

//static inline void _del_node(ogs_node *prev, ogs_node *next);
static inline void _list_init(ogs_node *head);
static inline void _list_add(ogs_node *new, ogs_node *head);

static inline int _ogs_empty(ogs_node *head);

ogs_head *ogs_init()
{
	ogs_head *head;
	ogs_node *node;

	if(NULL == (head = malloc(sizeof(ogs_head)))){
		return NULL;
	}

	if(NULL == (node = malloc(sizeof(ogs_node)))){
		free(head);
		return NULL;
	}

	node->data = NULL;
	_list_init(node);

	head->head = node;

	return head;
}

static inline void _list_init(ogs_node *head)
{
	head->next = NULL;
}

static inline void _list_add(ogs_node *new, ogs_node *head)//, ogs_node *next)
{
	new->next = head->next;
	head->next = new;
}

/**
 * push data into stack
 */
int ogs_push(ogs_head *h, void *data)
{
	ogs_node *this;
	ogs_node *head = h->head;

	if(!head)
		return -1;

	if(NULL == (this = malloc(sizeof(ogs_node)))){
		return -1;
	}

	this->data = data;

	_list_add(this, head);
//printf("push %p\n", data);

	return 0;
}

//static inline void _del_node(ogs_node *prev, ogs_node *next)
//{
//}

void *ogs_top(ogs_head *h)
{
	if(!h->head || _ogs_empty(h->head))
		return NULL;

	return h->head->next->data;
}

/**
 * pop data out from stack
 */
void *ogs_pop(ogs_head *h)
{
	ogs_node *head = h->head;

	if(!head || _ogs_empty(head))
		return NULL;

	ogs_node *this = head->next;
	void *data = this->data;

	head->next = this->next;
	this->next = NULL;
	//free(this);

//printf("pop  %p\n", data);
	return data;
}

/**
 * test whether list is empty
 * return	empty		1
 * 		Not empty	0
 * 		error		-1
 */
int ogs_empty(ogs_head *h)
{
	if(!h->head)
		return -1;

	return _ogs_empty(h->head);
}

/**
 * test whether list is empty
 * return	empty		1
 * 		Not empty	0
 */
static inline int _ogs_empty(ogs_node *head)
{
	return head->next == NULL;
}

/**
 * unsafe, if get err when destory, can not pause
 */
void ogs_destory(ogs_head *h, void (*func)(void *data))
{
	ogs_node *head = h->head;

//fprintf(stderr, "h = %p", h);
//fprintf(stderr, "head = %p", head);
	if(!head)
		return;

	ogs_node *this = head->next;
	ogs_node *tmp;

	while(this){
		tmp = this->next;
		(*func)(this->data);
		free(this);
		this = tmp;
	}

	free(head);
	h->head = NULL;
	free(h);
}

void ogs_travel(ogs_head *h, void (*func_prt)(void *data))
{
	ogs_node *head = h->head;

	if(!head)
		return;

	ogs_node *this = head->next;

	while(this){
		(*func_prt)(this->data);
		this = this->next;
	}
}
