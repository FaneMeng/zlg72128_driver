#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <poll.h>
#include <linux/input.h>

#define INPUT_DEV	 "/dev/input/event1"

int main(int argc,char *argv[])
{
	int fd,ret;
	struct pollfd fds;
	struct input_event key;

	if(argc==2)
		fd = open(argv[1],O_RDONLY);
	else
		fd = open(INPUT_DEV,O_RDONLY);
	if(fd < 0)
	{
		perror("open dev");
		return 1;
	}

	fds.fd = fd;
	fds.events = POLLIN;

	while(1)
	{
		ret = poll(&fds,1,-1);
		if(ret < 0)
			return -1;

		if(fds.revents == POLLIN){

			read(fd,&key,sizeof(key));
			if(key.type == EV_KEY){
				if(key.value == 0 || key.value == 1){
					printf("key %d %s\n",key.code,(key.value)?"Press":"Release");
					if(key.code == KEY_ESC)
						break;
				}
			}
		}
	}

	close(fd);
	return 0;
}
