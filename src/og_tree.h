#ifndef _OG_TREE_H_
#define _OG_TREE_H_

#include <stdint.h>

/**
 * operator the tree(storage in file)
 * author:      Mr.OS <zhuohongchao@chunhongtech.com>
 * 
 */

/**
 * log
 * 	------- 2017-01-05 -------
 * 	give up the method of node starage(all in file)
 *	(now, whole tree store in memory, data write in file)
 *
 * 	------- 2017-01-05 -------
 * 	start
 * 	[do not support multi-thread]
 * 	support max file size UINT32_MAX*OG_PAGE_SIZE
 * 
 */

#define OG_VERSION		100

#define OG_NAME_MAX		255
#define OG_HANDLER_CNT		8
/*every page is 32M; the value must be N*4k*/
//#define OG_PAGE_SIZE		(4*1024*8*1024UL)
#define OG_PAGE_SIZE		(4*1024*1024UL)

#define OG_DFT_MEM_PAGE		(4*1024UL)

/*this og_head_t will write to file*/
/*_SC_PAGE_SIZE must be larger than sizeof(og_head)*/
typedef struct og_head_t {
	uint32_t	version;	/*tree version(decide by og_head_t and og_node_t)*/
	uint32_t	page_cnt;	/*how many pages in this file*/
	uint32_t	data_len;	/*the length of data filed in og_node*/
	uint32_t	head_size;	/*maybe 4k, returned by sysconf(_SC_PAGE_SIZE)*/
//	uint32_t	page_size;
//	uint32_t	type;		/*Reserved now no useful*/

/*the fallow field need to add to the head in memory*/
//	uint32_t	page_now;	/*how many pages in this file*/
//	int32_t		fd;
//	char		path[OG_NAME_MAX+1];	/*tree file path*/
//	char		*start;		/*pointer to the mapped area,returned by mmap() or mremap()*/
} og_head;

typedef struct og_pos_t {
	uint32_t	page;
	uint32_t	offset;
} og_pos;

/**
 * old ---- 2017-02-08
 * left child
 * right sibling
 */
//typedef struct og_node_t {
//	og_pos		left;		/*first child*/
//	og_pos		right;		/*next sibling*/
//	og_pos		parent;		/*parent node offset*/
//	uint8_t		mask;		/*0 means unused, others */
//	char		data[0];	/*the data*/
//} og_node;

/**
 * tree_node stored in memory
 */
typedef struct og_node_t {
	struct og_node_t	*l_child;	/*first child*/
	struct og_node_t	*r_sib;		/*next sibling*/
	struct og_node_t	*parent;	/*parent node offset*/
	og_pos			pos;		/*node pos in file*/
 //-----------WARNING-----for a parent, if child is NULL, sib must NULL----
} og_node;

#define _OG_INUSE	(0x01UL<<0)
//#define _OG_ROOT	(0x01UL<<1)
/**
 * data_node stored in file
 * 
 */
typedef struct og_data_node_t {
	uint8_t		mask;		/*0 means unused, others */
	char		data[0];	/*the data*/
} og_data_node;

int og_init(const char *path, int size);
og_node *og_insert_by_parent(int handler, const void *data, int size, og_node *parent);
//int og_delete_by_cmp(int handler, void *data, int size);
int og_delete_node(int handler, og_node *this);
void og_travel(int handler, void (*func_p)(void *));

void og_destory(int handler);

#endif
