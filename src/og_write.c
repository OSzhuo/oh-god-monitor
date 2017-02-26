#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
/* for unix domain socket */
#include <sys/socket.h>
#include <sys/un.h>

#include "og_write.h"

	og_sock_node *pub_sock_node;
	int glb_sock_fd;
	int glb_list_fd;
	int glb_pic_fd;

ssize_t __server_sendto(og_sock_node *node, const struct sockaddr_un *s_addr, socklen_t len);
ssize_t __write_all(int fd, const void *buf, size_t count);
int __write_file(og_sock_node *node);
int __type_handler(og_sock_node *node);
int _client_recv(struct sockaddr_un *s_addr, socklen_t *len);
int og_client_work();
int og_client_init(const char *path);

int main(void)
{
	og_client_init(OG_SOCK_FILE);
	og_client_work(OG_SOCK_FILE);

	return 0;
}

int og_client_init(const char *path)
{
	int unix_sock_fd;
//	struct sockaddr_un c_addr;

	if((unix_sock_fd = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0){
		perror("unix domain socket()");
		return -1;
	}

	//c_addr.sun_family = AF_UNIX;
	//strcpy(c_addr.sun_path, path);

	//if(bind(unix_sock_fd, (struct sockaddr *)&c_addr, sizeof(struct sockaddr_un)) < 0){
	//	perror("bind()");
	//	return -1;
	//}

	if(!(pub_sock_node = malloc(sizeof(og_sock_node)+OG_LINE_LEN))){
		perror("malloc pub_sock_node");
		return -1;
	}
	pub_sock_node->action = 0;
	glb_sock_fd = unix_sock_fd;
	if((glb_list_fd = open(RUNTIME_LIST, O_CREAT | O_TRUNC | O_RDWR, 0644)) < 0){
		perror("open runtime list failed");
		return -1;
	}
	if((glb_pic_fd = open(PIC_RUN, O_CREAT | O_TRUNC | O_RDWR, 0644)) < 0){
		perror("open pic list failed");
		return -1;
	}

	return 0;
}

/**
 * socket file path
 * 
 */
int og_client_work(const char *path)
{
	struct sockaddr_un s_addr;
	socklen_t len;

	s_addr.sun_family = AF_UNIX;
	strcpy(s_addr.sun_path, path);
	len = sizeof(struct sockaddr_un);
	int i;

	for(i = 0; i < 3; i++){
		pub_sock_node->action = OG_SOCK_GET;
		pub_sock_node->len = 0;
		__server_sendto(pub_sock_node, &s_addr, len);
		
		if(recvfrom(glb_sock_fd, pub_sock_node, sizeof(og_sock_node)+OG_LINE_LEN, 0, (struct sockaddr *)&s_addr, &len) <= 0){
perror("recvfrom()");
			continue;
		}
fprintf(stdout, "recv action[%d]\n", pub_sock_node->action);
		if(OG_SOCK_INIT == pub_sock_node->action){
			fprintf(stdout, "already in init...\n");
			return 0;
		}
		if(OG_SOCK_START == pub_sock_node->action){
			break;
		}
	}
	if(i == 3){
		fprintf(stderr, "i equal 3, so.... socket have failed!\n");
		return -1;
	}

	_client_recv(&s_addr, &len);

	return 0;
}

int _client_recv(struct sockaddr_un *s_addr, socklen_t *s_len)
{
printf("this\n");
	while(1){
		if(recvfrom(glb_sock_fd, pub_sock_node, sizeof(og_sock_node)+OG_LINE_LEN, 0, (struct sockaddr *)&s_addr, s_len) <= 0){
			return -1;
		}
		if(OG_SOCK_END == pub_sock_node->action){
			break;
		}
		fprintf(stdout, "get line[%s]\n", pub_sock_node->line);
		__write_file(pub_sock_node);
	}

	return 0;
}

int __type_handler(og_sock_node *node)
{
	if(TYPE_P != node->action){
		return -1;
	}

	__write_all(glb_pic_fd, node->line, node->len-1);
	__write_all(glb_pic_fd, "\n", 1);

	return 0;
}

int __write_file(og_sock_node *node)
{
	__type_handler(node);

	__write_all(glb_list_fd, node->line, node->len-1);
	__write_all(glb_list_fd, "\n", 1);

	return 0;
}

ssize_t __write_all(int fd, const void *buf, size_t count)
{
	return write(fd, buf, count);
}

ssize_t __server_sendto(og_sock_node *node, const struct sockaddr_un *s_addr, socklen_t len)
{
	return sendto(glb_sock_fd, node, offsetof(og_sock_node, line)+node->len, 0, (struct sockaddr *)s_addr, len);
}
