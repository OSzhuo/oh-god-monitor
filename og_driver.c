#include <stdio.h>

#define OG_NAME_MAX		255

#include "og_tree.h"

int main(void)
{
	char *p = "/tmp/tree.st";
	og_tree_init(p, 256);
	

	return 0;
}
