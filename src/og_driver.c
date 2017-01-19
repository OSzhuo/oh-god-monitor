#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OG_NAME_MAX		255

#include "og_tree.h"

struct my_data_t {
	int	id;
	int	class;
	char	name[12];
};

void prt_func(void *data)
{
	struct my_data_t *d = data;

	printf("id:%-6d class:%-6d name:%-6s\n", d->id, d->class, d->name);
}

int main(void)
{
	char *p = "/ibig/tree.st";
	int tree_h;
	struct my_data_t data;

	tree_h = og_init(p, 246+sizeof(struct my_data_t));
printf("get handler %d\n", tree_h);
printf("sizeof() is %lu\n", sizeof(data));
	int i = 110970;
	int x = 0;
	while(i--){
		x++;
		data.id = x;
		data.class = x % 5;
		strcpy(data.name, "mdzz");
		og_insert(tree_h, &data, sizeof(data));
	}
//sleep(3);
og_travel(tree_h, prt_func);
printf("this x = %d i = %d\n", x, i);
	//sleep(500);
	//og_destory(tree_h);
	

	return 0;
}
