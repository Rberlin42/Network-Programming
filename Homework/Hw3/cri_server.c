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

struct User{
	int fd;
	char name[20];
	int operator;
};

struct Channel{
	char name[20];
	struct User * users;
	int maxUsers;
	int numUsers;
}; 

void quit(struct User * user, char * arg1, char * arg2, char * arg3);

//some global variables for the threads to use
char password[20];
struct User * users;
int num_users = 0;
int max_users = 10;
struct Channel * channels;
int num_channels = 0;
int max_channels = 10;

//push a user onto the array
void push_user(struct User user, struct User ** users_, int * nusers, int * musers){
	//check if we need to realloc
	if(*nusers == *musers){
		*users_ = realloc(*users_, (*musers+10) * sizeof(struct User));
		*musers += 10;
	}
	(*users_)[*nusers] = user;
	(*nusers)++;
}
//push a channel onto the array
void push_channel(struct Channel channel){
	//check if we need to realloc
	if(num_channels == max_channels){
		channels = realloc(channels, (max_channels+10) * sizeof(struct Channel));
		max_channels += 10;
	}
	channels[num_channels] = channel;
	num_channels++;
}
//pop a user off the array
//return true if the user was there
int pop_user(char * name, struct User ** users_, int * nusers){
	int found = 0;
	for(int i = 0; i < *nusers; i++){
		if(strcmp(name, (*users_)[i].name) == 0){
			found = 1;
			(*nusers)--;
			if(i == *nusers) break;
		}
		if(found)
			(*users_)[i] = (*users_)[i+1];
	}
	return found;
}

//find a user or channel
struct Channel * findChannel(char * name){
	if(name == NULL) return NULL;
	struct Channel * channel = NULL;
	for(int i = 0; i < num_channels; i++){
		if(strcmp(name, channels[i].name) == 0){
			channel = &channels[i];
			break;
		}
	}
	return channel;
}
struct User * findUser(char * name, struct User * users_, int size){
	if(name == NULL) return NULL;
	struct User * user = NULL;
	for(int i = 0; i < size; i++){
		if(strcmp(name, users_[i].name) == 0){
			user = &users_[i];
			break;
		}
	}
	return user;
}

//Wrapper functions that will disconnect if there are errors
int Read(struct User * user, char * buffer, size_t nbyte){
	int nc;
	read:
	if((nc = read(user->fd, buffer, nbyte)) == -1){
		if(errno == EINTR){
			goto read;
		}
		else {
			quit(user, "", "", "");
		}
	}
	return nc;
}
void Send(struct User * user, char * buffer, size_t length){
	if(send(user->fd, buffer, length, 0) != length) 
		quit(user, "", "", "");
}

//parse the given buffer for a command
//return -1 if invalid num/len of args, or 0 if good
int parseInput(char * buf, int size, char * command, char * arg1, char * arg2, char * message){
	if(buf[size-1] != '\0'){
		buf[size] = '\0';
		size--;
	}
	int i, j, k;
	command[0] = '\0';
	arg1[0] = '\0';
	arg2[0] = '\0';
	message[0] = '\0';

	if(size == 0) return -1;

	//trim trailing
    i = strlen(buf)-1;
    while(i >= 0 && isspace(buf[i]))
        i--;
    buf[i+1] = '\0';
    size = i+1;

	//find the command
	for(i = 0; i <= size; i++){
		if(isspace(buf[i]) || buf[i] == '\0'){
			buf[i] = '\0';
			if(strlen(buf) > 20)
				return -1;
			strcpy(command, buf);
			break;
		}
	}
	i++;
	//find first arg if its there
	for(j = i; j <= size; j++){
		if(isspace(buf[j]) || buf[j] == '\0'){
			buf[j] = '\0';
			if(strlen(&buf[i]) > 20)
				return -1;
			strcpy(arg1, &buf[i]);
			break;
		}
	}
	j++;
	if(strcmp("PRIVMSG", command) == 0){
		k = j;
		//copy the message if it's there
		if(strlen(&buf[k]) > 512)
			return -1;
		strcpy(message, &buf[k]);
		if(strlen(message)+k < size)
			return -1;
	}
	else{
		//find second arg if its there
		for(k = j; k <= size; k++){
			if(isspace(buf[k]) || buf[k] == '\0'){
				buf[k] = '\0';
				if(strlen(&buf[j]) > 20)
					return -1;
				strcpy(arg2, &buf[j]);
				break;
			}
		}
		k++;
	}
	return 0;
}

