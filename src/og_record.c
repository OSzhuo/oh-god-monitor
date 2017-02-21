#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>

#include "og_record.h"
#include "og_stack.h"
//#include "og_tree.h"
#include "obuf.h"

#define SED_NEW_STR		"$a\\%s///%c///%d"
#define SED_DEL_FILE		"/^%s\\/\\/\\/.*\\/\\/\\/.*$/d"
#define SED_DEL_DIR_f		"/^%s\\/.*\\/\\/\\/.*\\/\\/\\/.*$/d"
#define SED_DEL_DIR_d		"/^%s\\/\\/\\/D\\/\\/\\/.*$/d"

#define SED_PIC_NEW		"$a\\%s"
#define SED_PIC_SELF		"/^%s$/d"
#define SED_PIC_DEL_f		"/^%s\\/.*$/d"

static int glb_bd;
ogs_head *glb_dir_stack;

int _read_obuf(int bd, void *buf, size_t count);

/*if file exsit, will trunc!!!*/
int og_record_init(int bd)
{
	glb_bd = bd;
	//if((glb_fd = open(RUNTIME_LIST, O_RDWR | O_TRUNC | O_CREAT, 0644)) < 0 ){
	//	perror("open()");
	//	return -1;
	//}
	//char *p = "## RUNTIME.LIST ##\n";
	//write(glb_fd, p, strlen(p));
	//close(glb_fd);
	//int fd;
	//if((fd = open(PIC_RUN, O_RDWR | O_TRUNC | O_CREAT, 0644)) < 0 ){
	//	perror("open()");
	//	return -1;
	//}
	//p = "## PIC.RUN ##\n";
	//write(fd, p, strlen(p));
	//close(fd);

	return 0;
}

/**
 * just for init, init the stack to stor the last directory
 * 
 */
int og_init_start(void)
{
	ogs_head *head;

	head = ogs_init();
	glb_dir_stack = head;

	return 0;
}

/**
 * init ok, destory the stack
 */
int og_init_over(void)
{
	ogs_destory(glb_dir_stack, free);
	glb_dir_stack = NULL;
printf("destory directory stack\n");

	return 0;
}

int _trunc_path(_og_unit *buf, char *head)
{
	size_t len = strlen(head);
	if(strncmp(buf->path, head, len))
		return -1;

	buf->len -= len;
	memmove(buf->path, buf->path+len, buf->len);

	return 0;
}

int read_unit(int bd, _og_unit *buf)
{
	_read_obuf(bd, buf, sizeof(_og_unit));
	_read_obuf(bd, buf->path, buf->len);
	//printf("read from obuf[%s]\n", buf->path);
	//printf("[%c] st_size[%8ld] mtime[%ld] path[%s]\n", buf->type, buf->size, buf->mtime, buf->path);

	//_trunc_path(buf, "/tmp/mnt");

	return 0;
}

void *og_read_unit_from_obuf(void *x)
{
	_og_unit *p;

	if(NULL == (p = malloc(sizeof(_og_unit) + IBIG_PATH_MAX))){
		perror("malloc");
		exit(1);
	}

	while(1){
		//sleep(1);
		//printf("want read %d read %d bytes,", sizeof(struct list_line_t), obuf_read((int)bd, p, sizeof(struct list_line_t) + 4096, 1, NULL));
		//printf("want read %d read %d bytes,", sizeof(struct list_line_t), obuf_read((int)bd, p, sizeof(struct list_line_t), 1, NULL));
		//printf("want %d read %d bytes\n", p->len, obuf_read((int)bd, p->path, p->len, 1, NULL));
		read_unit(glb_bd, p);
		//_wrt_file(p);
	}

	return NULL;
}

//int _write_all(int fd, const void *buf, size_t size)
//{
//	int wrt = 0;
//	int r;
//
//	while(wrt < size){
//		if((r = write(glb_fd, buf+wrt, size - wrt)) < 0){
//			perror("write()");
//			return -1;
//		}
//		wrt += r;
//	}
//
//	return wrt;
//}


int _read_obuf(int bd, void *buf, size_t count)
{
	int c = 0;
	while(c < count)
		c += obuf_read(bd, buf + c, count - c, 1, NULL);

	return count;
}
