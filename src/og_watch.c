#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <sys/inotify.h>
#include <string.h>

#include "og_watch.h"
#include "og_tool.h"
#include "obuf.h"
/*temp*/
#include <stdio.h>

static _og_unit *pub_unit;
static int glb_bd;

/**
 * !
 */
//struct inotify_event {
//    int      wd;       /* Watch descriptor */
//    uint32_t mask;     /* Mask of events */
//    uint32_t cookie;   /* Unique cookie associating related
//                          events (for rename(2)) */
//    uint32_t len;      /* Size of name field */
//    char     name[];   /* Optional null-terminated name */
//};

int _monitor_worker(char *buf, int cnt);
int _process_watch_events(const struct inotify_event *evt);
int _i_get_create(const struct inotify_event *evt, _og_unit *buf, int dir, int err);
int _i_get_delete(const struct inotify_event *evt, _og_unit *buf, int dir, int err);
int _i_get_move(const struct inotify_event *evt, _og_unit *buf, int dir, int err, int from);
int _i_get_modify(const struct inotify_event *evt, _og_unit *buf, int dir, int err);
int _wrt_unit(_og_unit *buf);

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
	glb_bd = bd;

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

int og_watch_listen_cycle(int watch_fd)
{
	char buf[RD_BUF_LEN];
	//int nread;
	int cnt;

/* || read 0 bytes becase:read from inotify fd,at least 1 unit, do not worry for while(1) */
	while(1 || read(watch_fd, buf, 0)){
		//ioctl(fd, FIONREAD, &nread);
		if((cnt = read(watch_fd, buf, RD_BUF_LEN)) < 0){
			perror("read()");
			return -1;
		}
		if(_monitor_worker(buf, cnt)){
			printf("err ???\n");
			exit(0);
		}
	}

	return 0;
}

/*return bytes left do not work*/
int _monitor_worker(char *buf, int cnt)
{
	int wrt = 0;
	struct inotify_event *p = (struct inotify_event *)buf;
#if DEBUG > 7
	printf("want work %d bytes;\n", cnt);
#endif
	while(wrt < cnt){
//memset(pub_event, 0x00, sizeof(struct inotify_event)+IBIG_NM_MAX+1);
//memcpy(pub_event, p, sizeof(struct inotify_event)+((struct inotify_event *)p)->len);
		/*wrt the path to obuf*/
		_process_watch_events(p);
		wrt += sizeof(struct inotify_event) + p->len;
		p = (struct inotify_event *)(buf + wrt);
	}
#if DEBUG > 7
	printf("already work %d bytes;\n", wrt);
#endif

	return cnt - wrt;
}

int _process_watch_events(const struct inotify_event *evt)
{
	/* in this case, if mv 'file1' to'.file2', the 'file1' can not be deal with */
	if(evt->mask & (IN_DELETE | IN_CREATE | IN_MODIFY) && evt->len > 0 && '.' == evt->name[0])
		return 0;

	//char path[IBIG_PATH_MAX] = {};
	int dir = 0;

#if DEBUG > 6
	printf("\tp[%s], event:%08x, parent dir is list[%d] cookie[%d] len[%d] strlen[%lu] size[%lu]\n", evt->name, evt->mask, evt->wd, evt->cookie, evt->len, strlen(evt->name), sizeof(struct inotify_event));
#endif
/* if get IN_IGNORED, the wd has already been deleted */
	if(evt->mask & IN_IGNORED){
#if DEBUG > 7
		printf("IGNORE![%d]\n", evt->wd);
#endif
		return 0;
	}

	if(evt->mask & IN_ISDIR){
		dir = 1;
	}

//struct inotify_event {
//    int      wd;       /* Watch descriptor */
//    uint32_t mask;     /* Mask of events */
//    uint32_t cookie;   /* Unique cookie associating related
//                          events (for rename(2)) */
//    uint32_t len;      /* Size of name field */
//    char     name[];   /* Optional null-terminated name */
//};
	if(evt->mask & IN_CREATE){
		_i_get_create(evt, pub_unit, dir, 0);
	}else if(evt->mask & IN_DELETE){
		_i_get_delete(evt, pub_unit, dir, 0);
	}else if(evt->mask & IN_MODIFY){
		_i_get_modify(evt, pub_unit, dir, 0);
	}else if(evt->mask & IN_MOVED_FROM){
		_i_get_move(evt, pub_unit, dir, 0, 1);
	}else if(evt->mask & IN_MOVED_TO){
		_i_get_move(evt, pub_unit, dir, 0, 0);
	}else{
		printf("how can you get here?\n");
	}


	return 0;
}

