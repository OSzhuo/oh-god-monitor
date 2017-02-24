#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include "og_record.h"
#include "og_stack.h"
#include "og_tree.h"
#include "obuf.h"
/* .. */
#include "og_watch.h"

/*
 * used for init, it's easy to find tree node's parent
 */
typedef struct _dir_init_st {
//	int		wd;		/* if wd<0, this unit must not be DIR */
	int		len;		/* the path length() (include '\0') */
	ogt_node	*this;		/* this node's position */
	char		path[0];
} _dir_init;

/* TODO 
 * now:Singly linked list ==> rb-tree
 * link a watched directory's wd and its tree node
 * speed up find the file when get an event
 */
typedef struct _watch_list_node_st {
	struct _watch_list_node_st	*next;
	ogt_node			*this;
	int				wd;		/* unique in list */
} _watch_list_node;

static int		glb_bd;
static int		glb_watch_fd;
static int		glb_ogt_handle;
static ogs_head		*glb_path_stack;	/* this stack is used for store last used parent when init */
/* TODO  add a data in stack, it is the full path, to compare with new file's parent wd */
static ogs_head		*glb_name_stack;	/* this stack store the full path */
static og_file_unit	*pub_data;
static _watch_list_node	*wlist_head;

int read_one_unit(int bd, _og_unit *buf);
int _process_unit(_og_unit *buf);
int _push_path(_og_unit *buf, ogt_node *node);

ogt_node*_get_tree_parent(_og_unit *buf);
int _work_init(_og_unit *buf);

int _read_obuf(int bd, void *buf, size_t count);
void *_init_file_unit(void);
void _prt_unit(void *data, void *node);
void _travel_(void *data);

int _work_new(_og_unit *buf);
int _work_del(_og_unit *buf);

int _find_node_by_name(void *data, void *path);

int _get_full_path(_watch_list_node *wlist, int wd, ogs_head *stack_head, char *path, ogt_node **node);
int _clean_stack(ogs_head *head);
int __read_full_path(ogs_head *head, char *path);
int __push_parent_name(ogt_node *this);

static int _push_name(void *o, void *n, size_t size);

int _wlist_init(_watch_list_node **head);
int _wlist_add(_watch_list_node *head, ogt_node *node, int wd);
_watch_list_node *_wlist_node_init(void);
_watch_list_node *_wlist_search(const _watch_list_node *head, int wd);
int _wlist_is_empty(const _watch_list_node *head);
int __wlist_add(_watch_list_node *head, ogt_node *node, int wd);

/*if file exsit, will trunc!!!*/
int og_record_init(int bd, int watch_fd)
{
	glb_bd = bd;
	glb_watch_fd = watch_fd;

	_wlist_init(&wlist_head);

	if(!(glb_name_stack = ogs_init())){
		fprintf(stderr, "init name stack err!\n");
		return -1;
	}

	if((glb_ogt_handle = ogt_init(OG_TREE_FILE, sizeof(og_file_unit)+IBIG_NM_MAX) < 0)){
		fprintf(stderr, "ogt_init(): err!\n");
		ogs_destory(glb_name_stack, free);
		return -1;
	}

	if(!(pub_data = _init_file_unit())){
		fprintf(stderr, "_init_file_unit(): err!\n");
		ogt_destory(glb_ogt_handle);
		ogs_destory(glb_name_stack, free);
		return -1;
	}

	return 0;
}

/**
 * just for init, init the stack to stor the last directory
 * 
 */
int og_init_start(void)
{
	ogs_head *head;

	head = ogs_init();
	glb_path_stack = head;

	return 0;
}

/**
 * init ok, destory the stack
 */
int _init_over(void)
{
	ogs_destory(glb_path_stack, free);
	glb_path_stack = NULL;
printf("destory directory stack\n");

	return 0;
}

int _trunc_path(_og_unit *buf, char *head)
{
	size_t len = strlen(head);
	if(strncmp(buf->path, head, len))
		return -1;

	buf->len -= len;
	memmove(buf->path, buf->path+len, buf->len);

	return 0;
}

