#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

#define SIZE_OUT_HEADER (4*4)

static inline int call_gpi(int fd, int args_len, int ret_len, int ftn_num, char *args_buf)
{
	int ret = 0;
	//volatile int *i = (volatile int*)args_buf;
	int *i = (int*)args_buf;

	/* i[0] = pid; ...is filled in by the kernel module for virtio GL */
	i[1] = args_len;
	i[2] = ret_len;
	i[3] = ftn_num;

	fprintf(stdout, "args_len(%d), ret_len(%d), ftn_num(%d)  \n", i[1], i[2], i[3]);

	ret = write(fd, args_buf, args_len);
	if( ret != args_len) {
		perror("write fail");
		exit(0);
	}  

	return ret;
}

int main(int argc, char** argv)
{

	int fd;
	int ftn_num;
	char write_buf[4096] = {0};
	char read_buf[4096] = {0};


	if((fd = open("/dev/virt_gpi", O_RDWR)) == -1) {
		perror("open");
		exit(0);
	}

	if( argc >= 1 && argv[1] != NULL ){
		ftn_num = atoi(argv[1]);
	}else{
		exit(0);
	}

	if( argc >= 2 && argv[2] != NULL ){
		strcpy(write_buf, argv[2]);
	}

	if(call_gpi(fd, SIZE_OUT_HEADER + strlen(write_buf), sizeof(read_buf), ftn_num, write_buf) <= 0) {
		perror("call_gpi");
		exit(0);
	}

	if(read(fd, read_buf, sizeof(read_buf)) <= 0){
		perror("read");
		exit(0);
	}

	fprintf(stdout, "read_buf[%s] \n", read_buf);

	close(fd);
	return 0;
}


