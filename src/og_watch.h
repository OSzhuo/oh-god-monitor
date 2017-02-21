#ifndef _OG_WATCH_H_
#define _OG_WATCH_H_

#include "og_defs.h"

/* max name length(include nul) */
#define IBIG_NM_MAX		256

/* the length of buf for read watch event */
#define RD_BUF_LEN		4096

/*
				| IN_DELETE_SELF | IN_MOVE_SELF         \
 */
#define IBIG_EVENT_WATCH        (IN_EXCL_UNLINK | IN_DONT_FOLLOW        \
				| IN_MOVED_FROM | IN_MOVED_TO           \
				| IN_CREATE | IN_DELETE)

int og_watch_init(int bd);
int og_add_watch(int watch_fd, const char *path);

#endif
