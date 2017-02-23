#define _GNU_SOURCE
//#define _XOPEN_SOURCE 500
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <ftw.h>
#include <stddef.h>

#include "og_init.h"
#include "og_tool.h"
#include "obuf.h"
#include <stdio.h>

static int glb_watch_fd;
static int glb_bd;
static int (*_watch_add)(int fd, const char *path);
static _og_unit *pub_unit;

_og_unit *_init_unit(int size);
int _write_new_unit(const char *path, const struct stat *sb, int err, int base, int dir, int wd);
int _nftw_handle(const char *path, const struct stat *sb, int type, struct FTW *ftwbuf);

int og_list_all(int watch_fd, char *path, int bd, int (*watch_add_func)(int fd, const char *p))
{
	if(!path)
		return -1;

	_watch_add = *watch_add_func;

	glb_watch_fd = watch_fd;

	if(!(pub_unit = _init_unit(0))){
		fprintf(stderr, "init struct line err!\n");
		return -1;
	}
//	glb_bd = bd;

//printf("ftw_f[%08X] ftw_d[%08x] ftw_dnr[%08x]\n", FTW_F, FTW_D, FTW_DNR);
//printf("FTW_F[%04x] FTW_D[%04x] FTW_DNR[%04x] FTW_NS[%04x] FTW_SL[%04x]\n", FTW_F, FTW_D, FTW_DNR, FTW_NS, FTW_SL);

	return nftw(path, _nftw_handle, MAX_NFD, FTW_ACTIONRETVAL | FTW_PHYS);
}

int _nftw_handle(const char *path, const struct stat *sb, int type, struct FTW *ftwbuf)
{

/*	printf("%-3s %2d %7d   %-40s %-4d %s\n",
		(type == FTW_D) ?   "d"   : (type == FTW_DNR) ? "dnr" :
		(type == FTW_DP) ?  "dp"  : (type == FTW_F) ?   "f" :
		(type == FTW_NS) ?  "ns"  : (type == FTW_SLN) ?  "sln" :
		(type == FTW_SL) ? "sl" : "???",
		ftwbuf->level, (int)sb->st_size,
		path, ftwbuf->base, path + ftwbuf->base);
*/


	if('.' == path[ftwbuf->base]){
		if(type & FTW_D)
			return FTW_SKIP_SUBTREE;
		return FTW_CONTINUE;
	}

	int dir = 0;
	int err = 0;
	int wd = -1;

	switch(type){
		case FTW_DP:/*if FTW_DEPTH is set*/
			//printf("get type dir (depth)\n");
		case FTW_D:
			//printf("get type dir\n");
			dir = 1;
			break;
		case FTW_F:
			//printf("this is file\n");
			break;
		case FTW_SL:
			/*if FTW_PHYS is set, can not get sln, so access if is right*/
			if(access(path, F_OK) && errno == ENOENT){
				//      printf("this file is sln\n");
				err = 1;
			}else{
				//      printf("this file is sl\n");
			}
			break;
		case FTW_DNR:
			//printf("this is directory\n");

			dir = 1;
			//break;
		case FTW_SLN:
			/*This occurs only if FTW_PHYS is not setif*/
			//break;
		case FTW_NS:
			//break;
		default:
			err = 1;
			//printf("file err\n");
			break;
	}

	//printf("base[%d]%s level[%d];\n", ftwbuf->base, path+ftwbuf->base, ftwbuf->level)    ;

	//fprintf(stdout, "====== handle ==============\npath[%-32s] type[%08x]\n==========    ==\n", path, type);
	/*add watch if is dir*/
	if(dir)
		wd = (*_watch_add)(glb_watch_fd, path);

	/*write to the buffer*/
	_write_new_unit(path, sb, err, ftwbuf->base, dir, wd);

	return FTW_CONTINUE;
}

/**
 * === WARNING ===
 * do not check anything
 */
int _write_new_unit(const char *path, const struct stat *sb, int err, int base, int dir, int wd)
{
	pub_unit->action = ACT_INIT;
	pub_unit->err = err;
	strcpy(pub_unit->path, path);
	pub_unit->len = strlen(path) + 1;
	pub_unit->size = dir ? 0 : sb->st_size;
	pub_unit->base_or_selfwd = base;
	pub_unit->type = dir ? TYPE_D : get_type(path+base);
	pub_unit->mtime = sb->st_mtime;
	pub_unit->wd = wd;

	//printf("[%c] st_size[%8ld] mtime[%ld] path[%s]\n", pub_unit->type, pub_unit->size, pub_unit->mtime, pub_unit->path);

	while(obuf_write(glb_bd, pub_unit, offsetof(_og_unit, path) + pub_unit->len, 0, NULL) < 0){
		printf("buffer went full, wait and again \n");
//		//printf("size = %d\n",sizeof(struct list_line_t) + glb_line->len);
		usleep(10);
	}

	return 0;
}

int og_init_over(void)
{
	pub_unit->action = ACT_INIT_OK;
	pub_unit->len = 0;

	while(obuf_write(glb_bd, pub_unit, offsetof(_og_unit, path) + pub_unit->len, 0, NULL) < 0){
		//usleep(10);
	}

	return 0;
}

_og_unit *_init_unit(int size)
{
	if(size <= 0)
		size = IBIG_PATH_MAX;

	_og_unit *p;

	if(NULL == (p = malloc(sizeof(_og_unit)+size))){
		perror("malloc()");
		return NULL;
	}

	return p;
}