void *og_record_work(void *x)
{
	_og_unit *p;

	if(NULL == (p = malloc(sizeof(_og_unit) + IBIG_PATH_MAX))){
		perror("malloc");
		exit(1);
	}

	while(1){
		//sleep(1);
//printf("want read %d read %d bytes,", sizeof(struct list_line_t), obuf_read((int)bd, p, sizeof(struct list_line_t) + 4096, 1, NULL));
//printf("want read %d read %d bytes,", sizeof(struct list_line_t), obuf_read((int)bd, p, sizeof(struct list_line_t), 1, NULL));
//printf("want %d read %d bytes\n", p->len, obuf_read((int)bd, p->path, p->len, 1, NULL));
		read_one_unit(glb_bd, p);
		_process_unit(p);
	}

	return NULL;
}

int _process_unit(_og_unit *buf)
{
	//printf("act : %d; new %d del %d\n", buf->action, ACT_NEW, ACT_DEL);
	switch(buf->action){
	case ACT_INIT:
#if DEBUG >= 8
//printf("INIT path[%-52s][len:%d][%c][%d]\n", buf->path, buf->len, buf->type, buf->err);
#endif
		if(_work_init(buf)){
			fprintf(stderr, "init file err\n");
			return -1;
		}
		break;
	case ACT_NEW:
#if DEBUG >= 8
printf("NEW path[%-28s][len:%d][%c][%d][wd:%3d]\n", buf->path, buf->len, buf->type, buf->err, buf->wd);
#endif
		_work_new(buf);
		break;
	case ACT_DEL:
#if DEBUG >= 8
printf("DEL path[%-28s][len:%d][%c][%d]\n", buf->path, buf->len, buf->type, buf->err);
#endif
		_work_del(buf);
ogt_tree_travel(glb_ogt_handle, _travel_);
		break;
	case ACT_MV_D:
#if DEBUG >= 8
		printf("MOVE path[%-27s][len:%d] to [%-27s][len:%d] [%c][%d]\n", buf->path, buf->len, buf->path+buf->len, buf->len_t, buf->type, buf->err);
#endif
		//(RUNTIME_LIST, buf);
//		break;
	case ACT_INIT_OK:
#if DEBUG >= 8
		printf("[INIT OVER] start destory directory stack\n");
#endif
		_init_over();
ogt_tree_travel(glb_ogt_handle, _travel_);
		break;
	default:
		printf("Unknown action!\n");
		break;
	}

	return 0;
}

//typedef struct _og_unit_t{
//	int8_t	action;		
//	int8_t	err;		
//	char	type;		
//	off_t	size;		
//	time_t	mtime;		
//	int	wd;		
//	int16_t	len;		
//	int16_t	base;		
//	char	path[];		
//} _og_unit;

//typedef struct og_file_unit_st {
//	int8_t	err;		/* if file access err */
//	char	type;		/* file type */
//	int16_t	len;		/* the path length() (include '\0') */
//	off_t	size;		/* total size, in bytes */
//	time_t	mtime;		/* time of last modification */
//	int	wd;		/* if wd<0, this unit must not be DIR */
//	char	name[];		/* file name (include nul('\0')) */
//} og_file_unit;

/**
 * Do not check whether glb_path_stack is NULL
 * if can not find parent, insert node to tree by parent = NULL,
 * 				and name stored full path,
 * 				just for the first node, "/tmp/mnt/USB-disk-1"
 */
