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
#include <errno.h>

/*this _og_head is used for in memory*/
typedef struct _og_head_t {
	og_head		*file_head;	/*file head mapped into memory*/
	uint32_t	pageN_wrt;	/*now pageN used for write*/
	uint32_t	w_free_pos;
	uint32_t	pageN_read;	/*now pageN used for read*/
	void		*wrt_page;	/*pointer to the mapped page*/
	void		*read_page;	/*pointer to the mapped page*/
	int32_t		fd;
	char		path[OG_NAME_MAX+1];	/*tree file path*/
	//uint32_t	used_size;	/*length already use in file*/
} _og_head;

static _og_head *handlers[OG_HANDLER_CNT];
static uint32_t _mem_page_size = 0;

int _get_next_handler(void);
void *_mmap_pageN(int fd, uint32_t n);
int _load_pageN(_og_head *head, uint32_t n, int iswrt);
int _get_free_node_w(_og_head *head, og_pos *pos);
int _get_free_node_append(_og_head *head, og_pos *pos, int iswrt);
int _get_free_node_in_page(char *page, uint32_t len, uint32_t *off);
int _append_new_page(int fd, uint32_t cnt);
int _allocate_page(int fd, off_t offset);
int _insert_data(_og_head *head, const void *data, size_t n, og_pos *pos);

int _load_pageN_wrt(_og_head *head, uint32_t n);
int _load_pageN_read(_og_head *head, uint32_t n);
int _load_pageN(_og_head *head, uint32_t n, int iswrt);

