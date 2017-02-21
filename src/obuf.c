#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>

//#include <stdio.h>
#include "obuf.h"

/**
 * DELETE   ---Note: in the queue, the wrt never be used
 */
typedef struct obuf_t{
	char		*buf;	/*point to the buf memory allocated*/
	size_t		len;	/*length of this buffer/queue*/
	size_t		p_rd;	/*the read pointer/index*/
	size_t		p_wrt;	/*the write pointer/index*/
	size_t		cnt;	/*used size*/
	pthread_cond_t	cond_r;	/*cond for read*/
	pthread_mutex_t	mut_cnt;/*cond for read*/
} obuf;

#define MAX_LIST	4

static obuf *_obuf_list[MAX_LIST];

static obuf *_obuf_get_head(int head);
static obuf *_obuf_get_head(int head);

static int _obuf_get_index(void);
static int _obuf_read(obuf *h, void *p, size_t size);
static int _obuf_write(obuf *h, const void *buf, size_t size);
static void *_obuf_cpy(void *dst, const void *src, size_t size);
static int _obuf_isempty(obuf *h, size_t readsz);
static int _obuf_isfull(obuf *h, size_t wrtsz);
static int _obuf_is_overfllow(obuf *h, size_t wrtsz);

/**
 * init the buffer/queue and the struct obuf
 * return	
 * 		
 * 
 */
int obuf_open(const int size, int *err)
{
	int head;
	if((head = _obuf_get_index()) < 0){
		if(err)
			*err = ENOMEM;
		return -1;
	}

	obuf *h = NULL;
	if(NULL == (h = malloc(sizeof(obuf)))){
		if(err)
			*err = ENOMEM;
		return -1;
	}

	if((size < MIN_BUF_SZ || size <= 0)){
		if(err)
			*err = EINVAL;
		return -1;
	}

	char *buf;

	if(NULL == (buf = malloc(size))){
		if(err)
			*err = errno;
		return -1;
	}

	h->p_rd = h->p_wrt = 0;
	h->len = size;
	h->cnt= 0;
	h->buf = buf;
	pthread_mutex_init(&h->mut_cnt, NULL);
	pthread_cond_init(&h->cond_r, NULL);
	_obuf_list[head] = h;

	return head;
}

/**
 * get struct obuf *  by index in _obuf_list
 * 
 */
static obuf *_obuf_get_head(int head)
{
	if(head > MAX_LIST || head < 0)
		return NULL;
	return _obuf_list[head];
}

/**
 * get min index do not in used
 */
static int _obuf_get_index(void)
{
	int i;

	for(i = 0; i < MAX_LIST; i++)
		if(!_obuf_list[i])
			return i;

	return -1;
}

/**
 * 
 * brk must 1 ---- 2016.12.01
 */
ssize_t obuf_read(int head, void *buf, size_t size, int brk, int *err)
{
	obuf *h = _obuf_get_head(head);
	if(!h || !buf || size < 0){
		if(err)
			*err = EINVAL;
		return -1;
	}

	if(!brk){
		/* user data do not break if queue went empty */
		if(err)
			*err = EINVAL;
		return -1;
	}
//printf("want read %d\n", size);
	pthread_mutex_lock(&h->mut_cnt);
	while(_obuf_isempty(h, 0)){
//fprintf(stdout, "buffer is empty, wait for someone write..\n");
		pthread_cond_wait(&h->cond_r, &h->mut_cnt);
	}

	if(_obuf_isempty(h, size)){
//printf("do not have enough to read, want %d,", size);
		size = h->cnt;
//printf("actually read %d\n", size);
	}

	_obuf_read(h, buf, size);
	pthread_mutex_unlock(&h->mut_cnt);

	return size;
}

/**
 * read without check
 */
static int _obuf_read(obuf *h, void *p, size_t size)
{
	char *buf = p;
	int n = 0;
	if(h->p_rd + size >= h->len){
		n = h->len - h->p_rd;
		_obuf_cpy(buf, h->buf + h->p_rd, n);
		size -= n;
		h->p_rd = 0;
		h->cnt -= n;
		buf += n;
	}

	_obuf_cpy(buf, h->buf + h->p_rd, size);
	h->p_rd += size;
	h->cnt -= size;

	return 0;
}

