#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include <stddef.h>

#include "og_tree.h"
/*for test*/
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

/*this _og_head is used for in memory*/
typedef struct _og_head_t {
	og_head		*file_head;	/*file head mapped into memory*/
	og_node		*root;		/*tree_head in memory*/
	uint32_t	pageN_wrt;	/*now pageN used for write*/
	uint32_t	w_free_pos;
	uint32_t	pageN_read;	/*now pageN used for read*/
	void		*wrt_page;	/*pointer to the mapped page*/
	void		*read_page;	/*pointer to the mapped page*/
	int32_t		fd;
	char		path[OG_NAME_MAX+1];	/*tree file path*/
	//uint32_t	used_size;	/*length already use in file*/
} _og_head;

typedef struct _og_travel_head_t {
	og_node		*root;		/*tree_head in memory*/
	uint32_t	pageN;	/*now pageN used for write*/
	uint32_t	maxN;	/*now pageN used for write*/
	void		*tmp_page;	/*pointer to the mapped page*/
	int32_t		fd;
	//uint32_t	used_size;	/*length already use in file*/
} _og_travel_head;

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

int _insert_node(og_node *this, og_node *parent);

int _edit_data(_og_head *head, og_node *node, int (*f)(void *o, void *n, size_t size), void *data, size_t n);

int _delete_sub_tree(_og_head *head, og_node *this);
void _free_left_node(_og_head *head, og_node *this);
int _free_data_node(_og_head *head, og_pos *pos);

int _free_node(_og_head *head, og_node *this);
og_node *_get_prev_sib(og_node *this);
int _leave_tree(og_node *this);

int _load_pageN_wrt(_og_head *head, uint32_t n);
int _load_pageN_read(_og_head *head, uint32_t n);
int _load_pageN(_og_head *head, uint32_t n, int iswrt);

void _og_tree_travel(_og_travel_head *th, og_node *node, void (*func_p)(void *), int depth, int end, int root);
void _prt_one_page(void *page, int node_len, void (*func)(void *));

