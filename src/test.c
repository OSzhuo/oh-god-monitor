#include <stdio.h>
#include <time.h>
//#include <limits.h>
#include <unistd.h>
#include <stddef.h>

#define PATH_MAX_GUESS  4096
typedef struct _og_unit_t{
	int     action;         /* action  only dir will get MOVE event */
	int     type;           /* file type */
	int     err;            /* if file access err */
	off_t   size;           /* total size, in bytes */
	time_t  mtime;          /* time of last modification */
	int     wd;             /* if wd<0, this unit must not be DIR */
	int     len;            /* the path length() (include '\0') */
	char    path[];         /* file path(include /tmp/mnt/USB-disk-*)               \
				   and must write '\0' between two sring */
} _og_unit;


int get_path_max(void);

int main(void)
{
//	printf("path max = %d\n", get_path_max());
	printf("sizeof() = %lu\n", sizeof(_og_unit));
	printf("offset of path is = %lu\n", offsetof(_og_unit, path));
	printf("sizeof(time_t) = %lu\n", sizeof(time_t));
	printf("sizeof(off_t) = %lu\n", sizeof(off_t));
	printf("sizeof(int) = %lu\n", sizeof(int));

	return 0;
}

int get_path_max(void)
{
#ifdef PATH_MAX
	int pathmax=PATH_MAX;
#else
	int pathmax=0;
#endif
printf("[before] pathmax = %d\n", pathmax);
//	if(pathmax)
//		return pathmax + 1;
	if(!pathmax && (pathmax = pathconf("/", _PC_PATH_MAX)) < 0){
printf("[in] pathmax = %d\n", pathmax);
		pathmax = PATH_MAX_GUESS;
	}
printf("[after] pathmax = %d\n", pathmax);

	return pathmax;
}