/**
 * 
 * 
 */
ssize_t obuf_write(int head, const void *buf, size_t size, int brk, int *err)
{
	obuf *h = _obuf_get_head(head);

	if(!h || !buf || size <= 0){
		if(err)
			*err = EINVAL;
		return -1;
	}

	/* user data can break if queue went full */
	if(_obuf_is_overfllow(h, size)){
		if(brk){
			if(err)
				*err = ENOMEM;
			/*will break now do not support*/
			size -= size;
			return -1;
		}
		if(err)
			*err = ENOMEM;
		return -1;
	}

	pthread_mutex_lock(&h->mut_cnt);
	_obuf_write(h, buf, size);
	pthread_cond_signal(&h->cond_r);
	pthread_mutex_unlock(&h->mut_cnt);
//printf("pwrt = %d\trd=%d\tcnt=%d\n", h->p_wrt, h->p_rd, h->cnt);

	return size;
}

/**
 * write without lock
 */
static int _obuf_write(obuf *h, const void *buf, size_t size)
{
//struct list_line_t{
//	int	err;
//	int	type;
//	int	len;
//	char	path[0];
//};
//	printf("want write %s\n", buf+sizeof(struct list_line_t)+1);
	int n = 0;
	if(h->p_wrt + size >= h->len){
		n = h->len - h->p_wrt;
		_obuf_cpy(h->buf + h->p_wrt, buf, n);
		size -= n;
		h->cnt += n;
		h->p_wrt = 0;//_p_add(h->p_wrt, cnt - h->p_wrt, h->len);
		buf += n;
	}

	_obuf_cpy(h->buf + h->p_wrt, buf, size);
	h->p_wrt += size;
	h->cnt += size;

	return 0;
}

/**
 * copy size bytes from src to dst
 * 
 */
static void *_obuf_cpy(void *dst, const void *src, size_t size)
{
	return memmove(dst, src, size);
}

/**
 * get used size of the buffer
 * 
 */
size_t obuf_get_used(int head, int *err)
{
	obuf *h = _obuf_get_head(head);

	if(!h){
		if(err)
			*err = EINVAL;
		return -1;
	}

	return h->cnt;
}

//static size_t _p_add(size_t a, size_t b, size_t len)
//{
//	if(a + b > len)
//		return (a + b - len);
//	return len - (a + b);
//}

/*clean the struct obuf_t but do not free the memory*/
void obuf_clean(int head)
{
	obuf *h = _obuf_get_head(head);

	if(!h)
		return ;
	h->cnt = 0;
	h->p_rd = h->p_wrt = 0;
}

/*destory the buffer*/
void obuf_free(int head)
{
	obuf *h = _obuf_get_head(head);

	if(!h)
		return;
	obuf_clean(head);
	free(h->buf);
	h->buf = NULL;
}

/**
 * if buffer is empty
 */
int obuf_isempty(int head)
{
	obuf *h = _obuf_get_head(head);

	if(!h)
		return -1;

	return _obuf_isempty(h, 0);
}

/**
 * if buffer is empty when read readsz bytes
 */
static int _obuf_isempty(obuf *h, size_t readsz)
{
	return h->cnt <= readsz;
}

/**
 * if obuf is full
 * 1 full
 * 0 not full
 */
int obuf_isfull(int head)
{
	obuf *h = _obuf_get_head(head);

	if(!h)
		return -1;

	return _obuf_isfull(h, 0);
}

/**
 * test if obuf is full when write size bytes
 * 1 full
 * 0 not full
 */
static int _obuf_isfull(obuf *h, size_t wrtsz)
{
	return _obuf_is_overfllow(h, wrtsz + 1);
}

/**
 * test if obuf overfllow when write size bytes
 * 1 full
 * 0 not full
 */
static int _obuf_is_overfllow(obuf *h, size_t wrtsz)
{ 
	return h->cnt + wrtsz > h->len ? 1:0;
}