int _work_init(_og_unit *buf)
{
#if DEBUG >=9
	//printf("%s, this path %s \n", __FUNCTION__, buf->path);
	buf->path[buf->base_or_selfwd-1] = '\0';
	if(TYPE_D == buf->type){
		printf("TRAVEL STACK=======\n");
		ogs_travel(glb_path_stack, _prt_unit);
		printf("===================\n");
	}
#endif

	buf->path[buf->base_or_selfwd-1] = '\0';
	ogt_node *this;
	ogt_node *parent = _get_tree_parent(buf);

	pub_data->err	= buf->err;
	pub_data->type	= buf->type;
	pub_data->size	= buf->size;
	pub_data->mtime	= buf->mtime;
	pub_data->wd	= buf->wd;

	buf->path[buf->base_or_selfwd-1] = '/';
	/* if there no data in dir_stack */
	if(!parent){
		pub_data->len = buf->len;
		strcpy(pub_data->name, buf->path);
		//pub_data->name[buf->base-1] = '\0';
	}else{
		pub_data->len = buf->len - buf->base_or_selfwd;
		strcpy(pub_data->name, buf->path + buf->base_or_selfwd);
	}
//printf("[%s][%c] st_size[%8ld] mtime[%ld] name[%s] full[%s]\n", __FUNCTION__, pub_data->type, pub_data->size, pub_data->mtime, pub_data->name);

	if(!(this = ogt_insert_by_parent(glb_ogt_handle, pub_data, offsetof(og_file_unit, name)+pub_data->len, parent))){
		fprintf(stderr, "insert node err.\n");
		return -1;
	}

	if(TYPE_D == buf->type){
		_wlist_add(wlist_head, this, buf->wd);
		if(_push_path(buf, this) < 0)
			return -1;
	}
//printf("[%s]insert node [%p][%u,%u]\n", __FILE__, this, this->pos.page, this->pos.offset);

	return 0;
}

/**
 * get CREATE
 */
int _work_new(_og_unit *buf)
{
	char path[IBIG_PATH_MAX];
	struct stat status;
	int wd = -1;
	ogt_node *parent;
	ogt_node *this;

	_get_full_path(wlist_head, buf->wd, glb_name_stack, path, &parent);
	printf("get full path[%s]\n", path);
	strcat(path, buf->path);
	printf("get full name[%s]\n", path);

	if(TYPE_D == buf->type && (wd = og_add_watch(glb_watch_fd, path)) < 0){
		fprintf(stderr, "[%s]add watch failed\n", path);
		return -1;
	}

	if(stat(path, &status) < 0){
		perror("stat()");
		buf->err = 1;
		status.st_size = 0;
		status.st_mtime = 0;
	}

	pub_data->err	= buf->err;
	pub_data->type	= buf->type;
	pub_data->size	= status.st_size;
	pub_data->mtime	= status.st_mtime;
	pub_data->wd	= wd;
	strcpy(pub_data->name, buf->path);
	pub_data->len	= buf->len;

//printf("[%s][%c] st_size[%8ld] mtime[%ld] name[%s] len[%d]\n", __FUNCTION__, pub_data->type, pub_data->size, pub_data->mtime, pub_data->name, pub_data->len);

	if(!(this = ogt_insert_by_parent(glb_ogt_handle, pub_data, offsetof(og_file_unit, name)+pub_data->len, parent))){
		fprintf(stderr, "insert node err.\n");
		return -1;
	}

	if(TYPE_D == buf->type){
		_wlist_add(wlist_head, this, wd);
	}
//ogt_tree_travel(glb_ogt_handle, _travel_);
//printf("[%s]insert node [%p][%u,%u]\n", __FILE__, this, this->pos.page, this->pos.offset);

	return 0;
}

/**
 * get ACT_DEL
 */
int _work_del(_og_unit *buf)
{
//char path[IBIG_PATH_MAX];
//struct stat status;
	int wd = -1;
	ogt_node *parent;
	ogt_node *this;

	_watch_list_node *w_this;

	if(!(w_this = _wlist_search(wlist_head, buf->wd)) || !(parent = w_this->this)){
		fprintf(stderr, "can not find wd[%d] in watch list\n", wd);
		return -1;
	}
	//if(!(parent = _wlist_search(wlist_head, buf->wd))){
	//	/* need add function to find in all file_node */
	//	return -1;
	//}
//printf("this ??\n");
//printf("[%s][%c] name[%s] len[%d]\n", __FUNCTION__, pub_data->type, pub_data->name, pub_data->mtime, pub_data->name, pub_data->len);

	if(!(this = ogt_get_node_by_parent(glb_ogt_handle, parent, &_find_node_by_name, buf->path))){
		fprintf(stderr, "can not find path[%s] below parent[%p] \n", buf->path, parent);
		return -1;
	}
//printf("[%s][%c] st_size[%8ld] mtime[%ld] name[%s] len[%d]\n", __FUNCTION__, pub_data->type, pub_data->size, pub_data->mtime, pub_data->name, pub_data->len);

//int ogt_delete_node(int handle, ogt_node *this);
	if(ogt_delete_node(glb_ogt_handle, this) < 0){
		fprintf(stderr, "delete node err.\n");
		return -1;
	}

//	if(TYPE_D == buf->type){
//		_wlist_set_null(wlist_head, this, wd);
//	}
//printf("[%s]insert node [%p][%u,%u]\n", __FILE__, this, this->pos.page, this->pos.offset);

	return 0;
}

