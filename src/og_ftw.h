#ifndef _OG_FTW_H_
#define _OG_FTW_H_

#define _OG_FTW_DEBUG		6

#define _OG_FTW_PATH_MAX	4096

#define OG_FTW_CONTINUE		0
#define OG_FTW_SKIP		1
#define OG_FTW_STOP		2

#define OG_FTW_F		0
#define OG_FTW_D		1
#define OG_FTW_DNR		2
#define OG_FTW_NS		3
#define OG_FTW_SL		4
//#define _OG_FTW_SLN	0

/**
 * file tree walk
 *
 */

/**
 * ------ 2017-03-07 ------
 * do not support the max count of opened fd
 */

/**
 *
 * walks  through the directory tree that is located under the directory @path,
 *		 and calls fn() once for each entry in the tree. 
 * By default, directories are  handled before the files and subdirectories they contain
 *		(preorder traversal).
 */
int og_ftw(const char *path, int (*fn)(const char *path, const struct stat *sb, int type, int base), int nopenfd);

//int _walk_file_tree_recursively(const char *path, int (*handler)(const char *path, const struct stat *sb, int type, int base), int o_base);

#endif
