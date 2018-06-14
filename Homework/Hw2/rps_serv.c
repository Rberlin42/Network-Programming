#include <dns_sd.h>
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

// Note: the select() implementation on Windows (Winsock2)
//fails with any timeout much larger than this
#define LONG_TIME 100000000

struct clientData {
    int fd;
    char name[80];
    char move[10];
};

//returns 0 if its invalid, 1 if it is
//updates str to be all caps and removes surrounding whitespace
int isValid(char * string){
    //trim leading
    int i = 0;
    int j;
    while(i < strlen(string) && isspace(string[i]))
        i++;
    for(j = i; j < strlen(string); j++)
        string[j-i] = string[j];
    string[j-i] = '\0';

    //trim trailing
    i = strlen(string)-1;
    while(i >= 0 && isspace(string[i]))
        i--;
    string[i+1] = '\0';

    //check if its empty
    if(strlen(string) <= 0)
        return 0;

    //capitalize
    for(i = 0; i < strlen(string); i++)
        string[i] = toupper(string[i]);

    return 1;
}

static void MyRegisterCallBack(DNSServiceRef service,
                   DNSServiceFlags flags,
                   DNSServiceErrorType errorCode,
                   const char * name,
                   const char * type,
                   const char * domain,
                   void * context){
    #pragma unused(flags)
    #pragma unused(context)

    if (errorCode != kDNSServiceErr_NoError)
        fprintf(stderr, "MyRegisterCallBack returned %d\n", errorCode);
    else
        printf("%-15s %s.%s%s\n","REGISTER", name, type, domain);
}

//Determine a winner, send results to clients and disconnect
void playGame(struct clientData * clients){
    //determine the result
    char message[255];
    if(strcmp(clients[0].move, clients[1].move) == 0) //tie
        strcpy(message, "Tie!\n");
    if(strcmp(clients[0].move, "ROCK") == 0){
        if(strcmp(clients[1].move, "SCISSORS") == 0){
            strcpy(message, "ROCK smashes SCISSORS!  ");
            strcat(message, clients[0].name);
            strcat(message, " defeats ");
            strcat(message, clients[1].name);
            strcat(message, "!\n");
        }
        if(strcmp(clients[1].move, "PAPER") == 0){
            strcpy(message, "PAPER covers ROCK!  ");
            strcat(message, clients[1].name);
            strcat(message, " defeats ");
            strcat(message, clients[0].name);
            strcat(message, "!\n");
        }
    }
    if(strcmp(clients[0].move, "PAPER") == 0){
        if(strcmp(clients[1].move, "ROCK") == 0){
            strcpy(message, "PAPER covers ROCK!  ");
            strcat(message, clients[0].name);
            strcat(message, " defeats ");
            strcat(message, clients[1].name);
            strcat(message, "!\n");
        }
        if(strcmp(clients[1].move, "SCISSORS") == 0){
            strcpy(message, "SCISSORS cuts PAPER!  ");
            strcat(message, clients[1].name);
            strcat(message, " defeats ");
            strcat(message, clients[0].name);
            strcat(message, "!\n");
        }
    }
    if(strcmp(clients[0].move, "SCISSORS") == 0){
        if(strcmp(clients[1].move, "PAPER") == 0){
            strcpy(message, "SCISSORS cuts PAPER!  ");
            strcat(message, clients[0].name);
            strcat(message, " defeats ");
            strcat(message, clients[1].name);
            strcat(message, "!\n");
        }
        if(strcmp(clients[1].move, "ROCK") == 0){
            strcpy(message, "ROCK smashes SCISSORS!  ");
            strcat(message, clients[1].name);
            strcat(message, " defeats ");
            strcat(message, clients[0].name);
            strcat(message, "!\n");
        }
    }
    //send and disconnect
    if(send(clients[0].fd, message, strlen(message), 0) != strlen(message))
        perror("send");
    if(send(clients[1].fd, message, strlen(message), 0) != strlen(message))
        perror("send");
    close(clients[0].fd);
    close(clients[1].fd);
}

//perform client io
//returns true if the client disconnected
int clientIO(struct clientData * cli, char * buffer){
    int nc = read(cli->fd, buffer, 255);
    if(nc < 0) return 1;
    buffer[nc] = '\0';
    if(!strlen(cli->name)) { //name input
        if(isValid(buffer)){ //valid input
            strcpy(cli->name, buffer);
            if(send(cli->fd, "Rock, paper, or scissors?\n", 26, 0) != 26)  
                perror("send");
        }
        else{ //not valid, resend request
            if(send(cli->fd, "What is your name?\n", 19, 0) != 19)  
                perror("send");
        }
    }
    else{ //move input
        if(isValid(buffer) && (strcmp(buffer, "ROCK") == 0 || strcmp(buffer, "PAPER") == 0 || strcmp(buffer, "SCISSORS") == 0)) { //valid input
            strcpy(cli->move, buffer);
        }
        else{ //not valid, resend request
            if(send(cli->fd, "Rock, paper, or scissors?\n", 26, 0) != 26)  
                perror("send");
        }
    }
    return 0;
}

