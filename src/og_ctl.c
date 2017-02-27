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
#include "og_write.h"

#include "obuf.h"

#define PID_S_PATH		"/tmp/og_s.pid"
#define PID_C_PATH		"/tmp/og_c.pid"
#define ARG_ACT_S		'S'
#define ARG_ACT_C		'C'

int og_client(void);
int init_buf(int size);
int og_master(int cnt, char **path);
int get_action(const char *action);
int set_pid_file(char *path);
void og_daemonize(void);

/**
 * 
 * 
 */
int og_master(int cnt, char **path)
{
	og_daemonize();

	set_pid_file(PID_S_PATH);

	pthread_t last;
	int bd;
	int watch_fd;

	//if((bd = init_buf(512*1024)) < 0){
	if((bd = init_buf(1024)) < 0){
		return -1;
	}

	if((watch_fd = og_watch_init(bd)) < 0){
		return -1;
	}
	if(og_record_init(bd, watch_fd)){
		return -1;
	}

	if(pthread_create(&last, NULL, og_record_work, NULL)){
		perror("pthread_create()");
		return -1;
	}

	og_init_start();
	og_server_init(OG_SOCK_FILE);

	if(pthread_create(&last, NULL, og_server_work, NULL)){
		perror("pthread_create()");
		return -1;
	}

	//char *path = "/tmp/mnt/USB-disk-1";//"/tmp/mnt/USB-disk-1";
	int i;
	int err = 1;
	for(i = 0; i < cnt; i++){
		fprintf(stdout, "start list file in [%s]\n", path[i]);
		if(!og_list_all(watch_fd, path[i], bd, og_add_watch)){
			err = 0;
		}
	}
	if(err){
		fprintf(stderr, "file list init failed !\n");
		return -1;
	}
	og_init_over();

	if(pthread_create(&last, NULL, og_watch_listen_cycle, NULL)){
		perror("pthread_create()");
		return -1;
	}

	pthread_join(last, NULL);

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

/*
 * 
 */
int get_action(const char *action)
{
	int ret;

	switch(action[0]){
	case 'C':
	case 'c':
		fprintf(stdout, "get action Client\n");
		ret = ARG_ACT_C;
		break;
	case 'S':
	case 's':
		fprintf(stdout, "get action Server\n");
		ret = ARG_ACT_S;
		break;
	default:
		ret = -1;
		break;
	}

	return ret;
}

/**
 * og_monitor action[server/client] path0 [path1 path2 ...]
 */
int main(int argc, char *argv[])
{
	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);

	if(argc < 2){
		fprintf(stderr, "Usage: %s action path0 [path1 path2...]\n", argv[0]);
		exit(1);
	}

	switch(get_action(argv[1])){
	case ARG_ACT_C:
		og_client();
	case ARG_ACT_S:
		if(argc < 3){
			fprintf(stderr, "Usage: %s action path0 [path1 path2...]\n", argv[0]);
			exit(1);
		}
		if(og_master(argc-2, argv+2)){
			exit(1);
		}
		break;
	}

	return 0;
}

int og_client(void)
{
	set_pid_file(PID_C_PATH);
	int ret;

	if(og_client_init(OG_SOCK_FILE)){
		exit(1);
	}
	ret = og_client_work();
	og_client_clean();

	exit(ret);
}

int set_pid_file(char *path)
{
	pid_t pid;
	pid = getpid();
	char pid_str[8];
	int fd;

	sprintf(pid_str, "%d\n", pid);

	if((fd = open(path, O_CREAT | O_TRUNC | O_RDWR, 0644)) < 0){
		perror("open()");
		exit(1);
	}

	int cnt = 0;
	int ret;
	int len = strlen(pid_str);

	while(cnt < len){
		if((ret = write(fd, pid_str+cnt, len-cnt)) < 0){
			perror("write()");
			exit(1);
		}
		cnt += ret;
	}
	close(fd);

	return 0;
}

void og_daemonize(void)
{
	printf("daemon start in function %s\n", __FUNCTION__);
	pid_t pid;
	if((pid = fork()) < 0){
		perror("fork()");
		exit(1);
	}
	if(0 != pid){
		exit(0);
	}

	setsid();

	//close(0);
	//close(1);
	//close(2);

	//int fd;
	//dup2(0, fd);
	//dup2(1, fd);
	//dup2(2, fd);
}
