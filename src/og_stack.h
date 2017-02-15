#ifndef _OG_STACK_H_
#define _OG_STACK_H_

typedef struct ogs_node_t {
	struct ogs_node_t	*next;	/*next*/
	//struct ogs_head_t	*p;	/*prev*/
	void			*data;
} ogs_node;

typedef struct ogs_head_t {
	ogs_node		*head;
} ogs_head;

ogs_head *ogs_init(void);
int ogs_push(ogs_head *h, void *data);
void *ogs_top(ogs_head *h);
void *ogs_pop(ogs_head *h);
int ogs_empty(ogs_head *h);
void ogs_destory(ogs_head *h, void (*func)(void *data));

#endif
