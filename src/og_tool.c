#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#include "og_tool.h"
#include "og_defs.h"

static int _get_type(char *ext);

/**
 * do not check if dir
 */
int get_type(const char *name)
{
	char ext[NM_LEN] = {};
	char *p;

//printf("this name %s\n", name);
	if(NULL == (p = strrchr(name, '.')))
		return 'O';
	strcpy(ext, p+1);

	return _get_type(strlwr(ext));
}

static int _get_type(char *ext)
{
	//printf("get extension name %s [func %s()]\n", ext, __FUNCTION__);
	char list[][MAX_EXT_LEN] = {EXT_FLAG_P, EXT_LIST_P, EXT_FLAG_A, EXT_LIST_A, EXT_FLAG_V, EXT_LIST_V, EXT_FLAG_T, EXT_LIST_T, EXT_FLAG_O, ""};
	char *p;
	int i = 0;
	int type;

	while('\0' != *(p = list[i++])){
		if(strchr(EXT_FLAG_ALL, *p) && (type = *p))
			continue;
		if(strlen(p) != strlen(ext))
			continue;
		if(strcmp(p, ext))
			continue;
		else
			break;
	}

	return type;
}

/**
 * lower all char in string s
 */
char *strlwr(char *s)
{
	char *p = s;

#if DEBUG >= 8
	printf("before: %s in func %s()\n", s, __FUNCTION__);
#endif
	while(*p){
		*p = tolower(*p);
		p++;
	}
#if DEBUG >= 8
	printf("after: %s in func %s()\n", s, __FUNCTION__);
#endif

	return s;
}

#define PATH_MAX_GUESS  4096
/**
 * get the max path length include nul
 */
int get_path_max(void)
{
#ifdef PATH_MAX
        int pathmax=PATH_MAX;
#else
        int pathmax=0;
#endif
        if(!pathmax && (pathmax = pathconf("/", _PC_PATH_MAX)) < 0)
                pathmax = PATH_MAX_GUESS;

        return pathmax;
}
