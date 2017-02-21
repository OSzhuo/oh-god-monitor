#include <stdlib.h>
//#include <unistd.h>
#include <sys/inotify.h>

#include "og_watch.h"
/*temp*/
#include <stdio.h>

_og_unit *pub_unit;

/**
 * init inotify, and init the public unit to write into obuf after init ok
 * 
 */
int og_watch_init(int bd)
{
	int fd;

	if(NULL == (pub_unit = calloc(sizeof(_og_unit) + IBIG_PATH_MAX, sizeof(char)))){
		perror("calloc()");
		return -1;
	}

	if((fd = inotify_init()) < 0){
		perror("inotify_init()");
		free(pub_unit);
		pub_unit = NULL;
		return -1;
	}

	return fd;
}

int og_add_watch(int watch_fd, const char *path)
{
	int wd = -1;

	if((wd = inotify_add_watch(watch_fd, path, IBIG_EVENT_WATCH)) < 0){
		perror("inotify_add_watch()");
		return -1;
	}

//printf("[add] wd[%d]( %s )\n", wd, path);

	return wd;
}
