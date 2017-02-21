#ifndef _OG_RECORD_H_
#define _OG_RECORD_H_

#include "og_defs.h"

int og_record_init(int bd);
int og_init_start(void);

/**
 * init ok, destory the stack
 */
int og_init_over(void);

void *og_read_unit_from_obuf(void *);

#endif
