#include <stdlib>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "og_tree.h"

#include <stdio.h>

static og_head *handlers[HANDLER_CNT];

int og_tree_init(const char *path, int size)
{
	int handler;
	if((handler = _get_next_handler()) < 0)
		return -1;

	int fd;

	if((fd = open(path, O_CREAT|O_TRUNC|O_RDWR, 0644)) < 0){
		perror("open()");
		return -1;
	}
	og_head *head;

	if(NULL == (head = malloc(sizeof(og_head)))){
		perror("malloc()");
		return -1;
	}

	handlers[handler];

	return handler;
}

int _get_next_handler()
{
	int i;

	for(i = 0; i < HANDLER_CNT; i++){
		if(NULL == handlers[i])
			return i;
	}

	return -1;
}
