#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>

int main(){
	signal(SIGALRM, alarm_handler);
	test:
	alarm(3);
	printf("ug\n");
	while(1);
	printf("Hello\n");
	return EXIT_SUCCESS;
}

void alarm_handler(){
	printf("yuh\n");
	goto test;
}