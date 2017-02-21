#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>

#include <stdio.h>
#include "og_init.h"
#include "og_watch.h"
#include "og_record.h"
#include "og_stack.h"

#include "obuf.h"

#define PID_PATH		"/tmp/ibig_monitor.pid"

// * @bd	obuf descriptor
int glb_bd = -1;

void *read_from_buf(void *bd);
int init_buf(int size);

/**
 * temp
 * 
 */
int og_master(void)
{

//	if(argc < 2){
//		fprintf(stderr, "Usage: %s action path0 [path1 path2...]\n", argv[0]);
//		exit(1);
//	}

	pthread_t x;
	int bd;
	int watch_fd;

	//if((bd = init_buf(512*1024)) < 0){
	if((bd = init_buf(1024)) < 0){
		return -1;
	}

	if((watch_fd = og_watch_init(bd)) < 0){
		return -1;
	}
	if(og_record_init(bd)){
		return -1;
	}

	if(pthread_create(&x, NULL, og_record_work, NULL))
		perror("create()");

	og_init_start();

	char *path = "/ibig/tmp/mnt";//"/tmp/mnt/USB-disk-1";
	if(og_list_all(watch_fd, path, bd, og_add_watch)){
		perror("list_all nftw()");
		exit(1);
	}
	//og_init_over();

//printf("list all ok buf size %lu\n", obuf_get_used(glb_bd, NULL));

	//fprintf(stderr, "list_all() err\n");

	//int i;
	//for(i = 0; i < MIN_NWD; i++)
	//	printf("i[%d] path:%p\n", i, watch_list[i]);

	//i_listen(fd_watch);

	pthread_join(x, NULL);

	return 0;
}

int init_buf(int size)
{
	int err;
	int bd;

	if((bd = obuf_open(size, &err)) < 0){
		fprintf(stderr, "obuf_open():%s\n", strerror(err));
		return -1;
	}

	return bd;
}

int main(int argc, char *argv[])
{
setvbuf(stdout, NULL, _IONBF, 0);
setvbuf(stderr, NULL, _IONBF, 0);

	og_master();

	return 0;
}
