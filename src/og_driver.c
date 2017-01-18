#include <stdio.h>

#define OG_NAME_MAX		255

#include "og_tree.h"

struct my_data_t {
	int	id;
	int	class;
	char	name[12];
};

int main(void)
{
	char *p = "/ibig/tree.st";
	int tree_h;
	struct my_data_t data;

	tree_h = og_init(p, sizeof(struct my_data_t));
printf("get handler %d\n", tree_h);
	int i = 110970;
	int x = 0;
	while(i--){
		x++;
		data.id = x;
		data.class = x % 5;
		strcpy(data.name, "mdzz");
		og_insert(tree_h, data.name, sizeof(data));
	}
printf("this x = %d i = %d\n", x, i);
	//sleep(500);
	//og_destory(tree_h);
	

	return 0;
}