int _i_get_modify(const struct inotify_event *evt, _og_unit *buf, int dir, int err)
{
	buf->action = ACT_MODIFY;
	buf->err = err;
	//buf->base_or_pwd = evt->wd;
	//buf->size = dir ? 0 : sb->st_size;
	//buf->mtime = sb->st_mtime;
	buf->wd = evt->wd;
	buf->path[0] = '\0';
	strncat(buf->path, evt->name, evt->len);
	buf->type = dir ? TYPE_D : get_type(evt->name);
	buf->len = strlen(buf->path) + 1;

	_wrt_unit(buf);

	return 0;
}

int _i_get_move(const struct inotify_event *evt, _og_unit *buf, int dir, int err, int from)
{
	buf->action = from ? ACT_MV_F : ACT_MV_T;
	buf->err = err;
	//buf->base_or_pwd = evt->wd;
	//buf->size = dir ? 0 : sb->st_size;
	//buf->mtime = sb->st_mtime;
	buf->wd = evt->wd;
	buf->path[0] = '\0';
	strncat(buf->path, evt->name, evt->len);
	buf->type = dir ? TYPE_D : get_type(evt->name);
	buf->len = strlen(buf->path) + 1;

	_wrt_unit(buf);

	return 0;
}

/* note IN_ISDIR means that I delete a dir */
int _i_get_delete(const struct inotify_event *evt, _og_unit *buf, int dir, int err)
{
	buf->action = ACT_DEL;
	buf->err = err;
	buf->wd = evt->wd;
	//buf->base_or_pwd = evt->wd;
	//buf->size = dir ? 0 : sb->st_size;
	//buf->mtime = sb->st_mtime;
	buf->path[0] = '\0';
	strncat(buf->path, evt->name, evt->len);
	buf->type = dir ? TYPE_D : get_type(evt->name);
	buf->len = strlen(buf->path) + 1;

	_wrt_unit(buf);

	return 0;
}

/*TODO: add function get_full_path(wd) */
/**
 * 'i' means inotify, not me
 * 
 */
int _i_get_create(const struct inotify_event *evt, _og_unit *buf, int dir, int err)
{
printf("in function[%s]\n", __FUNCTION__);
	buf->action = ACT_NEW;
	buf->err = err;
	buf->base_or_selfwd = dir ? 0 : -1;
	buf->size = 0;
	buf->mtime = 0;
	//buf->size = dir ? 0 : sb->st_size;
	//buf->mtime = sb->st_mtime;
/* BUG !!  Oh NO, this time, I can not get full path, now, add_watch in og_record */
	buf->wd = evt->wd;
	buf->path[0] = '\0';
	strncat(buf->path, evt->name, evt->len);
	buf->type = dir ? TYPE_D : get_type(evt->name);
	buf->len = strlen(buf->path) + 1;

	_wrt_unit(buf);

	return 0;
}

int _wrt_unit(_og_unit *buf)
{
printf("in function[%s]\n", __FUNCTION__);
printf("write path[%s],action[%d],wd[%d],write[%lu]\n", buf->path, buf->action, buf->wd, offsetof(_og_unit, path) + buf->len);
	while(obuf_write(glb_bd, buf, offsetof(_og_unit, path) + buf->len, 0, NULL) < 0){
#if DEBUG > 9
		printf("buffer went full, wait and again\n");
#endif
		usleep(10);
	}

	return 0; 
}
