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
 * 	support max file size UINT32_MAX*OGT_PAGE_SIZE
 * 
 */

#define OGT_VERSION		100

#define OGT_NAME_MAX		255
#define OGT_HANDLER_CNT		8
/*every page is 32M; the value must be N*4k*/
//#define OGT_PAGE_SIZE		(4*1024*8*1024UL)
#define OGT_PAGE_SIZE		(4*1024*1024UL)

#define OGT_DFT_MEM_PAGE		(4*1024UL)

/*this ogt_head_t will write to file*/
/*_SC_PAGE_SIZE must be larger than sizeof(ogt_head)*/
typedef struct ogt_head_t {
	uint32_t	version;	/*tree version(decide by ogt_head_t and ogt_node_t)*/
	uint32_t	page_cnt;	/*how many pages in this file*/
	uint32_t	data_len;	/*the length of data filed in ogt_node*/
	uint32_t	head_size;	/*maybe 4k, returned by sysconf(_SC_PAGE_SIZE)*/
//	uint32_t	page_size;
//	uint32_t	type;		/*Reserved now no useful*/

/*the fallow field need to add to the head in memory*/
//	uint32_t	page_now;	/*how many pages in this file*/
//	int32_t		fd;
//	char		path[OGT_NAME_MAX+1];	/*tree file path*/
//	char		*start;		/*pointer to the mapped area,returned by mmap() or mremap()*/
} ogt_head;

typedef struct ogt_pos_t {
	uint32_t	page;
	uint32_t	offset;
} ogt_pos;

/**
 * old ---- 2017-02-08
 * left child
 * right sibling
 */
//typedef struct ogt_node_t {
//	ogt_pos		left;		/*first child*/
//	ogt_pos		right;		/*next sibling*/
//	ogt_pos		parent;		/*parent node offset*/
//	uint8_t		mask;		/*0 means unused, others */
//	char		data[0];	/*the data*/
//} ogt_node;

/**
 * tree_node stored in memory
 */
typedef struct ogt_node_t {
	struct ogt_node_t	*l_child;	/*first child*/
	struct ogt_node_t	*r_sib;		/*next sibling*/
	struct ogt_node_t	*parent;	/*parent node offset*/
	ogt_pos			pos;		/*node pos in file*/
 //-----------WARNING-----for a parent, if child is NULL, sib must NULL----
} ogt_node;

#define _OGT_INUSE	(0x01UL<<0)
//#define _OGT_ROOT	(0x01UL<<1)
/**
 * data_node stored in file
 * 
 */
typedef struct ogt_data_node_t {
	uint8_t		mask;		/*0 means unused, others */
	char		data[0];	/*the data*/
} ogt_data_node;

int ogt_init(const char *path, int size);
ogt_node *ogt_insert_by_parent(int handler, const void *data, int size, ogt_node *parent);
//int ogt_delete_by_cmp(int handler, void *data, int size);
int ogt_delete_node(int handler, ogt_node *this);
int ogt_edit_data(int handler, ogt_node *node, int (*func)(void *old, void *new, size_t n), void *data, size_t n);
int ogt_move_node(int handler, ogt_node *this, ogt_node *parent);
int ogt_preorder_R(int handler, int (*node_func)(void *file, void *data, const ogt_node *node), void *data);
int ogt_get_node_travel(int handler, const ogt_node *this, int (*func_p)(void *, void *, const ogt_node *), void *data);

ogt_node *ogt_parent(const ogt_node *this);

void ogt_tree_travel(int handler, void (*func_p)(void *));
void ogt_travel(int handler, void (*func_p)(void *));

void ogt_destory(int handler);

#endif
