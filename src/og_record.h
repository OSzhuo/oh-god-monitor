#ifndef _OG_RECORD_H_
#define _OG_RECORD_H_

#include "og_defs.h"

#define OG_TREE_FILE	"/ibig/ibig_monitor/tree.st"

int og_record_init(int bd, int fd);
int og_init_start(void);

/**
 * init ok, destory the stack
 */
int og_init_over(void);

void *og_record_work(void *);

#endif
