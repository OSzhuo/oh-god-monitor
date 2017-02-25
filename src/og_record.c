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
#include "og_tool.h"
#include "og_tree.h"
#include "obuf.h"

/* .just for add a watch when get ACT_NEW. */
#include "og_watch.h"

/*
 * ============== define functions ===============
 */

/*
 * used for init, it's easy to find tree node's parent
 */
typedef struct _dir_init_st {
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

struct __move_node_t {
	int	wd;		/*parent's wd*/
	char	name[IBIG_NM_MAX];
};

/*
 * used for connect ACT_MV_F and ACT_MV_T actions
 */
typedef struct _file_move_node_st {
	struct __move_node_t	from;
	struct __move_node_t	to;
	int32_t			cookie;
} _file_move_node;

/*
 * =========== define global variables ============
 */
static int		glb_in_init;		/* if is in init */
static int		glb_bd;			/* the obuf handle */
static int		glb_watch_fd;		/* watched fd, opened by inotify_init() */
static int		glb_ogt_handle;		/* tree handle */
static ogs_head		*glb_path_stack;	/* this stack is used for store last used parent when init */
/* TODO  add a data in stack, it is the full path, to compare with new file's parent wd */
static ogs_head		*glb_name_stack;	/* this stack store the full path */
static og_file_unit	*pub_data;		/* public file node */
static _watch_list_node	*wlist_head;		/* watch descriptor list head */
static _file_move_node	pub_move_list[_TMP_MOVE_CNT];	/* tmp */

/*
 * ============== define functions ===============
 */

/* read events and process them */
static int _read_one_unit(int bd, _og_unit *buf);
static int _read_obuf(int bd, void *buf, size_t count);
static int _process_unit(_og_unit *buf);
static int _work_init(_og_unit *buf);
static int _work_new(_og_unit *buf);
static int _work_modify(_og_unit *buf);
static int _work_del(_og_unit *buf);

/* used in initialization (read all file list) */
static int _init_start(void);
static int _init_over(void);
static int _push_path(_og_unit *buf, ogt_node *node);
static ogt_node *_get_tree_parent(_og_unit *buf);

/* tmp move node */
static int _work_move(_og_unit *buf, int from);
static int __work_move(_og_unit *buf, int from);
static int __connect_match_move_node(_file_move_node *mv_node);
static void __clean_move_node(_file_move_node *node);
static _file_move_node *_get_free_move_node(void);
static _file_move_node *_find_move_node(uint32_t cookie);

static int _node_cmp_by_name(void *data, void *path);

/* update file node info */
static int _update_file_node_stat(ogt_node *this, struct stat *status);
/* └──────➔ ogt_edit_data() ─➔────┐ */
static int __update_stat(void *old, void *new, size_t n);
static int _update_file_node_name(ogt_node *this, char *newname);
/* └──────➔ ogt_edit_data() ─➔────┐ */
static int __update_name(void *old, void *new, size_t n);

/* get full path with tree opts */
static int _get_full_path(_watch_list_node *wlist, int wd, ogs_head *stack_head, char *path, ogt_node **node);
static int _clean_stack(ogs_head *head);
static int __read_full_path(ogs_head *head, char *path);
static int __push_parent_name(ogt_node *this);
static int _push_name(void *o, void *n, size_t size);

/* opts for watched wd list */
static int _wlist_init(_watch_list_node **head);
static int _wlist_add(_watch_list_node *head, ogt_node *node, int wd);
static int __wlist_add(_watch_list_node *head, ogt_node *node, int wd);
static int _wlist_is_empty(const _watch_list_node *head);
static _watch_list_node *_wlist_search(const _watch_list_node *head, int wd);
static _watch_list_node *_wlist_node_init(void);

/* init the pub_data */
static void *_init_file_unit(void);

/* debug travel */
void _prt_unit(void *data, void *node);
void _travel_(void *data);


/*
 * =============== define end ================
 * ============== coding start ===============
 */


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

/*
 * just for init, init the stack to stor the last directory
 * 
 */
int og_init_start(void)
{
	return _init_start();
}

int _init_start(void)
{
	ogs_head *head;

	if(!(head = ogs_init())){
		fprintf(stderr, "ogs_init failed\n");
		return -1;
	}
	glb_path_stack = head;
	glb_in_init = 1;

	return 0;
}

/*
 * init ok, destory the stack
 */
int _init_over(void)
{
	ogs_destory(glb_path_stack, free);
	glb_path_stack = NULL;
	glb_in_init = 0;
printf("init ok, destory directory stack\n");

	return 0;
}

//int _trunc_path(_og_unit *buf, char *head)
//{
//	size_t len = strlen(head);
//	if(strncmp(buf->path, head, len))
//		return -1;
//
//	buf->len -= len;
//	memmove(buf->path, buf->path+len, buf->len);
//
//	return 0;
//}

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
		_read_one_unit(glb_bd, p);
		_process_unit(p);
	}

	return NULL;
}