//checks if the string matches the regex: [a-zA-Z][_0-9a-zA-Z]*
//hashtag in front if set
//must have length of 1 (or 2 if hashtag)
int matchesRegex(char * string, int hashtag){
	int i = 0;
	if(strlen(string) < 1) return 0;
	//check for a hashtag
	if(hashtag){
		if(strlen(string) < 2) return 0;
		if(string[i] != '#')
			return 0;
		i++;
	}
	//check the first char is [a-zA-Z]
	if(!isalpha(string[i])) return 0;
	i++;
	//check the rest
	for(; i < strlen(string); i++)
		if(!(isalnum(string[i]) || string[i] == '_'))
			return 0;
	return 1;
}

//////////////////////////////////////////////////////////////////////////////////
//                       COMMAND FUNCTIONS                                      // 
//////////////////////////////////////////////////////////////////////////////////

// USER <nickname>
void user(struct User * user, char * nickname, char * arg2, char * arg3){
	if(matchesRegex(nickname, 0) && strlen(arg2)+strlen(arg3) == 0 && findUser(nickname, users, num_users) == NULL){
		//add to list
		strcpy(user->name, nickname);
		push_user(*user, &users, &num_users, &max_users);

		//send welcome message
		char message[30];
		sprintf(message, "Welcome, %s\n", nickname);
		Send(user, message, strlen(message));
	}
	else{
		send(user->fd, "Invalid command, please identify yourself with USER.\n", 53, 0);
        close(user->fd);
        pthread_exit(NULL);
	}	
}

// LIST [#channel]
void list(struct User * user, char * channel, char * arg2, char * arg3){
	if(strlen(arg2)+strlen(arg3) != 0){
		Send(user, "Invalid command.\n", 17);
		return;
	}

	char msg[255];
	struct Channel * ch = findChannel(channel);
	if(ch == NULL){
		//list all channels
		sprintf(msg, "There are currently %d channels.\n", num_channels);
		for(int i = 0; i < num_channels; i++){
			if(strlen(msg) > 230){ //flush the buffer
				Send(user, msg, strlen(msg));
				msg[0] = '\0';
			}
			strcat(msg, "* ");
			strcat(msg, &channels[i].name[1]);
			strcat(msg, "\n");
		}
	}
	else{
		//list members of channel
		sprintf(msg, "There are currently %d members.", ch->numUsers);
		if(ch->numUsers > 0) {
			strcat(msg, "\n");
			strcat(msg, channel);
			strcat(msg, " members:");
		}
		for(int i = 0; i < ch->numUsers; i++){
			if(strlen(msg) > 230){ //flush the buffer
				Send(user, msg, strlen(msg));
				msg[0] = '\0';
			}
			strcat(msg, " ");
			strcat(msg, (ch->users)[i].name);
		}
		strcat(msg, "\n");
	}

	//send message
	Send(user, msg, strlen(msg));
}

// JOIN <#channel>
void join(struct User * user, char * channel, char * arg2, char * arg3){
	if(strlen(arg2)+strlen(arg3) != 0){
		Send(user, "Invalid command.\n", 17);
		return;
	}

	struct Channel * ch = findChannel(channel);
	if(ch == NULL){
		if(!matchesRegex(channel, 1)){
			Send(user, "Invalid command.\n", 17);
			return;
		}
		//new channel
		struct Channel newChannel;
		strcpy(newChannel.name, channel);
		newChannel.users = calloc(5, sizeof(struct User));
		newChannel.maxUsers = 5;
		newChannel.numUsers = 0;
		push_channel(newChannel);
		ch = &channels[num_channels-1];
	}

	//check if the user is already in the channel
	if(findUser(user->name, ch->users, ch->numUsers)) return;

	//send a welcome message
	char msg[100];
	sprintf(msg, "Joined channel %s\n", channel);
	Send(user, msg, strlen(msg));

	//send a message to current members
	sprintf(msg, "%s> %s joined the channel\n", channel, user->name);
	for(int i = 0; i < ch->numUsers; i++)
		Send(&(ch->users[i]), msg, strlen(msg));

	//add user to channel
	push_user(*user, &(ch->users), &(ch->numUsers), &(ch->maxUsers));
}

