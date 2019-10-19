#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <signal.h>

#define MAXLINE 1000

int msg, cmd;	// command socket and messaging socjet
int chat = 0;
char target[100];

// SIGINT handler
void handler(int signo){

	char text[MAXLINE];
	if(chat) printf("Closing chat\n");
	sprintf(text, "close");
	write(cmd, text, strlen(text));
	chat = 0;

}

// unsigned char *str -> string to hash
// hash function
unsigned long hash(unsigned char *str){
	unsigned int result = 5381;
	unsigned char *p;

	p = (unsigned char *) str;

	while(*p != '\0'){
		result = (result << 5) + result + *p;
		++p;
	}

	return result;
}

// void *param -> not used
// function that reads commands from the Message socket
void *readerMsg(void *param){
	char buffer[MAXLINE];
	int ret;

	while(1){
		memset(buffer, 0, sizeof(buffer));
		ret = read(msg, buffer, MAXLINE);
		if(ret > 0){
			chat = 1;
			printf("%s", buffer);
		}
	}	
}

// char *string -> string to check
// function that check the presence of blanks in a string
int checkSpaces(char *string){
	int i;
	for(i = 0; i < strlen(string); i++){
		if(string[i] == ' '){
			return 1;
		}
	}
	return 0;
}

// void *param -> not used
// function that reads commands from the Command socket
void *readerCmd(void *param){

	char buffer[MAXLINE];
	int ret;

	while(1){
		memset(buffer, 0,sizeof(buffer));
		ret=read(cmd,&buffer,MAXLINE);
		if(ret > 0 && strncmp(buffer, "change", 6) != 0){
			printf("%s",buffer);
		}
		else if(ret == -1){
			exit(EXIT_FAILURE);
		}
		if(strncmp(buffer,"change",6)==0){
			chat=0;
			printf("user disconnected\n");
		} else if(strncmp(buffer, "quit", 4) == 0){
			printf("Server shutdown\nexiting\n");
			exit(0);
		}
	}
}


// void *param -> not used
// function that send strings to the server
void *writer(void *param){

	char buffer[MAXLINE]; 
	int ret;
	int c;

	while(1){

		ret=scanf("%[^\n]",buffer);
		c=getchar();
		if(ret==-1){
			printf("scanf error\n");
			exit(EXIT_FAILURE);
		}

		if(strcmp(buffer,"quit")==0){	// sending special string to the server and terminating client
			printf("Closing...\n");

			write(cmd,buffer,strlen(buffer));
			write(msg,buffer,strlen(buffer));


			if(close(cmd)<0 || close(msg)<0){
				printf("errore chiusura socket\n");
				exit(EXIT_FAILURE);
			}
			else{
				exit(0);
			}

		}else if(chat){
			//printf("Sending msg\n");
			ret=write(msg,buffer,sizeof(buffer));	// sending message
		}
		else{
			//printf("Sending cmd\n");
			ret=write(cmd,buffer,sizeof(buffer));	// sending message
		}

		if(ret<=0) {
			printf("write error\n");	
			exit(EXIT_FAILURE);
		}



		memset(buffer,0,sizeof(char)*(strlen(buffer)+1));

	}

}

