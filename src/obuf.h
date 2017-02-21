#ifndef __OBUF_H__
#define __OBUF_H__

/**
 * operate the circular buffer
 * author:	Mr.OS <zhuohongchao@chunhongtech.com>
 * 
 * 
 */

/**
 * Log:
 *	---- 2016-11-30 ----
 * 	create project(or what ?)
 * 	[do not support multi-thread]
 * 	====================
 */

#define MIN_BUF_SZ	8
//#define ERR_LEN		32

//typedef struct obuf_t obuf;


/**
 * init a buffer/queue and the struct obuf
 * 
 */
int obuf_open(const int size, int *err);

/**
 * 
 * brk must 1 ---- 2016.12.01
 */
ssize_t obuf_read(int head, void *buf, size_t size, int brk, int *err);

/**
 * 
 * 
 */
ssize_t obuf_write(int head, const void *buf, size_t size, int brk, int *err);

/**
 * get used size of the buffer
 * 
 */
size_t obuf_get_used(int head, int *err);

//static size_t _p_add(size_t a, size_t b, size_t len);

/*clean the struct obuf_t but do not free the memory*/
void obuf_clean(int head);

/*destory the buffer*/
void obuf_free(int head);

/**
 * if buffer is empty
 */
int obuf_isempty(int head);

/**
 * if obuf is full
 * 1 full
 * 0 not full
 */
int obuf_isfull(int head);

#endif
