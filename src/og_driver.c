#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OG_NAME_MAX		255

#include "og_tree.h"

int func_edit(void *a, void *b, size_t n);

struct my_data_t {
	int	id;
	int	class;
	char	name[12];
};

void prt_func2(void *data)
{
	struct my_data_t *d = data;

	printf("id:%-6d\n", d->id);
}

void prt_func(void *data)
{
	struct my_data_t *d = data;

	printf("id:%-6d class:%-6d name:%-6s\n", d->id, d->class, d->name);
}

void insert(int tree_h)
{
	struct my_data_t data;

	int i = 110970;
	int x = 0;
	og_node *t = NULL;
	og_node *t1 = NULL;
	og_node *t2 = NULL;
	og_node *t3 = NULL;
	//while(i--){
	//	x++;
	//	data.id = x;
	//	data.class = x % 5;
	//	strcpy(data.name, "mdzz");
	//	if(NULL == (t = og_insert_by_parent(tree_h, &data, sizeof(data), t))){
	//		printf("err!\n");
	//		break;
	//	}
	//}
	x++;
	data.id = x;
	data.class = x % 5;
	strcpy(data.name, "mdzz1");
	printf("want insert id %d\n", data.id);
	if(NULL == (t = og_insert_by_parent(tree_h, &data, sizeof(data), t))){
		printf("err!\n");
	}
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
	if(NULL == (t = t = og_insert_by_parent(tree_h, &data, sizeof(data), t))){
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

og_travel(tree_h, prt_func);
og_tree_travel(tree_h, prt_func2);

	og_move_node(tree_h, t2, t1);
	og_move_node(tree_h, t2, t3);
og_tree_travel(tree_h, prt_func2);

	strcpy(data.name, "hello");
	og_edit_data(tree_h, t, &func_edit, &data, sizeof(data));
}

int func_edit(void *a, void *b, size_t n)
{
	memcpy(a, b, n);

	return 0;
}

int main(void)
{
	char *p = "/ibig/tree.st";
	int tree_h;
	struct my_data_t data;

	tree_h = og_init(p, 246+sizeof(struct my_data_t));
printf("get handler %d\n", tree_h);
printf("sizeof() is %lu\n", sizeof(og_node));
insert(tree_h);
//sleep(30);
og_travel(tree_h, prt_func);
	sleep(500);
	//og_destory(tree_h);
	

	return 0;
}
