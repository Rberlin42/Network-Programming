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

#define BUF_LEN 517
#define RRQ 1
#define WRQ 2
#define DATA 3
#define ACK 4
#define ERROR 5

//signal handler for SIGCHILD
void sig_child(int signo){
    pid_t pid;
    int stat;

    while((pid = waitpid(-1, &stat, WNOHANG)) > 0){
        printf("child %d terminatied\n", pid);
    }
}

//sendto wrapper function
int sendData(int server_socket, char * buffer, int len, struct sockaddr_in * sock_info){
    int n;
    try_send:
        n = sendto(server_socket, buffer, len, 0, (struct sockaddr *)sock_info, sizeof(*sock_info));
        if(n < 0) {
            if(errno == EINTR) goto try_send;
            perror("sendto");
            exit(-1);
        }
    return n;
}

void handle_read(int server_socket, struct sockaddr_in * dest_sock_info, char * buffer){
    ssize_t n = 0;
    int tid = ntohs(dest_sock_info->sin_port);
    unsigned short * opcode_ptr = (unsigned short *)buffer;
    char filename[80];
    FILE * fp;
    int block = 0;
    socklen_t sockaddr_len;
    char last_packet[BUF_LEN];
    int last_packet_size;
    int count = 0;

    //get file
    strcpy(filename, buffer+2);
    fp = fopen(filename, "r");
    if(fp == NULL){ //no such file
        *opcode_ptr = htons(ERROR);
        *(opcode_ptr+1) = htons(1);
        *(buffer+4) = 0;
        //send an error and terminate
        n = sendData(server_socket, buffer, 5, dest_sock_info);
        return;
    }

    while(1){
        block++;

        //construct packet
        *opcode_ptr = htons(DATA);
        *(opcode_ptr+1) = htons(block);
        for(n = 4; n < 516; n++){
            if(feof(fp)) break;
            buffer[n] = fgetc(fp);
        }

        //store packet
        for(int i = 0; i < BUF_LEN; i++)
            last_packet[i] = buffer[i];
        last_packet_size = n;

        //send
        n = sendData(server_socket, buffer, n, dest_sock_info);

        //Wait for ack
        try_rec:
        n = recvfrom(server_socket, buffer, BUF_LEN, 0, (struct sockaddr *)&dest_sock_info, &sockaddr_len);
        if(n < 0) {
            if(errno == EINTR) goto try_rec;
            if(errno == EWOULDBLOCK){//1 second timeout
                if(++count >= 10){
                    printf("transaction timed out\n");
                    break;
                }
                //restore last packet
                for(int i = 0; i < BUF_LEN; i++)
                    buffer[i] = last_packet[i];
                n = sendData(server_socket, buffer, last_packet_size, dest_sock_info);
                goto try_rec;
            }
            perror("recvfrom");
            exit(-1);
        }
        //check the tid
        if(ntohs(dest_sock_info->sin_port) != tid){
            //different client
            *opcode_ptr = htons(ERROR);
            *(opcode_ptr+1) = htons(5);
            *(buffer+4) = 0;
            n = sendData(server_socket, buffer, 5, dest_sock_info);
            goto try_rec;
        }

        //reset the timout counter
        count = 0;

        /* check the opcode */ 
        if(ntohs(*opcode_ptr) != ACK) {
            if(ntohs(*opcode_ptr) == RRQ){
                //restore last packet
                for(int i = 0; i < BUF_LEN; i++)
                    buffer[i] = last_packet[i];
                n = sendData(server_socket, buffer, last_packet_size, dest_sock_info);
            }
            goto try_rec;
        }
        if(ntohs(*(opcode_ptr+1)) != block)
            goto try_rec;

        if(last_packet_size < 516)
            break;
    }

    fclose(fp);
}

