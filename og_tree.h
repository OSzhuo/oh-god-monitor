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
 * 	start
 * 	[do not support multi-thread]
 * 
 */

#define OG_VERSION		100

/*max map length is 4G*/
#define OG_MAP_LEN		(4*1024*1024*1024UL)
#define OG_NAME_MAX		255
#define OG_HANDLER_CNT		8
#define OG_PAGE_SIZE		(300*1024*1024UL)

/*this og_head_t will write to file*/
/*!!! different with _og_head_t !!!*/
typedef struct og_head_t {
	uint32_t	version;	/*tree version(decide by og_head_t and og_node_t)*/
	uint32_t	data_len;	/*the length of data filed in og_node*/
//	uint32_t	use_len;	/*length already use in file*/
	uint32_t	page_cnt;	/*how many pages in this file*/
//	uint32_t	page_size;
//	uint32_t	type;		/*now no nueful(if write into file or other)*/
//	int32_t		fd;
	char		path[OG_NAME_MAX+1];	/*tree file path*/
} og_head;

/**
 * left child
 * right sibling
 */
typedef struct og_node_t {
	int8_t		inuse;
	uint32_t	child;
	uint32_t	sibling;
	uint32_t	parent;
//	uint32_t	len;
	char		data[0];
}og_node;


#endif