//main select loop
static volatile int stopNow = 0;
static volatile int timeOut = LONG_TIME;
void HandleEvents(DNSServiceRef serviceRef, int listen_fd){
    int dns_sd_fd = DNSServiceRefSockFD(serviceRef);
    int nfds = dns_sd_fd + 1;
    fd_set readfds;
    struct timeval tv;
    struct sockaddr_in address;
    int result, new_socket;
    int addrlen = sizeof(address);
    memset(&address, 0, addrlen);
    addrlen = sizeof(address);
    char buffer[255];

    //array to hold clients
    struct clientData clients[2];
    for(int i = 0; i < 2; i++){
        clients[i].fd = -1;
        strcpy(clients[i].name, "");
        strcpy(clients[i].move, "");
    }

    while (!stopNow) {
        //check if all the data is in
        if(strcmp(clients[0].name, "") != 0 && strcmp(clients[0].move, "") != 0 && strcmp(clients[1].name, "") != 0 && strcmp(clients[1].move, "") != 0){
            playGame(clients);
            clients[0].fd = -1;
            clients[1].fd = -1;
        }
        //reset the data if we dont have connection
        for(int i = 0; i < 2; i++){
            if(clients[i].fd == -1){
                strcpy(clients[i].name, "");
                strcpy(clients[i].move, "");
            }
        }

        FD_ZERO(&readfds);
        FD_SET(dns_sd_fd, &readfds);
        FD_SET(listen_fd, &readfds);
        tv.tv_sec = timeOut;
        tv.tv_usec = 0;

        //add clients to fdset
        if(clients[0].fd != -1){
            FD_SET(clients[0].fd, &readfds);
            if(clients[0].fd >= nfds)
                nfds = clients[0].fd+1;
        }        
        if(clients[1].fd != -1){
            FD_SET(clients[1].fd, &readfds);
            if(clients[1].fd >= nfds)
                nfds = clients[1].fd+1;
        }

        result = select(nfds, &readfds, NULL, NULL, &tv);
        if (result > 0){
            DNSServiceErrorType err = kDNSServiceErr_NoError;
            if (FD_ISSET(dns_sd_fd, &readfds))
                err = DNSServiceProcessResult(serviceRef);
            if (err)
                stopNow = 1;
            //new connection
            if(FD_ISSET(listen_fd, &readfds)) {
                if ((new_socket = accept(listen_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {  
                    perror("accept");  
                    exit(EXIT_FAILURE);  
                }  

                //check if the game is already full
                if(clients[0].fd != -1 && clients[1].fd != -1){
                    if(send(new_socket, "Game is full\n", 13, 0) != 13)
                        perror("send");
                    close(new_socket);
                }
                else{
                    //send new connection greeting message 
                    if(send(new_socket, "What is your name?\n", 19, 0) != 19)  
                        perror("send");   

                    if(clients[0].fd == -1)
                        clients[0].fd = new_socket;
                    else
                        clients[1].fd = new_socket;
                }
            }
            //user input
            if(clients[0].fd >= 0 && FD_ISSET(clients[0].fd, &readfds) && strcmp(clients[0].move, "") == 0){
                if(clientIO(&(clients[0]), buffer)){
                    close(clients[0].fd);
                    clients[0].fd = -1;
                }
            }
            if(clients[1].fd >= 0 && FD_ISSET(clients[1].fd, &readfds) && strcmp(clients[1].move, "") == 0){
                if(clientIO(&(clients[1]), buffer)){
                    close(clients[0].fd);
                    clients[0].fd = -1;
                }
            }
        }
        else {
            printf("select() returned %d errno %d %s\n", result, errno, strerror(errno));
            if (errno != EINTR) 
                stopNow = 1;
        }
    }
    printf("Done\n");
}

int main(int argc, char * argv[]){

    int server_socket;
    struct sockaddr_in sock_info;
    socklen_t sockaddr_len;
    int port;

    //set up a socket
    sockaddr_len = sizeof(sock_info);
    memset(&sock_info, 0, sockaddr_len);
    
    //pick a port
    srand(time(NULL));
    port = (rand() % 64511) + 1024;
    sock_info.sin_addr.s_addr = htonl(INADDR_ANY);
    sock_info.sin_port = htons(port);
    sock_info.sin_family = AF_INET;
  
    //create tcp socket, bind, and listen
    if((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(-1);
    }
    printf("%d\n",server_socket);
    if(bind(server_socket, (struct sockaddr *)&sock_info, sockaddr_len) < 0) {
        perror("bind");
        exit(-1);
    }
    if (listen(server_socket, 2) < 0) {  
        perror("listen");  
        exit(EXIT_FAILURE);  
    }  
    DNSServiceErrorType error;
    DNSServiceRef serviceRef;

    //register locally
    error = DNSServiceRegister(&serviceRef, 0, kDNSServiceInterfaceIndexLocalOnly, "berlir", "_rps._tcp", "", NULL, htons(port), 0, NULL, MyRegisterCallBack, NULL);        
    if (error == kDNSServiceErr_NoError){
        HandleEvents(serviceRef, server_socket);
        DNSServiceRefDeallocate(serviceRef);
    }

    return EXIT_SUCCESS;
}