int og_init(const char *path, int size)
{
	if(strlen(path) > OG_NAME_MAX || size > OG_PAGE_SIZE || size < 0)
		return -1;
	printf("this \n");

	int handler;
	int fd;
	og_head *map;
	_og_head *head;
	void *page_w;
	void *page_r;

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
	if(MAP_FAILED == (page_w = _mmap_pageN(fd, 0))){
		perror("mmap()");
		munmap(map, _mem_page_size);
		close(fd);
		return -1;
	}
	if(MAP_FAILED == (page_r = _mmap_pageN(fd, 0))){
		perror("mmap()");
		munmap(map, _mem_page_size);
		munmap(page_w, OG_PAGE_SIZE);
		close(fd);
		return -1;
	}

	if(NULL == (head = malloc(sizeof(_og_head)))){
		perror("malloc()");
		munmap(page_w, OG_PAGE_SIZE);
		munmap(page_r, OG_PAGE_SIZE);
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

	head->pageN_wrt = 0;
	head->wrt_page = page_w;
	head->pageN_read = 0;
	head->read_page = page_r;

	strcpy(head->path, path);

	handlers[handler] = head;

	return handler;
}

/**
 * insert a node
 * 
 */
int og_insert(int handler, const void *data, int size)
{
	if(!handlers[handler])
		return -1;
	og_pos pos;
	_og_head *head = handlers[handler];

	//_next_free_node_w();
	if(_get_free_node_w(head, &pos)){
		printf("_get_free_node_w err!!\n");
		return -1;
	}
	//printf("get the node free (page[%lu], offset[%lu])\n", pos.page, pos.offset);
	if(_insert_data(head, data, size, &pos) < 0){
		return -1;
	}

	return 0;
}

/**
 * write data to page when insert
 * 
 */
int _insert_data(_og_head *head, const void *data, size_t n, og_pos *pos)
{
//printf("this data %s %d page %u off %u\n", data, n, pos->page, pos->offset);
	if(head->pageN_wrt != pos->page){
		/*need reload a page*/
		if(_load_pageN_wrt(head, pos->page)){
			return -1;
		}
	}
	char *tmp = head->wrt_page;
	memcpy(tmp+pos->offset, data, n);

	return 0;
}

/**
 * get free node for write
 * 
 */
int _get_free_node_w(_og_head *head, og_pos *pos)
{
	return _get_free_node_append(head, pos, 1);
}

/**
 * get the node that not used
 * 
 */
//int _get_free_node(_og_head *head, og_pos *pos, int iswrt)
//{
//	//uint32_t;
//	char *page = iswrt ? head->wrt_page : head->read_page;
//	uint32_t *page_i = iswrt ? head->pageN_wrt : head->pageN_read;
//	og_node *this = page;
//	uint32_t offset = 0;
//	//uint32_t *node_i = 0;
////		uint32_t	page;
////		uint32_t	offset;
////	og_pos		left;		/*first child*/
////	og_pos		right;		/*next sibling*/
////	og_pos		parent;		/*parent node offset*/
////	uint8_t		used;		/*1 means inuse*/
////	char		data[0];	/*the data*/
//
//
//	while(offset + len <= OG_PAGE_SIZE){
//		if(0 == this->used){
//			this->used = 1;
//			pos->page = page_i;
//			pos->offset = offset;
//			return 0;
//		}
//		offset += len;
//		this = page + offset;
//		//node_i++;
//	}
//
//	return -1;
//}

/**
 * ----------- Warning ------------
 * this function will change the current loaded page, if multi-thread...
 * --------------------------------
 * get the free node, if all pages in file can not insert one node, it will append an new page and load it
 * 
 * @head
 * @pos
 * @iswrt	1 is get node for wrt, 0 is for read
 * return	0  success
 * 		-1 failed
 * 
 */
//int _get_free_node(_og_head *head, og_pos *pos, int iswrt)
int _get_free_node_append(_og_head *head, og_pos *pos, int iswrt)
{
//printf("in function %s()\n", __FUNCTION__);
	/*read do not need get free node*/
	if(!iswrt)
		return -1;

	uint32_t len = head->file_head->data_len + sizeof(og_node);
	char *page = head->wrt_page;
	uint32_t page_i = head->pageN_wrt;
	//og_node *this = page;
	uint32_t offset = head->w_free_pos;

	/*if Not found, load next page, if no page, append the file*/
	while(_get_free_node_in_page(page, len, &offset)){
		//load next page
		page_i += 1;
		if(page_i >= head->file_head->page_cnt){
			/*no more page, to append a new page*/
			if(_append_new_page(head->fd, head->file_head->page_cnt))
				return -1;
			head->file_head->page_cnt += 1;
		}
		if(_load_pageN(head, page_i, iswrt) < 0){
			return -1;
		}
		page = head->wrt_page;
		offset = head->w_free_pos;
	}

	pos->page = page_i;
	pos->offset = head->w_free_pos = offset;

//printf("out function %s()\n", __FUNCTION__);
	return 0;
}

/**
 * allocate a new page, after the @cnt page
 * @cnt		the last page index
 * @fd		file
 * 
 */
int _append_new_page(int fd, uint32_t cnt)
{
//printf("in function %s()\n", __FUNCTION__);
	return _allocate_page(fd, _mem_page_size+OG_PAGE_SIZE*cnt);
}

int _allocate_page(int fd, off_t offset)
{
	int err;

//printf("in function %s()\n", __FUNCTION__);
	if((err = posix_fallocate(fd, offset, OG_PAGE_SIZE))){
		fprintf(stderr, "posix_fallocate():%s\n", strerror(err));
		return -1;
	}

	return 0;
}

/**
 * get the empty node from given page and set the node to inuse
 * @page	page start
 * @len		length of node+data
 * @off		point to the og_pos.offset (start find with *off, when find, set to *off)
 * return	success 0;
 * 		failed	-1
 * 
 */
int _get_free_node_in_page(char *page, uint32_t len, uint32_t *off)
{
//printf("in function %s()\n", __FUNCTION__);
	uint32_t offset = *off;
	og_node *this = (og_node *)(page + offset);
//printf("start find with offset[%lu]\n", *off);

	while(offset + len <= OG_PAGE_SIZE){
//if(offset > 4193010)
//	printf("offset = %lu page_size %lu \n", offset, OG_PAGE_SIZE);

		if(0 == this->used){
			this->used = 1;
			*off = offset;
//printf("out function %s()\n", __FUNCTION__);
			return 0;
		}
		offset += len;
		this = (og_node *)(page + offset);
	}

printf("err out function %s(): need append new page\n", __FUNCTION__);
	return -1;
}

/**
 * load the N page for write and unload the previous page
 * @head	head in mem
 * @n		index
 * @RETURN	0 for success, -1 error
 * 
 */
int _load_pageN_wrt(_og_head *head, uint32_t n)
{
	if(n >= head->file_head->page_cnt || n < 0)
		return -1;
	return _load_pageN(head, n, 1);
}

/**
 * load the N page for read and unload the previous page
 * @head	head in mem
 * @n		index
 * @RETURN	0 for success, -1 error
 * 
 */
int _load_pageN_read(_og_head *head, uint32_t n)
{
	if(n >= head->file_head->page_cnt || n < 0)
		return -1;
	return _load_pageN(head, n, 0);
}

/**
 * load the N page and unload the previous page
 * @head	head in mem
 * @n		index
 * @iswrt	load page for wrt or for read
 * @RETURN	0 for success, -1 error
 * 
 */
int _load_pageN(_og_head *head, uint32_t n, int iswrt)
{
	void *page = NULL;
	void *old = NULL;

	if(MAP_FAILED == (page = _mmap_pageN(head->fd, n))){
		perror("mmap()");
		return -1;
	}
	if(iswrt){
		head->pageN_wrt = n;
		old = head->wrt_page;
		head->wrt_page = page;
		head->w_free_pos= 0;
	}else{
		head->pageN_read = n;
		old = head->read_page;
		head->read_page = page;
	}
	munmap(old, OG_PAGE_SIZE);

	return 0;
}

void og_destory(int handler)
{
	if(NULL == handlers[handler]){
		return ;
	}

	_og_head *head = handlers[handler];
	handlers[handler] = NULL;

	munmap(head->wrt_page, OG_PAGE_SIZE);
	munmap(head->read_page, OG_PAGE_SIZE);
	munmap(head->file_head, head->file_head->head_size);
	close(head->fd);
	unlink(head->path);
	free(head);
}

void og_travel(void (*func_p)(void *), _og_head *head)
{

	return 0;
}

/**
 * mmap the N page and return the pointer
 * @fd the file
 * @n start from 0
 * 
 */
void *_mmap_pageN(int fd, uint32_t n)
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
