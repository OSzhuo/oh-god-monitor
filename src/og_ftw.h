#ifndef _OG_FTW_H_
#define _OG_FTW_H_

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
int og_ftw(const char *path, int (*fn)(const char *path, int type?), int nopenfd);

#endif
