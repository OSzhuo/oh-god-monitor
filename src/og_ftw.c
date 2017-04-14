#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "og_ftw.h"
#include <stdio.h>

/* ftw errno like errno */
static int _ftw_errno;

static int _handler_run(const char *path, int  (*handler)(const char *path, const struct stat *sb, int type, int base), struct stat *buf, int base);
static int _walk_file_tree_recursively(const char *path, int (*handler)(const char *path, const struct stat *sb, int type, int base), int o_base);

#if _OG_FTW_DEBUG > 6
int tmp_hand(const char *path, const struct stat *sb, int type, int base)
{
	fprintf(stdout, "[%s][%-32s][%d][%d][%-12s]\n",
		(type == OG_FTW_D) ? "d" : (type == OG_FTW_F) ? "f" :
		(type == OG_FTW_DNR) ? "dnr" : (type == OG_FTW_NS) ? "ns" :
		(type == OG_FTW_SL) ? "sl" : "???",
		path, sb->st_size, base, path+base);

	return 0;
}
#endif

int og_ftw(const char *path, int (*fn)(const char *path, const struct stat *sb, int type, int base), int nopenfd)
{
	char my_path[_OG_FTW_PATH_MAX];
	int len;

	if(!fn){
		fprintf(stderr, "invalid function.\n");
		return -1;
	}
	strcpy(my_path, path);
	len = strlen(my_path);
#if _OG_FTW_DEBUG > 6
fprintf(stdout, "og_ftw get path[%s]len[%d]\n", my_path, len);
#endif
	if('/' == path[len-1]){
		my_path[len-1] = '\0';
		len--;
	}
#if _OG_FTW_DEBUG > 6
fprintf(stdout, "og_ftw set path[%s]len[%d]\n", my_path, len);
#endif

	/* point to the last '/' in my_path */
	char *last;
	/* base is the offset of the filename in path */
	int base;

	if(!(last = strrchr(my_path, '/'))){
		fprintf(stderr, "invalid path [%s]\n", my_path);
		return -1;
	}
	base = last - my_path + 1;
#if _OG_FTW_DEBUG > 6
fprintf(stdout, "og_ftw set base[%d] name[%s]\n", base, my_path+base);
#endif

	return _walk_file_tree_recursively(my_path, fn, base);
}

int _handler_run(const char *path, int  (*handler)(const char *path, const struct stat *sb, int type, int base), struct stat *buf, int base)
{
	int type;
	struct stat tmp;

	if(!buf){
		if(!lstat(path, &tmp)){
			buf = &tmp;
		}else{
			perror("lstat()");
			buf = NULL;
		}
	}

	if(buf && S_ISDIR(buf->st_mode) && !S_ISLNK(buf->st_mode)){
		type = OG_FTW_D;
	}else{
		type = OG_FTW_F;
	}
	//fprintf(stdout, "[%s][%s][%d] type[%d][%s] name[%d][%s]\n", __FILE__, __FUNCTION__, __LINE__, type, path, base, path+base);

	return (*handler)(path, buf, type, base);
}

/**
 * walk file tree
 * fn() return	OG_FTW_CONTINUE	--->continue
 * 		OG_FTW_SKIP	--->skip this subtree
 * 		OG_FTW_STOP	--->stop walk
 * @path can not be end with '/' !!
 * _nftw_handle(const char *path, const struct stat *sb, int type, struct FTW *ftwbuf)
 */
int _walk_file_tree_recursively(const char *path, int (*handler)(const char *path, const struct stat *sb, int type, int base), int o_base)
{
	int ret;
	DIR *dir;
	int base = strlen(path) + 1;
	_ftw_errno = 0;

	if(OG_FTW_CONTINUE != (ret = _handler_run(path, handler, NULL, o_base)))
		return ret;

	dir = opendir(path);
	if(!dir){
		fprintf(stderr, "opendir [%s] failed!\n", path);
		_ftw_errno = errno;
		return OG_FTW_CONTINUE;
		//if(ENOTDIR == errno){
		//	/* call fun() */
		//	return _handler_run(path, handler, NULL, base);
		//}else{
		//	_ftw_errno = errno;
		//	return 0;
		//}
	}

	char *next_file;
	static struct dirent *ent;
	static struct stat my_stat;

	// walk each directory within this directory
	while(NULL != (ent = readdir(dir))){
		if((0 != strcmp(ent->d_name, ".")) && (0 != strcmp(ent->d_name, ".."))){
			if(-1 == asprintf(&next_file,"%s/%s", path, ent->d_name)){
				fprintf(stderr, "out of memory!\n");
				return -1;
			}
			if(-1 == lstat(next_file, &my_stat)){
				_ftw_errno = errno;
				fprintf(stderr, "stat file[%s] failed! walk continue.\n", next_file);
				/* this file unreadable */
				if(OG_FTW_STOP == (ret = _handler_run(path, handler, NULL, base))){
					free(next_file);
					closedir(dir);
					return ret;
				}
				/* err, have no other operatation */
				free(next_file);
				//if(errno != EACCES){
				//	_ftw_errno = errno;
				//	if(my_path != path) free(my_path);
				//	closedir(dir);
				//	return 0;
				//}
			}else if(S_ISDIR(my_stat.st_mode) && !S_ISLNK(my_stat.st_mode)){
				free(next_file);
				if(-1 == asprintf(&next_file,"%s/%s", path, ent->d_name)){
					fprintf(stderr, "out of memory!\n");
					return -1;
				}
				//static int status;
				if(OG_FTW_SKIP == (ret = _walk_file_tree_recursively(next_file, handler, base))){
					fprintf(stdout, "this dir[%s] skiped.\n", next_file);
				}else if(OG_FTW_STOP == ret){
					fprintf(stderr, "[%d]handler return nonzero,stop walk.\n", __LINE__);
					free(next_file);
					closedir(dir);
					return ret;
				}
				free(next_file);
			// if isdir and not islnk
			}else{
				/* not dir, ignore OG_FTW_SKIP */
				free(next_file);
				if(-1 == asprintf(&next_file,"%s/%s", path, ent->d_name)){
					fprintf(stderr, "out of memory!\n");
					return -1;
				}
				if(OG_FTW_STOP == (ret = _handler_run(next_file, handler, &my_stat, base))){
					fprintf(stderr, "handler return nonzero,stop walk.\n");
					free(next_file);
					closedir(dir);
					return ret;
				}
				free(next_file);
			}
		}
		_ftw_errno = 0;
	}

	closedir(dir);

	return OG_FTW_CONTINUE;
}