/*
 * for func:	return 0 means stop and return this node
 * 		return others means continue
 */
int _find_node_by_name(void *data, void *path)
{
	og_file_unit *this = data;
//printf("this [%s]  VS [%s]\n", this->name, path);
	return strcmp(this->name, path);
}

int _clean_stack(ogs_head *head)
{
	char *path;

	while(NULL != (path = ogs_pop(head))){
		free(path);
	}

	return 0;
}

/**
 * TODO ==> add path size check
 * 
 */
int _get_full_path(_watch_list_node *wlist, int wd, ogs_head *stack_head, char *path, ogt_node **node)
{
	/* clean the path stack */
	_clean_stack(stack_head);
	*node = NULL;

	ogt_node *this;
	_watch_list_node *w_this;

	if(!(w_this = _wlist_search(wlist, wd))){
		fprintf(stderr, "can not find wd[%d] in watch list\n", wd);
		return -1;
	}
	this = w_this->this;

	if(__push_parent_name(this)){
		return -1;
	}

	__read_full_path(stack_head, path);

	*node = this;

	return 0;
}

/**
 * pop stack to get full path
 */
int __read_full_path(ogs_head *head, char *path)
{
	char *name;
	path[0] = '\0';

	while(NULL != (name = ogs_pop(head))){
		strcat(path, name);
		strcat(path, "/");
		free(name);
	}
	//path[strlen(path)-1] = '\0';

	return 0;
}

/*
 * push parent node's parent's name recursively, include @this self
 * @this	start with this node
 * 
 */
int __push_parent_name(ogt_node *this)
{
	if(!this){
		return 0;
	}

	if(ogt_edit_data(glb_ogt_handle, this, &_push_name, NULL, 0)){
		fprintf(stderr, "ogt_edit_data()err, may be push name err! \n");
		return -1;
	}

	return __push_parent_name(ogt_parent(this));
}

/**
 * this function called in ogt_edit_data()
 */
int _push_name(void *o, void *n, size_t size)
{
	og_file_unit *data = o;
	char *name;

	if(!(name = strdup(data->name))){
		fprintf(stderr, "strdup() err !\n");
		return -1;
	}

	if(ogs_push(glb_name_stack, name) < 0){
		fprintf(stderr, "strdup() err !\n");
		return -1;
	}

	return 0;
}

/*
 * 
 */
int _push_path(_og_unit *buf, ogt_node *node)
{
	buf->path[buf->base_or_selfwd-1] = '/';
	_dir_init *this;

	if(!(this = malloc(offsetof(_dir_init, path)+buf->len))){
		perror("malloc()");
		return -1;
	}

	this->this = node;
	this->len = buf->len;
	strcpy(this->path, buf->path);

//printf("push dir[%p][%s]\n", this->this, this->path);
//printf("[MALLOC]push dir[%p][%s], size %d, len %d\n", this, this->path,sizeof(_dir_init)+buf->len, buf->len);
	if(ogs_push(glb_path_stack, this) < 0){
		fprintf(stderr, "push to stack err\n");
		return -1;
	}

	return 0;
}

void _travel_(void *data)
{
	og_file_unit *d = data;

	printf("[%c][wd:%2d][len:%d]name: %-32s\n", d->type, d->wd, d->len, d->name);
}

/**
 * get the parent node(directory) in tree
 */
ogt_node*_get_tree_parent(_og_unit *buf)
{
	_dir_init *this;

	while(1){
		this = ogs_top(glb_path_stack);
		if(!this){
			return NULL;
		}
//printf("want [%s]top dir[%p][%s], strlen %lu len %d\n", buf->path, this, this->path, strlen(this->path), this->len);
		if(strcmp(buf->path, this->path)){
			//printf("[FREE]this %p pop %p\n", this, ogs_pop(glb_path_stack));
			//free(this);
			free(ogs_pop(glb_path_stack));
			//ogs_pop(glb_path_stack);
			continue;
		}
			break;
	}

	return this->this;
}

