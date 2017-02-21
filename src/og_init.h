#ifndef _OG_INIT_H_
#define _OG_INIT_H_

#include "og_defs.h"

/* just for nftw() */
#define MAX_NFD		1024

#define PATH_MAX_GUESS  4096
/*extension name max length (with '\0')*/

int og_list_all(int fd, char *path, int bd, int (*watch_add_func)(int fd, const char *p));
int og_init_over(void);

#endif
