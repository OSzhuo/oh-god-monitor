#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>

#include "og_tree.h"
/*for test*/
#include <unistd.h>
#include <stdio.h>

/*this _og_head is used for in memory*/
typedef struct _og_head_t {
	og_head		*file_head;	/*file head mapped into memory*/
	uint32_t	page_wrt;	/*now pageN used for write*/
	uint32_t	page_read;	/*now pageN used for read*/
	void		*wrt_p;		/*pointer to the mapped page*/
	void		*read_p;	/*pointer to the mapped page*/
	int32_t		fd;
	char		path[OG_NAME_MAX+1];	/*tree file path*/
	//uint32_t	used_size;	/*length already use in file*/
} _og_head;

static _og_head *handlers[OG_HANDLER_CNT];
static int _mem_page_size = 0;

int _get_next_handler(void);
void *_mmap_pageN(int fd, int n);

int og_init(const char *path, int size)
{
	if(strlen(path) > OG_NAME_MAX)
		return -1;
	printf("this \n");

	int handler;
	int fd;
	og_head *map;
	void *page;
	_og_head *head;

	if((handler = _get_next_handler()) < 0)
		return -1;

	if((fd = open(path, O_CREAT|O_TRUNC|O_RDWR, 0644)) < 0){
		perror("open()");
		return -1;
	}

	if(!_mem_page_size){
		if((_mem_page_size = sysconf(_SC_PAGE_SIZE)) < 0)
			_mem_page_size = OG_DFT_MEM_PAGE;//if err, set to DEFAULT
	}

	if(posix_fallocate(fd, 0, _mem_page_size + OG_PAGE_SIZE)){
		fprintf(stderr, "posix_fallocate() err!\n");
		perror("posix_fallocate()");
		close(fd);
		return -1;
	}

	/*map head first*/
	if(MAP_FAILED == (map = mmap(NULL, _mem_page_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0))){
		perror("mmap()");
		close(fd);
		return -1;
	}

	/*map the first(0) page*/
	if(MAP_FAILED == (page = _mmap_pageN(fd, 0))){
		perror("mmap()");
		munmap(map, _mem_page_size);
		close(fd);
		return -1;
	}

	if(NULL == (head = malloc(sizeof(_og_head)))){
		perror("malloc()");
		munmap(page, OG_PAGE_SIZE);
		munmap(map, _mem_page_size);
		close(fd);
		return -1;
	}
	map->version = OG_VERSION;
	map->page_cnt = 1;
	map->data_len = size;
	map->head_size = _mem_page_size;

	head->file_head = map;
	head->fd = fd;
	head->page_now = 0;
	head->start = page;
	strcpy(head->path, path);

	handlers[handler] = head;

	return handler;
}

int og_insert(int handler, void *data, int size)
{
	if(!handlers[handler])
		return -1;

	_next_free_node();

	return 0;
}

og_node *_next_free_node(_og_head *head, )
{
}

int _load_pageN(_og_head *head, int n, int iswrt)
{
	if(n >= head->page_cnt || n < 0)
		return -1;

	void *page = NULL;
	void *old = NULL;

	if(MAP_FAILED == (page = _mmap_pageN(head->fd, n))){
		perror("mmap()");
		return -1;
	}
	if(iswrt){
		head->page_wrt = n;
		old = head->wrt_p;
		head->wrt_p = page;
	}else{
		head->page_read = n;
		old = head->read_p;
		head->read_p = page;
	}
	munmap(head->old, OG_PAGE_SIZE);

	return 0;
}

void og_destory(int handler)
{
	if(NULL == handlers[handler]){
		return ;
	}

	_og_head *head = handlers[handler];
	handlers[handler] = NULL;

	munmap(head->start, OG_PAGE_SIZE);
	munmap(head->file_head, head->file_head->head_size);
	close(head->fd);
	unlink(head->path);
	free(head);
}

/**
 * mmap the N page and return the pointer
 * @fd the file
 * @n start from 0
 * 
 */
void *_mmap_pageN(int fd, int n)
{
	return mmap(NULL, OG_PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, \
			fd, _mem_page_size + n * OG_PAGE_SIZE);
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
