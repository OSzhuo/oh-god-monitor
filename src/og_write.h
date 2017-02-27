#ifndef _OG_WRITE_H_
#define _OG_WRITE_H_

#include   "og_defs.h"

#define C_ERR_OTHER		1
#define C_ERR_INIT		2
//#define C_ERR_OTHER		3

/* TODO ===> check if server is in running */
int og_client_init(const char *path);
int og_client_work(void);
void og_client_clean(void);

#endif