int og_init(const char *path, int size)
{
	if(strlen(path) > OG_NAME_MAX || size > OG_PAGE_SIZE || size < 0)
		return -1;
	printf("this \n");

	int handler;
	int fd;
	og_head *map;
	_og_head *head;
	og_node *root;
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

	if(NULL == (root = malloc(sizeof(og_node)))){
		perror("root_malloc()");
		close(fd);
		munmap(map, _mem_page_size);
		return -1;
	}
	root->l_child = root->r_sib = root->parent = NULL;

	/*map the first(0) page*/
	if(MAP_FAILED == (page_w = _mmap_pageN(fd, 0))){
		perror("mmap()");
		free(root);
		munmap(map, _mem_page_size);
		close(fd);
		return -1;
	}

	if(MAP_FAILED == (page_r = _mmap_pageN(fd, 0))){
		perror("mmap()");
		free(root);
		munmap(map, _mem_page_size);
		munmap(page_w, OG_PAGE_SIZE);
		close(fd);
		return -1;
	}

	if(NULL == (head = malloc(sizeof(_og_head)))){
		perror("malloc()");
		free(root);
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

	head->root= root;

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

int og_edit_data(int handler, og_node *node, int (*func)(void *old, void *new, size_t n), void *data, size_t n)
{
	if(!handlers[handler] || !node || !func)
		return -1;
	_og_head *head = handlers[handler];

	return _edit_data(head, node, func, data, n);
}

int _edit_data(_og_head *head, og_node *node, int (*f)(void *o, void *new, size_t n), void *data, size_t n)
{
	og_pos *pos = &node->pos;
	og_data_node *data_node;

	if(pos->page != head->pageN_read && _load_pageN_read(head, pos->page) < 0){
		return -1;
	}
	data_node = (og_data_node *)((char *)head->read_page + pos->offset);

	return f(data_node->data, data, n);
}

/**
 * insert a node by comparsion
 *
*/
og_node *og_insert_by_cmp(int handler, const void *data, int size, og_node *parent)
{
	/*wait...*/

	return 0;
}

/**
 * insert a rightmost node
 * @handler
 * @data
 * @size
 * @parent
 * return	NULL for failed, success return pointor that point to node
 * 
 */
//int og_insert_by_parent(int handler, const void *data, int size);
og_node *og_insert_by_parent(int handler, const void *data, int size, og_node *parent)
{
	if(!handlers[handler])
		return NULL;
	og_pos pos;
	_og_head *head = handlers[handler];

	//_next_free_node_w();
	if(_get_free_node_w(head, &pos)){
		printf("_get_free_node_w err!!\n");
		return NULL;
	}
	//printf("get the node free (page[%lu], offset[%lu])\n", pos.page, pos.offset);
	if(_insert_data(head, data, size, &pos) < 0){
		return NULL;
	}

	og_node *node;
	if(NULL == (node = malloc(sizeof(og_node)))){
		perror("malloc()");
		return NULL;
	}

	if(parent && !parent->parent){
		/*--- ToDo ---*/
		/*now, if err, can not free data_node*/
		printf("insert err, parent have been deleted\n");
		_free_data_node(head, &pos);
		free(node);
		return NULL;
	}

	if(NULL == parent){
printf("insert head ok !\n");
		parent = head->root;
	}
	node->pos.page = pos.page;
	node->pos.offset = pos.offset;
if(parent == head->root) printf("[Root] ");

	_insert_node(node, parent);

	return node;
}

/**
 * insert node below @parent
 * 
 *
 */
int _insert_node(og_node *this, og_node *parent)
{
printf("insert node[%p][%lu,%lu] below parent[%p][%lu, %lu]", this, this->pos.page, this->pos.offset, parent, parent->pos.page, parent->pos.offset);

	this->parent = parent;
	if(!parent->l_child){
		parent->l_child = this;
printf("at left\n");
		return 0;
	}

	this->r_sib = parent->l_child->r_sib;
	parent->l_child->r_sib = this;
printf("at left's right\n");

	return 0;
}

/**
 * get the rightmost node
 */
og_node *_get_right_node(og_node *this)
{
	if(NULL == this->r_sib)
		return this;
	return _get_right_node(this);
}

/**
 * set the node unused
 * warning:	do not check anything
 */
int _free_data_node(_og_head *head, og_pos *pos)
{
	if(pos->page != head->pageN_wrt && _load_pageN_wrt(head, pos->page) < 0){
		return -1;
	}

	char *page = head->wrt_page;

	og_data_node *this = (og_data_node *)(page + pos->offset);
	if(pos->offset + sizeof(og_data_node) > OG_PAGE_SIZE){
		return -1;
	}

	this->mask &= ~_OG_INUSE;
	head->w_free_pos = pos->offset;

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
	//if(pos->page)
	memcpy(tmp+pos->offset+offsetof(og_data_node, data), data, n);
	//printf("insert pos %u\n", pos->offset);

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

	uint32_t len = head->file_head->data_len + sizeof(og_data_node);
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
 * move node to @parent, Do Not Check if in the same handler
 */
int og_move_node(int handler, og_node *this, og_node *parent)
{
	if(!handlers[handler] || !this)
		return -1;

	_og_head *head = handlers[handler];

	if(NULL == parent){
printf("move node to root!\n");
		parent = head->root;
	}

	if(_leave_tree(this) && printf("err when leave node\n"))
		return -1;

	_insert_node(this, parent);

	return 0;
}

/**
 * Delete a node and all below it
 * 
*/
int og_delete_node(int handler, og_node *this)
{
	if(!handlers[handler])
		return -1;
printf("del pos[%p][%lu,%lu]\n", this, this->pos.page, this->pos.offset);
	_og_head *head = handlers[handler];

	_delete_sub_tree(head, this);
	_free_node(head, this);

	return 0;
}

/**
 * node leave from tree, but do not free it
 */
int _leave_tree(og_node *this)
{
	if(!this || !this->parent)
		return -1;
printf("leave node [%p][%lu,%lu]\n", this, this->pos.page, this->pos.offset);
	if(this == this->parent->l_child){
		/*this node is first child*/
		this->parent->l_child = this->r_sib;
		this->r_sib = NULL;
		return 0;
	}

	og_node *left = _get_prev_sib(this);

	if(!left)
		return -1;

	left->r_sib = this->r_sib;

	return 0;
}

/**
 * Do Not Check if this is the first child !!
 * 
*/
og_node *_get_prev_sib(og_node *this)
{
	og_node *left = this->parent->l_child;

	while(left->r_sib){
		if(this == left->r_sib)
			break;
		left = left->r_sib;
	}

	return left;
}

int _free_node(_og_head *head, og_node *this)
{
	if(!this->parent){
		/*parent is NULL, @this is root*/
		return -1;
	}

	if(_leave_tree(this) && printf("err when leave node\n"))
		return -1;
	_free_data_node(head, &this->pos);

	memset(this, 0x00, sizeof(og_node));
	free(this);
//printf("this->pos->off %lu\n", this->pos.offset);

	return 0;
}

/**
 * delete node 
 * 
 */
int _delete_sub_tree(_og_head *head, og_node *this)
{
	/*this == NULL, it means that the the upper's left is NULL*/
	if(!this->l_child){
		return 0;
	}

	while(1){
		_delete_sub_tree(head, this->l_child);
		_free_left_node(head, this);
		if(!this->l_child)
			return 0;
	}

	return 0;
}

/**
 * move @this's right node to left, then free the left
 * WARNING	this function do not check if left have sub-tree
 */
void _free_left_node(_og_head *head, og_node *this)
{
	if(this->l_child)
		return;

	og_node *tmp = this->l_child;

	this->l_child = this->l_child->r_sib;
	_free_data_node(head, &tmp->pos);
	memset(tmp, 0x00, sizeof(og_node));
	free(tmp);

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
	og_data_node *this = (og_data_node *)(page + offset);
//printf("start find with offset[%lu]\n", *off);

	while(offset + len <= OG_PAGE_SIZE){
//if(offset > 4193010)
//	printf("offset = %lu page_size %lu \n", offset, OG_PAGE_SIZE);

		if(!(this->mask & _OG_INUSE)){
			this->mask |= _OG_INUSE;
			*off = offset;
//printf("out function %s()\n", __FUNCTION__);
			return 0;
		}
		offset += len;
		this = (og_data_node *)(page + offset);
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

/**
 * 
 */
void og_tree_travel(int handler, void (*func_p)(void *))
{
	_og_head *head = handlers[handler];
	if(!head)
		return;

	_og_travel_head thead;

	if(MAP_FAILED == (thead.tmp_page = _mmap_pageN(head->fd, 0))){
		perror("_mmap_pageN()");
		return;
	}
	thead.fd = head->fd;
	thead.pageN = 0;
	thead.root = head->root;
	thead.maxN = head->file_head->page_cnt;

	//uint32_t len = head->file_head->data_len + sizeof(og_data_node);

	_og_tree_travel(&thead, head->root, func_p, 0, 0, 1);
}

void _og_tree_travel(_og_travel_head *th, og_node *node, void (*func_p)(void *), int depth, int end, int root)
{
	if(!node)
		return;
	int i;

	for(i = 0; i < depth; i++){
		if(depth-1 == i)
			printf("├── ");
		else
			printf("│   ");
	}
	if(!root)
		_prt_node(th, &node->pos, func_p);
	else
		printf("[Root]\n");
	_og_tree_travel(th, node->l_child, func_p, depth+1, 0, 0);
	_og_tree_travel(th, node->r_sib, func_p, depth, 0, 0);

	return;
}

void _prt_node(_og_travel_head *th, og_pos *pos, void (*func_p)(void *))
{
	if(pos->page >= th->maxN){
		return;
	}

	char *tmp_page = th->tmp_page;
	if(pos->page != th->pageN){
		if(MAP_FAILED == (tmp_page = _mmap_pageN(th->fd, pos->page))){
			perror("mmap_pageN()");
			return;
		}
		munmap(th->tmp_page, OG_PAGE_SIZE);
		th->pageN = pos->page;
		th->tmp_page = tmp_page;
	}

	func_p(((og_data_node *)(tmp_page+pos->offset))->data);
}

void og_travel(int handler, void (*func_p)(void *))
{
	_og_head *head = handlers[handler];
	char *tmp_page;
	int max = head->file_head->page_cnt;
	uint32_t len = head->file_head->data_len + sizeof(og_data_node);
	int i;

	for(i = 0; i < max; i++){
		if(MAP_FAILED == (tmp_page = _mmap_pageN(head->fd, i))){
			perror("_mmap_pageN()");
			continue;
		}
		//printf("this page is %d data len is %d\n", i, head->file_head->data_len);
		_prt_one_page(tmp_page, len, func_p);
		munmap(tmp_page, OG_PAGE_SIZE);
	}

	return;
}

void _prt_one_page(void *page, int node_len, void (*func)(void *))
{
	uint32_t off = 0;
	og_data_node *this = page;
	char *tmp = page;


	while(off + node_len <= OG_PAGE_SIZE){
		if(this->mask & _OG_INUSE){
			func(this->data);
			//printf("offset %u\n", off); //sleep(1);
		}
		off += node_len;
		this = (og_data_node *)(tmp + off);
	}
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

/**
 * get next unused handler
 * return 	>=0	the handler
 * 		<0	err
 */
int _get_next_handler(void)
{
	int i;

	for(i = 0; i < OG_HANDLER_CNT; i++){
		if(NULL == handlers[i])
			return i;
	}

	return -1;
}