// PART [#channel]
void part(struct User * user, char * channel, char * arg2, char * arg3){
	if(strlen(arg2)+strlen(arg3) != 0){
		Send(user, "Invalid command.\n", 17);
		return;
	}

	char msg[100];
	struct Channel * ch = findChannel(channel);
	if(strlen(channel) == 0){
		//remove from all
		for(int i = 0; i < num_channels; i++){
			if(pop_user(user->name, &(channels[i].users), &(channels[i].numUsers))){
				//user was removed, notify the rest of the channel
				sprintf(msg, "%s> %s left the channel.\n", channels[i].name, user->name);
				for(int j = 0; j < channels[i].numUsers; j++)
					Send(&(channels[i].users[j]), msg, strlen(msg));
				Send(user, msg, strlen(msg));
			}
		}
	}
	else if(ch == NULL || findUser(user->name, ch->users, ch->numUsers) == NULL){
		//not in channel or it doesn't exist
		sprintf(msg, "You are not currently in %s\n", channel);
		Send(user, msg, strlen(msg));
	}
	else{
		//remove from channel
		sprintf(msg, "%s> %s left the channel.\n", ch->name, user->name);
		for(int i = 0; i < ch->numUsers; i++)
			Send(&(ch->users[i]), msg, strlen(msg));
		pop_user(user->name, &(ch->users), &(ch->numUsers));
	}
}

// OPERATOR <password>
void operator(struct User * user, char * pass, char * arg2, char * arg3){
	if(strlen(arg2)+strlen(arg3) != 0){
		Send(user, "Invalid command.\n", 17);
		return;
	}
	if(strlen(password) == 0){
		//no password set
		Send(user, "OPERATOR status not available.\n", 31);
		return;
	}
	if(strcmp(password, pass) == 0){
		user->operator = 1;
		Send(user, "OPERATOR status bestowed.\n", 26);
	}
	else{
		Send(user, "Invalid OPERATOR command.\n", 26);
	}
}

// KICK <#channel> <user>
void kick(struct User * user, char * channel, char * name, char * arg3){
	if(strlen(arg3) != 0 || strlen(channel) == 0 || strlen(name) == 0){
		Send(user, "Invalid command.\n", 17);
		return;
	}
	if(!user->operator){
		Send(user, "You must have OPERATOR status.\n", 31);
		return;
	}

	char msg[100];
	struct Channel * ch = findChannel(channel);
	struct User * kick_user = findUser(name, users, num_users);
	if(kick_user == NULL){
		//check if the user exists
		sprintf(msg, "\"%s\" is not on the server.\n", name);
		Send(user, msg, strlen(msg));
		return;
	}
	if(ch == NULL || findUser(name, ch->users, ch->numUsers) == NULL){
		//not in channel or it doesn't exist
		sprintf(msg, "%s is not currently in %s\n", name, channel);
		Send(user, msg, strlen(msg));
		return;
	}

	//send a message and remove
	sprintf(msg, "%s> %s has been kicked from the server.\n", ch->name, kick_user->name);
	for(int i = 0; i < ch->numUsers; i++)
		Send(&(ch->users[i]), msg, strlen(msg));
	pop_user(name, &(ch->users), &(ch->numUsers));
}

// PRIVMSG ( <#channel> | <user> ) <message>
void privmsg(struct User * user, char * dest, char * arg2, char * message){
	if(strlen(arg2) > 0 || strlen(dest) == 0){
		Send(user, "Invalid command.\n", 17);
		return;
	}

	char msg[550];
	if(dest[0] == '#'){
		//send to a channel
		struct Channel * ch = findChannel(dest);
		if(ch == NULL || findUser(user->name, ch->users, ch->numUsers) == NULL){
			//not in channel or it doesn't exist
			sprintf(msg, "You are not currently in %s\n", dest);
			Send(user, msg, strlen(msg));
		}
		else{
			sprintf(msg, "%s> %s: %s\n", ch->name, user->name, message);
			for(int i = 0; i < ch->numUsers; i++)
				Send(&(ch->users[i]), msg, strlen(msg));
		}
	}
	else{
		//send to a user
		struct User * dest_user = findUser(dest, users, num_users);
		if(dest_user == NULL){
			//check if the user exists
			sprintf(msg, "\"%s\" is not on the server.\n", dest);
			Send(user, msg, strlen(msg));
		}
		else{
			sprintf(msg, "%s> %s\n", user->name, message);
			Send(dest_user, msg, strlen(msg));
		}
	}
}	

// QUIT
void quit(struct User * user, char * arg1, char * arg2, char * arg3){
	if(strlen(arg1)+strlen(arg2)+strlen(arg3) > 0){
		Send(user, "Invalid command.\n", 17);
		return;
	}
	pop_user(user->name, &users, &num_users);
	close(user->fd);
	part(user, "", "", "");
	pthread_exit(NULL);
}	



