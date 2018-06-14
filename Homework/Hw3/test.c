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
#include <ctype.h>
#include <pthread.h>


struct Test{
	int foo;
};

void yuh(struct Test dug){
	dug.foo = 10;
}

int main(int argc, char * argv[]){

	char foo [50];
	strcpy(foo, "abc\ndef");
	printf("%s\n", foo);
	printf("%lu\n", strlen(foo));


	return EXIT_SUCCESS;
}