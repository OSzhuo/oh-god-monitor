#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OG_NAME_MAX		255

#include "og_tree.h"

int func_edit(void *a, void *b, size_t n);
int _func(void *data, void *b, const og_node *this);

int glb_handler;

struct my_data_t {
	int	id;
	int	class;
	char	name[12];
};

int travel_func(void *data, void *b, const og_node *this)
{
	struct my_data_t *d = data;

	printf("this ");
	printf("id:%-6d class:%-6d name:%-6s\n", d->id, d->class, d->name);
printf("[parent] \n");

	og_node *parent = og_parent(this);
	if(parent){
		//printf("parent----->");
		og_get_node_travel(glb_handler, parent, _func, NULL);
	}
printf(" END\n");

	return 0;
}

int _func(void *data, void *b, const og_node *this)
{
	struct my_data_t *d = data;
	og_node *parent = og_parent(this);


	if(parent){
		//printf("    ");
		og_get_node_travel(glb_handler, parent, _func, NULL);
	}
	printf("id:%-6d  ", d->id);


	return 0;
}

void prt_func(void *data)
{
	struct my_data_t *d = data;

	printf("id:%-6d class:%-6d name:%-6s\n", d->id, d->class, d->name);
}

void prt_func2(void *data)
{
	struct my_data_t *d = data;

	printf("id:%-6d\n", d->id);
}

void insert(int tree_h)
{
	struct my_data_t data;

//	int i = 110970;
	int i = 15;
	int x = 0;
	og_node *t = NULL;
	og_node *t1 = NULL;
	og_node *t2 = NULL;
	og_node *t3 = NULL;
	while(i--){
		x++;
		data.id = x;
		data.class = x % 5;
		strcpy(data.name, "mdzz");
		if(NULL == (t = og_insert_by_parent(tree_h, &data, sizeof(data), t))){
			printf("err!\n");
			break;
		}
	}
	x++;
	data.id = x;
	data.class = x % 5;
	strcpy(data.name, "mdzz1");
	printf("want insert id %d\n", data.id);
	if(NULL == (t = og_insert_by_parent(tree_h, &data, sizeof(data), t))){
		printf("err!\n");
	}
printf( "insert ok \n");
	t1 = t;
	x++;
	data.id = x;
	data.class = x % 5;
	printf("want insert id %d\n", data.id);
	strcpy(data.name, "mdzz2");
	if(NULL == (t2 = t = og_insert_by_parent(tree_h, &data, sizeof(data), t))){
		printf("err!\n");
	}
//og_delete_node(tree_h,t);
	x++;
	data.id = x;
	data.class = x % 5;
	strcpy(data.name, "mdzz3");
	printf("want insert id %d\n", data.id);
	if(NULL == (t3 = t = og_insert_by_parent(tree_h, &data, sizeof(data), NULL))){
		printf("err!\n");
	}
//og_delete_node(tree_h,t);
//printf("t%p %lu %lu", t->parent, t->pos.page, t->pos.offset);
	x++;
	data.id = x;
	data.class = x % 5;
	strcpy(data.name, "mdzz4");
	printf("want insert id %d\n", data.id);
	if(NULL == (t = og_insert_by_parent(tree_h, &data, sizeof(data), t))){
		printf("err!\n");
	}
	x++;
	data.id = x;
	data.class = x % 5;
	strcpy(data.name, "mdzz5");
	printf("want insert id %d\n", data.id);
	if(NULL == (t = og_insert_by_parent(tree_h, &data, sizeof(data), t))){
		printf("err!\n");
	}
	x++;
	data.id = x;
	data.class = x % 5;
	strcpy(data.name, "mdzz6");
	printf("want insert id %d\n", data.id);
	if(NULL == (t = og_insert_by_parent(tree_h, &data, sizeof(data), t1))){
		printf("err!\n");
	}

og_preorder_R(tree_h, travel_func, NULL);
og_tree_travel(tree_h, prt_func2);

	og_move_node(tree_h, t2, t1);
	og_move_node(tree_h, t2, t3);
//og_preorder_R(tree_h, travel_func, NULL);
//og_tree_travel(tree_h, prt_func2);

	strcpy(data.name, "hello");
	og_edit_data(tree_h, t, &func_edit, &data, sizeof(data));
}

int main(void)
{
	char *p = "/ibig/tree.st";
	int tree_h;
	struct my_data_t data;

	glb_handler = tree_h = og_init(p, 246+sizeof(struct my_data_t));
printf("get handler %d\n", tree_h);
printf("sizeof() is %lu\n", 246+sizeof(struct my_data_t)+sizeof(og_data_node));
sleep(1);
insert(tree_h);
sleep(2);
//og_travel(tree_h, prt_func);
	sleep(500);
	//og_destory(tree_h);
	

	return 0;
}
