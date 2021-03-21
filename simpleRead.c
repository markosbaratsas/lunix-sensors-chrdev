#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>

int main(int argc, char **argv)
{
	if(argc != 2) {
		printf("      Usage: %s file_to_read_from\n", argv[0]);
		return 1;
	}
	char buff[1024];
	ssize_t rcnt;
	int fd = open(argv[1], O_RDONLY);
	if (fd ==-1) 
	{ 
		perror(argv[1]);                  
		return 1;
	} 
	for (;;){
		rcnt = read(fd,buff,sizeof(buff)-1);
		if (rcnt == 0) /* end\u2010of\u2010file */
			return 0;
		if (rcnt == -1){ /* error */
			perror("read");
			return 1;
		}
		buff[rcnt] = '\0';
		fprintf(stdout, "%s", buff);
	}
}