int _process_unit(_og_unit *buf)
{
	switch(buf->action){
	case ACT_INIT:
#if DEBUG >= 8
		printf("INIT path[%-28s][len:%d][%c][%d]\n", buf->path, buf->len, buf->type, buf->err);
#endif
		if(_work_init(buf)){
			fprintf(stderr, "init file err\n");
			return -1;
		}
		break;
	case ACT_NEW:
#if DEBUG >= 8
		printf("NEW path[%-28s][len:%d][%c][%d][wd:%3d]\n", buf->path, buf->len, buf->type, buf->err, buf->wd);
		ogt_tree_travel(glb_ogt_handle, _travel_);
#endif
		_work_new(buf);
		break;
	case ACT_DEL:
		_work_del(buf);
#if DEBUG >= 8
		printf("DEL path[%-28s][len:%d][%c][%d]\n", buf->path, buf->len, buf->type, buf->err);
		ogt_tree_travel(glb_ogt_handle, _travel_);
#endif
		break;
	case ACT_MV_F:
		_work_move(buf, 1);
#if DEBUG >= 8
		printf("MOVE FROM path[%-28s][len:%d][%c][wd:%d]\n", buf->path, buf->len, buf->type, buf->wd);
		ogt_tree_travel(glb_ogt_handle, _travel_);
#endif
		break;
	case ACT_MV_T:
		_work_move(buf, 0);
#if DEBUG >= 8
		printf("MOVE TO path[%-28s][len:%d][%c][wd:%d]\n", buf->path, buf->len, buf->type, buf->wd);
		ogt_tree_travel(glb_ogt_handle, _travel_);
#endif
		break;
	case ACT_MODIFY:
		_work_modify(buf);
#if DEBUG >= 8
		printf("MODIFY path[%-28s][len:%d][%c][wd:%d]\n", buf->path, buf->len, buf->type, buf->wd);
		ogt_tree_travel(glb_ogt_handle, _travel_);
#endif
		break;
	case ACT_INIT_OK:
		_init_over();
#if DEBUG >= 8
		printf("[INIT OVER] start destory directory stack\n");
		ogt_tree_travel(glb_ogt_handle, _travel_);
#endif
		break;
	default:
		printf("Unknown action!\n");
		break;
	}

	return 0;
}

/*
 * Do not check whether glb_path_stack is NULL
 * if can not find parent, insert node to tree by parent = NULL,
 * 				and name stored full path,
 * 				just for the first node, "/tmp/mnt/USB-disk-1"
 */
