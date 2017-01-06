#ifndef _OG_TREE_H_
#define _OG_TREE_H_

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

#define HANDLER_CNT		8

typedef struct og_head_t {
	int	version;
	int	data_len;
	int	type;		/*now no nueful(if write into file or other)*/
	int	fd;
//	char	path[256];	/*tree file path*/
} og_head;

/**
 * left child
 * right sibling
 */
typedef struct og_node_t {
	uint32_t	child;
	uint32_t	sibling;
	uint32_t	parent;
	char		data[0];
}og_node;



#endif
