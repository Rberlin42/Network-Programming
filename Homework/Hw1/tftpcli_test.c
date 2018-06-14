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
#include <arpa/inet.h>

#define BUF_LEN 517
#define RRQ 1
#define WRQ 2
#define DATA 3
#define ACK 4
#define ERROR 5

int main(int argc, char * argv[]){
	ssize_t n;
    char buffer[BUF_LEN];
    socklen_t sockaddr_len;
    int server_socket;
    unsigned short int opcode;
    unsigned short int * opcode_ptr;
    struct sockaddr_in sock_info;
    struct sockaddr_in dest_sock_info;
    struct timeval timeout_interval;

    sockaddr_len = sizeof(dest_sock_info);
    memset(&dest_sock_info, 0, sockaddr_len);

    dest_sock_info.sin_addr.s_addr = inet_addr("127.0.0.1");
    dest_sock_info.sin_port = htons((rand() % 64511) + 1024);
    dest_sock_info.sin_family = PF_INET;
    
    if((server_socket = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket");
        exit(-1);
    }
    
    if(bind(server_socket, (struct sockaddr *)&dest_sock_info, sockaddr_len) < 0) {
        perror("bind");
        exit(-1);
    }

    dest_sock_info.sin_port = htons(atoi(argv[1]));

    opcode_ptr = (unsigned short *) buffer;
    *opcode_ptr = htons(RRQ);
    strcpy(&(buffer[2]), "a.txt");
    strcpy(&(buffer[8]), "binary");
    for(int i = 0; i < 25; i++){
        printf("%c\n", buffer[i]);
    }

    intr_send:
        n = sendto(server_socket, buffer, 5, 0, (struct sockaddr *)&dest_sock_info, sockaddr_len);
        if(n < 0) {
            if(errno == EINTR) goto intr_send;
    		perror("sendto");
            exit(-1);
        }

}