int _work_init(_og_unit *buf)
{
#if DEBUG >=9
	//printf("%s, this path %s \n", __FUNCTION__, buf->path);
	buf->path[buf->base_or_cookie-1] = '\0';
	if(TYPE_D == buf->type){
		printf("TRAVEL STACK=======\n");
		ogs_travel(glb_path_stack, _prt_unit);
		printf("===================\n");
	}
#endif

	buf->path[buf->base_or_cookie-1] = '\0';
	ogt_node *this;
	ogt_node *parent = _get_tree_parent(buf);

	pub_data->err	= buf->err;
	pub_data->type	= buf->type;
	pub_data->size	= buf->size;
	pub_data->mtime	= buf->mtime;
	pub_data->wd	= buf->wd;

	buf->path[buf->base_or_cookie-1] = '/';
	/* if there no data in dir_stack */
	if(!parent){
		pub_data->len = buf->len;
		strcpy(pub_data->name, buf->path);
		//pub_data->name[buf->base-1] = '\0';
	}else{
		pub_data->len = buf->len - buf->base_or_cookie;
		strcpy(pub_data->name, buf->path + buf->base_or_cookie);
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

/*
 * get MOVE
 * @from	1 move form
 * 		0 move to
 */
int _work_move(_og_unit *buf, int from)
{
	return __work_move(buf, from);
}

/*
 * get ACT_MV_T/F
 */
int __work_move(_og_unit *buf, int isfrom)
{
	_file_move_node *mv_node;
	uint32_t cookie = buf->base_or_cookie;

	if(!(mv_node = _find_move_node(cookie)) && !(mv_node = _get_free_move_node())){
		fprintf(stderr, "can not get node where cookie = %u and there no temp move node\n", cookie);
		return -1;
	}

	if(isfrom){
		strcpy(mv_node->from.name, buf->path);
		mv_node->from.wd = buf->wd;
	}else{
		strcpy(mv_node->to.name, buf->path);
		mv_node->to.wd = buf->wd;
	}
	mv_node->cookie = cookie;

	/* check if move from and move to is complete */
	if(mv_node->from.name[0] == '\0' || mv_node->to.name[0] == '\0'){
		fprintf(stdout, "cookie[%u] have not get the other half\n", mv_node->cookie);
		return 0;
	}

	__connect_match_move_node(mv_node);

	__clean_move_node(mv_node);

	return 0;
}

/*
 * move 'from' and 'to' cookie match !!!
 */
int __connect_match_move_node(_file_move_node *mv_node)
{
	/* IN_MOVE_FROM and IN_MOVED_TO all get */
	_watch_list_node *w_this;
	ogt_node *to_parent;
	ogt_node *from_parent;
	ogt_node *from;

	/* find to's parent */
	if(!(w_this = _wlist_search(wlist_head, mv_node->to.wd)) || !(to_parent = w_this->this)){
		fprintf(stderr, "can not find wd[%d] in watch list\n", mv_node->from.wd);
		return -1;
	}

	/* find from's parent */
	if(!(w_this = _wlist_search(wlist_head, mv_node->from.wd)) || !(from_parent = w_this->this)){
		fprintf(stderr, "can not find wd[%d] in watch list\n", mv_node->to.wd);
		return -1;
	}
	/* get from's node */
	if(!(from = ogt_get_node_by_parent(glb_ogt_handle, from_parent, &_node_cmp_by_name, mv_node->from.name))){
		fprintf(stderr, "can not find path[%s] below parent[%p] \n", mv_node->from.name, from_parent);
		return -1;
	}

	if(ogt_move_node(glb_ogt_handle, from, to_parent) < 0){
		fprintf(stderr, "move from[%p] to to's parent[%p] failed !\n", from, to_parent);
		return -1;
	}

	if(strcmp(mv_node->from.name, mv_node->to.name)){
		fprintf(stdout, "file name have been changed\n");
		/*update the name TODO need add check if update ok */
		return _update_file_node_name(from, mv_node->to.name);
	}

	return 0;
}

/*
 * only modify the name info, used for move
 */
int _update_file_node_name(ogt_node *this, char *newname)
{
	return ogt_edit_data(glb_ogt_handle, this, &__update_name, newname, 0);
}

/*
 * update name, type and len, called by ogt_edit_data
 */
int __update_name(void *old, void *new, size_t n)
{
	char *src = new;
	og_file_unit *this = old;
	strcpy(this->name, src);
	this->len = strlen(src) + 1;
	if(TYPE_D != this->type)
		this->type = get_type(src);

	return 0;
}

/*
 * only modify the stat info, like err,mtime,size...
 */
int _update_file_node_stat(ogt_node *this, struct stat *status)
{
	return ogt_edit_data(glb_ogt_handle, this, &__update_stat, status, 0);
}

/*
 * update stat info called by ogt_edit_data
 */
int __update_stat(void *old, void *new, size_t n)
{
	struct stat *status = new;
	og_file_unit *this = old;

	if(!new){
		fprintf(stderr, "get file[%s] status failed\n", this->name);
		this->err = 1;
		this->size = 0;
		this->mtime = 0;
		return 0;
	}

#if DEBUG > 7
	fprintf(stdout, "file[%s] get modfied [%lu, %lu] ==> [%lu, %lu]\n", this->name, this->size, this->mtime, status->st_size, status->st_mtime);
#endif
	this->err = 0;
	this->size = status->st_size;
	this->mtime = status->st_mtime;

	return 0;
}

void __clean_move_node(_file_move_node *node)
{
	node->cookie = 0;

	node->from.wd = -1;
	node->from.name[0] = '\0';

	node->to.wd = -1;
	node->to.name[0] = '\0';
}

/*
 * find the move node where cookie = @cookie
 */
_file_move_node *_find_move_node(uint32_t cookie)
{
	int i;

	for(i = 0; i < _TMP_MOVE_CNT; i++){
		if(cookie == pub_move_list[i].cookie){
			return pub_move_list + i;
		}
	}

	return NULL;
}

/*
 * find the move node where cookie = 0
 */
_file_move_node *_get_free_move_node(void)
{
	return _find_move_node(0);
}

/*
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
	//printf("get full path[%s]\n", path);
	strcat(path, buf->path);
#if DEBUG > 8
	printf("get full name[%s]\n", path);
#endif

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

/*
 * get ACT_MODIFY
 */
int _work_modify(_og_unit *buf)
{
	char path[IBIG_PATH_MAX];
	struct stat status;
	ogt_node *parent;
	ogt_node *this;
	int err = 0;

	_get_full_path(wlist_head, buf->wd, glb_name_stack, path, &parent);
	strcat(path, buf->path);
#if DEBUG > 8
	printf("get full name[%s]\n", path);
#endif

	if(TYPE_D == buf->type){
		fprintf(stderr, "this path[%s] get modifed what happend??\n", path);
//		return -1;
	}

	if(stat(path, &status) < 0){
		perror("stat()");
		err = 1;
		status.st_mtime = 0;
		status.st_size = 0;
	}
	_watch_list_node *w_this;

	/* get this's parent */
	if(!(w_this = _wlist_search(wlist_head, buf->wd)) || !(parent = w_this->this)){
		fprintf(stderr, "can not find wd[%d] in watch list\n", buf->wd);
		return -1;
	}

	/* get this node by compare fil name */
	if(!(this = ogt_get_node_by_parent(glb_ogt_handle, parent, &_node_cmp_by_name, buf->path))){
		fprintf(stderr, "can not find path[%s] below parent[%p] \n", buf->path, parent);
		return -1;
	}

	_update_file_node_stat(this, err ? NULL : &status);

//ogt_tree_travel(glb_ogt_handle, _travel_);

	return 0;
}

/*
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

	if(!(this = ogt_get_node_by_parent(glb_ogt_handle, parent, &_node_cmp_by_name, buf->path))){
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
 * cmpare name, called by ogt_get_node_by_parent()
 *
 * for func:	return 0 means stop and return this node
 * 		return others means continue
 */
int _node_cmp_by_name(void *data, void *path)
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

/*
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

/*
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

/*
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
	buf->path[buf->base_or_cookie-1] = '/';
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

/*
 * get the parent node(directory) in tree
 */
ogt_node *_get_tree_parent(_og_unit *buf)
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

/*
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
int _read_one_unit(int bd, _og_unit *buf)
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