//function for a thread to listen to a user's inputs
void * handle_user(void * arg){
	struct User user_;
	user_.operator = 0;
	user_.fd = *(int *)arg;
	char buffer[600];
	char command[20];
	char arg1[20];
	char arg2[20];
	char message[512];
	int nc;

	//attempt to read first message
	//disconnect if it's not USER
	try_read:
	if((nc = read(user_.fd, buffer, 600)) == -1){
		if(errno == EINTR) {
			goto try_read;
		}
		else{
			//user disconnected
			close(user_.fd);
			pthread_exit(NULL);
		}
	}
	if(parseInput(buffer, nc, command, arg1, arg2, message)){
		send(user_.fd, "Invalid command, please identify yourself with USER.\n", 53, 0);
        close(user_.fd);
        pthread_exit(NULL);
	}
	if(strcmp(command, "USER") != 0){
		send(user_.fd, "Invalid command, please identify yourself with USER.\n", 53, 0);
        close(user_.fd);
        pthread_exit(NULL);
	}
	user(&user_, arg1, arg2, message);

	//loop for input
	while(1){
		nc = Read(&user_, buffer, 600);
		//parse the input for the command and arguments
		if(parseInput(buffer, nc, command, arg1, arg2, message)){
			Send(&user_, "Invalid command.\n", 17);
            continue;
		}

		//check command and handle appropriately
		if(strcmp(command, "LIST") == 0)
			list(&user_, arg1, arg2, message);
		else if(strcmp(command, "JOIN") == 0)
			join(&user_, arg1, arg2, message);
		else if(strcmp(command, "PART") == 0)
			part(&user_, arg1, arg2, message);
		else if(strcmp(command, "OPERATOR") == 0)
			operator(&user_, arg1, arg2, message);
		else if(strcmp(command, "KICK") == 0)
			kick(&user_, arg1, arg2, message);
		else if(strcmp(command, "PRIVMSG") == 0)
			privmsg(&user_, arg1, arg2, message);
		else if(strcmp(command, "QUIT") == 0)
			quit(&user_, arg1, arg2, message);
		else 
			Send(&user_, "Invalid command.\n", 17);
	}

	pthread_exit(NULL);
}



int main(int argc, char * argv[]){

	int server_socket, new_socket;
    socklen_t sockaddr_len;
    int port;
    pthread_t tid;
    struct sockaddr_in6 sock_info;

    //initialize storage structures
    users = calloc(10, sizeof(struct User));
    channels = calloc(10, sizeof(struct Channel));

    //check user input
    if(argc == 1)
    	password[0] = '\0';
    if(argc == 2){
    	if(strncmp(argv[1], "--opt-pass=", 11) != 0){
    		fprintf(stderr, "Invalid usage\n");
    		return EXIT_FAILURE;
    	}
    	if(matchesRegex(&(argv[1][11]), 0)){
    		strcpy(password, &(argv[1][11]));
    	}
    	else{
    		fprintf(stderr, "Startup failed - invalid password.\n");
    		return EXIT_FAILURE;
    	}
    }
    if(argc > 2){
    	fprintf(stderr, "Invalid usage\n");
    	return EXIT_FAILURE;
    }

    //set up a socket
    sockaddr_len = sizeof(sock_info);
    memset(&sock_info, 0, sockaddr_len);
    
    //pick a port
    srand(time(NULL));
    port = (rand() % 64511) + 1024;
    sock_info.sin6_addr = in6addr_any;
    sock_info.sin6_port = htons(port);
    sock_info.sin6_family = AF_INET6;
  
    //create tcp socket, bind, and listen
    if((server_socket = socket(AF_INET6, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(-1);
    }
    const int on = 0;
    setsockopt(server_socket, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof(on));
    if(bind(server_socket, (struct sockaddr *)&sock_info, sockaddr_len) < 0) {
        perror("bind");
        exit(-1);
    }
    if (listen(server_socket, 2) < 0) {  
        perror("listen");  
        exit(EXIT_FAILURE);  
    } 
    printf("%d\n", port);

    //loop and wait for connections
    while(1){
    	if((new_socket = accept(server_socket, (struct sockaddr *)&sock_info, (socklen_t *)&sockaddr_len)) < 0){
    		if(errno == EINTR) continue;
    		perror("accept");
    		return EXIT_FAILURE;
    	}
    	if(pthread_create(&tid, NULL, handle_user, (void *)&new_socket) != 0){
    		fprintf(stderr, "ERROR: Could not create thread\n");
    		return EXIT_FAILURE;
    	}
    }

	return EXIT_SUCCESS;
}