void handle_write(int server_socket, struct sockaddr_in * dest_sock_info, char * buffer){
    ssize_t n = 0;
    int tid = ntohs(dest_sock_info->sin_port);
    unsigned short * opcode_ptr = (unsigned short *)buffer;
    char filename[80];
    FILE * fp;
    int block = 0;
    socklen_t sockaddr_len;
    int more = 1;
    char last_packet[BUF_LEN];
    int last_packet_size;
    int last_block = 0;
    int count = 0;

    //get file
    strcpy(filename, buffer+2);
    fp = fopen(filename, "w");

    //Ack packet, block #0
    *opcode_ptr = htons(ACK);
    *(opcode_ptr + 1) = htons(block);

    //store contents of this packet
    for(int i = 0; i < BUF_LEN; i++)
        last_packet[i] = buffer[i];
    last_packet_size = 4;

    //send the first ack to begin receiving data
    n = sendData(server_socket, buffer, 4, dest_sock_info);

    while(more){
        n = recvfrom(server_socket, buffer, BUF_LEN, 0, (struct sockaddr *)&dest_sock_info, &sockaddr_len);
        if(n < 0) {
            if(errno == EINTR) continue;
            if(errno == EWOULDBLOCK){//1 second timeout
                if(++count >= 10){
                    printf("transaction timed out\n");
                    break;
                }
                //restore last packet
                for(int i = 0; i < BUF_LEN; i++)
                    buffer[i] = last_packet[i];
                n = sendData(server_socket, buffer, last_packet_size, dest_sock_info);
                continue;
            }
            perror("recvfrom");
            exit(-1);
        }
        //check the tid
        if(ntohs(dest_sock_info->sin_port) != tid){
            //different client
            *opcode_ptr = htons(ERROR);
            *(opcode_ptr+1) = htons(5);
            *(buffer+4) = 0;
            n = sendData(server_socket, buffer, 5, dest_sock_info);
            continue;
        }

        //reset the timout counter
        count = 0;

        /* check the opcode */ 
        if(ntohs(*opcode_ptr) != DATA) {
            if(ntohs(*opcode_ptr) == WRQ){
                //restore last packet
                for(int i = 0; i < BUF_LEN; i++)
                    buffer[i] = last_packet[i];
                n = sendData(server_socket, buffer, last_packet_size, dest_sock_info);
            }
            continue;
        }

        /* At this point, the tid has been verified and it is a DATA packet */

        block = ntohs(*(opcode_ptr+1));
        if(block == last_block){
            //restore last packet
            for(int i = 0; i < BUF_LEN; i++)
                buffer[i] = last_packet[i];
            n = sendData(server_socket, buffer, last_packet_size, dest_sock_info);
            continue;
        }

        buffer[n] = '\0';
        fprintf(fp, "%s\n", buffer+4);

        if(n < 516) //last packet
            more = 0;

        //send an ack
        *opcode_ptr = htons(ACK);
        *(opcode_ptr+1) = htons(block);

        //store contents of this packet
        for(int i = 0; i < BUF_LEN; i++)
            last_packet[i] = buffer[i];
        last_packet_size = 4;

        n = sendData(server_socket, buffer, 4, dest_sock_info);
    }    
    fclose(fp);  
}

int main() {
    ssize_t n;
    char buffer[BUF_LEN];
    socklen_t sockaddr_len;
    int server_socket;
    struct sigaction act;
    unsigned short int opcode;
    unsigned short int * opcode_ptr;
    struct sockaddr_in sock_info;
    struct sockaddr_in dest_sock_info;
    struct timeval timeout_interval;
    
    /* Set up interrupt handlers */
    act.sa_handler = sig_child;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    sigaction(SIGCHLD, &act, NULL);
    
    sockaddr_len = sizeof(dest_sock_info);
    
    /* Set up UDP socket */
    memset(&dest_sock_info, 0, sockaddr_len);
    
    srand(time(NULL));
    dest_sock_info.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_sock_info.sin_port = htons((rand() % 64511) + 1024);
    dest_sock_info.sin_family = AF_INET;
    
    if((server_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket");
        exit(-1);
    }
    
    if(bind(server_socket, (struct sockaddr *)&dest_sock_info, sockaddr_len) < 0) {
        perror("bind");
        exit(-1);
    }
    
    /* Get port and print it */
    getsockname(server_socket, (struct sockaddr *)&dest_sock_info, &sockaddr_len);

    printf("%d\n", ntohs(dest_sock_info.sin_port));
    
    /* Receive the first packet and deal w/ it accordingly */
    while(1) {
    intr_recv:
        n = recvfrom(server_socket, buffer, BUF_LEN, 0,
                     (struct sockaddr *)&dest_sock_info, &sockaddr_len);
        if(n < 0) {
            if(errno == EINTR) goto intr_recv;
            perror("recvfrom\n");
            exit(-1);
        }
        /* check the opcode */
        opcode_ptr = (unsigned short *)buffer;
        opcode = ntohs(*opcode_ptr);
        if(opcode != RRQ && opcode != WRQ) {
            /* Illegal TFTP Operation */
            *opcode_ptr = htons(ERROR);
            *(opcode_ptr + 1) = htons(4);
            *(buffer + 4) = 0;
        intr_send:
            n = sendto(server_socket, buffer, 5, 0,
                       (struct sockaddr *)&dest_sock_info, sockaddr_len);
            if(n < 0) {
                if(errno == EINTR) goto intr_send;
                perror("sendto");
                exit(-1);
            }
        }
        else {
            if(fork() == 0) {
                /* Child - handle the request */
                close(server_socket);
                break;
            }
            else {
                /* Parent - continue to wait */
            }
        }
    }

    int tid = (rand() % 64511) + 1024; //random number from 1024 to 65,535
    sock_info.sin_addr.s_addr = htonl(INADDR_ANY);
    sock_info.sin_port = htons(tid);
    sock_info.sin_family = PF_INET;

    if((server_socket = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket");
        exit(-1);
    }

    if(bind(server_socket, (struct sockaddr *)&sock_info, sockaddr_len) < 0) {
        perror("bind");
        exit(-1);
    }

    //set a 1 second timeout for receiving from the socket
    timeout_interval.tv_sec = 1;
    timeout_interval.tv_usec = 0;
    setsockopt(server_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout_interval, sizeof(timeout_interval));

    if(opcode == RRQ) handle_read(server_socket, &dest_sock_info, buffer);
    if(opcode == WRQ) handle_write(server_socket, &dest_sock_info, buffer);

    close(server_socket);
    
    return 0;
}