/**
 * malloc space for unit in file
 */
void *_init_file_unit(void)
{
	og_file_unit *this;

	if(NULL == (this = malloc(sizeof(og_file_unit)+IBIG_NM_MAX))){
		perror("malloc()");
	}

	return this;
}


/*
 * read one unit from obuf
 * 
 */
int read_one_unit(int bd, _og_unit *buf)
{
	_read_obuf(bd, buf, offsetof(_og_unit, path));
	_read_obuf(bd, buf->path, buf->len);
//	printf("[%s][%c] st_size[%8ld] mtime[%ld] path[%s]\n", __FUNCTION__, buf->type, buf->size, buf->mtime, buf->path);

	return 0;
}

int _read_obuf(int bd, void *buf, size_t count)
{
	int c = 0;
	while(c < count)
		c += obuf_read(bd, buf + c, count - c, 1, NULL);

	return count;
}

/*
 * used for travel the parent path stack in when init all file
 */
void _prt_unit(void *data, void *node)
{
        _dir_init *this = data;

        if(!data)
                printf("{%p ++%p++}\n", this, node);
        else
                printf("{%p ++%p++ %d %s}\n", this, node, this->len, this->path);
}

/*
 * init watch list(link wd to tree node)
 */
int _wlist_init(_watch_list_node **head)
{
	if(!(*head = _wlist_node_init())){
		return -1;
	}

	return 0;
}

int _wlist_add(_watch_list_node *head, ogt_node *node, int wd)
{
//printf("[%s] add node [wd:%2d]\n", __FUNCTION__, wd);
	_watch_list_node *this = _wlist_search(head, wd);

	if(this){
		this->this = node;
		this->wd = wd;
		return 0;
	}

	return __wlist_add(head, node, wd);
}

int __wlist_add(_watch_list_node *head, ogt_node *node, int wd)
{
	_watch_list_node *this;

	if(!(this = _wlist_node_init())){
		return -1;
	}

	this->next = head->next;
	this->this = node;
	this->wd = wd;

	head->next = this;

	return 0;
}

//int _wlist_set_null(_watch_list_node *head, ogt_node *node, int wd)
//{
////printf("[%s] add node [wd:%2d]\n", __FUNCTION__, wd);
//	_watch_list_node *this = _wlist_search(head, wd);
//
//	if(this){
//		this->this = node;
//		this->wd = wd;
//		return 0;
//	}
//}

_watch_list_node *_wlist_search(const _watch_list_node *head, int wd)
{
	if(_wlist_is_empty(head))
		return NULL;

	_watch_list_node *this = head->next;

	while(this){
		//printf("[FIND] [wd:%2d] cmpare with [%d]\n", this->wd, wd);
		if(wd == this->wd){
			return this;
		}
		this = this->next;
	}

	return NULL;
}

int _wlist_is_empty(const _watch_list_node *head)
{
	return NULL == head->next;
}

//int _wlist_remove_by_wd(_watch_list_node *head, ogt_node *node, int wd)
//{
//	_watch_list_node *this;
//
//	if(!(this = _wlist_node_init())){
//		return -1;
//	}
//
//	this->next = head->next;
//	this->this = node;
//
//	head->next = this;
//
//	return 0;
//}

/*
 * init a watch list node
 */
_watch_list_node *_wlist_node_init(void)
{
	_watch_list_node *this;

	if(!(this = malloc(sizeof(_watch_list_node)))){
		perror("malloc()");
		return NULL;
	}

	this->next = NULL;
	this->this = NULL;
	this->wd = -1;

	return this;
}

//int _write_all(int fd, const void *buf, size_t size)
//{
//	int wrt = 0;
//	int r;
//
//	while(wrt < size){
//		if((r = write(glb_fd, buf+wrt, size - wrt)) < 0){
//			perror("write()");
//			return -1;
//		}
//		wrt += r;
//	}
//
//	return wrt;
//}
