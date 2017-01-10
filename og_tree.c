#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>

#include "og_tree.h"
/*for test*/
#include <unistd.h>
#include <stdio.h>

/*this _og_head is used for in memory*/
typedef struct _og_head_t {
	void		*start;		/*pointer to the mapped area,returned by mmap() or mremap()*/
	uint32_t	data_len;	/*the length of data filed in og_node*/
	uint32_t	used_size;	/*length already use in file*/
	uint32_t	page_cnt;	/*how many pages in this file*/
	int32_t		fd;
	char		path[OG_NAME_MAX+1];	/*tree file path*/
} _og_head;

static _og_head *handlers[OG_HANDLER_CNT];

int _get_next_handler(void);

int og_tree_init(const char *path, int size)
{
	int handler;
	int fd;
	char *map;

	if((handler = _get_next_handler()) < 0)
		return -1;

	if((fd = open(path, O_CREAT|O_TRUNC|O_RDWR, 0644)) < 0){
		perror("open()");
		return -1;
	}

	if(posix_fallocate(fd, 0, sizeof(og_head)+OG_PAGE_SIZE)){
		fprintf(stderr, "posix_fallocate() err!\n");
		//perror("posix_fallocate()");
		return -1;
	}

	if(MAP_FAILED == (map = mmap(NULL, OG_MAP_LEN, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0))){
		perror("mmap()");
		return -1;
	}
//fprintf(stdout, "lseek get size %d map size if %lu\n", lseek(fd, 0, SEEK_END), OG_MAP_LEN);

	_og_head *head;

	if(NULL == (head = malloc(sizeof(og_head)))){
		perror("malloc()");
		return -1;
	}
sleep(1000);

	//handlers[handler];

	return handler;
}

int _get_next_handler(void)
{
	int i;

	for(i = 0; i < OG_HANDLER_CNT; i++){
		if(NULL == handlers[i])
			return i;
	}

	return -1;
}