// function used during register and login procedures
void logOrReg(){

	char buffer[MAXLINE];
	char c;

	while(1){
		memset(buffer,0,sizeof(buffer));

		printf("Type reg to register\nType log to login\n");
		scanf("%[^\n]",buffer);
		c=getchar();
		write(cmd,buffer,strlen(buffer));

		if(strcmp(buffer,"reg")==0){

redUser:
			printf("Write your username, it must not contain blanks\n");

			scanf("%[^\n]", buffer);
			c = getchar();

			if(checkSpaces(buffer) == 1){
				printf("Invalid username\n");
				goto redUser;
			}

			write(cmd, buffer, strlen(buffer));
			
			memset(buffer, 0, sizeof(buffer));
			printf("Write your password\n");
			scanf("%[^\n]", buffer);
			c = getchar();

			sprintf(buffer, "%lu", hash(buffer));

			write(cmd, buffer, strlen(buffer));
			memset(buffer, 0, sizeof(buffer));
			
			read(cmd, buffer, MAXLINE);
			printf("%s\n", buffer);
			

		}

		else if(strcmp(buffer,"log")==0){

			printf("write your username\n");

			scanf("%[^\n]",buffer);
			c = getchar();

			write(cmd,buffer,strlen(buffer));

			memset(buffer,0,sizeof(buffer));	
			printf("write your password\n");
			scanf("%[^\n]",buffer);
			c = getchar();

			sprintf(buffer, "%lu", hash(buffer));

			write(cmd,buffer,strlen(buffer));
			memset(buffer,0,sizeof(buffer));

			read(cmd,buffer,MAXLINE);

			printf("%s \n",buffer);
			if(strncmp(buffer, "wrong", 5) == 0 || strncmp(buffer, "already", 7) == 0){
				printf("Log error\n");
				exit(-1);
			}
			printf("Connected to server\n\n");

			pthread_t tid;

			if(pthread_create(&tid,NULL,&readerCmd,NULL)!=0){
				printf("reader thread creation error\n");
				exit(EXIT_FAILURE);
			}

			if(pthread_create(&tid,NULL,&writer,NULL)!=0){
				printf("writer thread creation error\n");
				exit(EXIT_FAILURE);
			}

			if(pthread_create(&tid,NULL,&readerMsg,NULL)!=0){
				printf("reader thread creation error\n");
				exit(EXIT_FAILURE);
			}

			while(1) pause();

		} else if(strcmp(buffer,"quit")==0) {
			close(cmd);
			close(msg);
			printf("Closing all\n");
			exit(0);	

		} else printf("Command not recognised\n");
	}
}

// argv[1] IP address, argv[2] port
int main(int argc,char *argv[]){

	struct sockaddr_in server;
	struct hostent *hn;
	char buffer[MAXLINE];
	char temp[100];
	char c;

	int porta;

	if(argc<3){
		printf("Wrong usage\nclient [IP] [port]\n");
		exit(EXIT_FAILURE);
	}

	signal(SIGINT,handler);	// manage SIGINT

	printf("Translating port number...\n");
	porta=strtol(argv[2],NULL,10);
	if(porta<=0){
		printf("Invalid port\n");
		exit(EXIT_FAILURE);
	}
	printf("Port number obtained succesfully\n");

	printf("Creating socket...\n");
	if((cmd=socket(AF_INET,SOCK_STREAM,0))<0){
		printf("socket creation error\n");
		exit(EXIT_FAILURE);
	}
	if((msg=socket(AF_INET,SOCK_STREAM,0))<0){
		printf("socket creation error\n");
		exit(EXIT_FAILURE);
	}
	printf("Socket creation succesfull\n");

	memset(&server,0,sizeof(server));
	server.sin_family=AF_INET;
	server.sin_port=htons(porta);

	printf("Translating IP address...\n");
	if(inet_aton(argv[1],&server.sin_addr)<=0){
		printf("Invalid IP\nresolving name...\n");

		if((hn=gethostbyname(argv[1]))==NULL){
			printf("failed to resolve\n");
			exit(EXIT_FAILURE);
		}
		printf("resolution complete\n");
		server.sin_addr=*((struct in_addr*)hn->h_addr);
	}
	printf("IP obtained succesfully\n");

	printf("Connecting...\n");
	if(connect(cmd,(struct sockaddr *)&server,sizeof(server))<0){
		printf("connection error\n");
		exit(EXIT_FAILURE);
	}

	sprintf(temp, "sockCmd"); // control string to check if the two socket belong to the same client
	write(cmd, temp, strlen(temp));

	read(cmd,buffer,MAXLINE);
	printf("%s",buffer);
	memset(buffer,0,sizeof(buffer));

	if(connect(msg,(struct sockaddr *)&server,sizeof(server))<0){
		printf("connection error\n");
		exit(EXIT_FAILURE);
	}

	sprintf(temp, "sockMsg");
	write(msg, temp, strlen(temp));

	read(cmd,buffer,MAXLINE);
	printf("%s",buffer);
	if(strncmp(buffer, "error:", 6) == 0){
		exit(-1);
	}
	memset(buffer, 0, sizeof(buffer));

	logOrReg();

} 
