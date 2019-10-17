#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/sem.h>
#include <semaphore.h>
#include <fcntl.h>
#define MAXLINE 1000	// maximum lenght of a message

struct user{
	char username[100];	// username AGGIUNTO PARAMETRI MA LEVATO IP
	char pass[100]; //password
	int sockMsg;	// user socket for msg
	int sockCmd; //user socket for cmd
	int state; // state of the user: 0 if free, 1 if busy

	struct user *target;	// target user
	struct user *next; // used to create connected list
};

struct user *list; // list of all the connected users
int semList; // semaphore of the list
int semFile; // semaphore of the file
int fd;
FILE *f;


// struct user **head -> head of the list
// char *ip -> new user ip
// int socket -> socket of the new user
//AGGIUNTO PARAMETRI
struct user *addUser(struct user **head, struct user *temp){	// function used to add a user to the list ADDED PASS USER SOCK CMD
	struct user *aux = temp;
	aux -> state = 0;	// setting user state to free
	aux -> next = *head;	// inserting user to the top of the list
	*head = aux;

	return aux;
}

//return pointer of the target
struct user *findTarget(char *name){

	struct user *aux=list;

	while(aux!=NULL){
		if(strcmp(name,aux->username)==0){
			return aux;
		}
		aux=aux->next;
	}

	return NULL;
}

//change target of name in tar
void changeTarget(char *name,struct user *tar){

	struct user *aux=list;

	while(aux!=NULL){

		if(strcmp(aux->username,name)==0){
			aux->target=tar;
			return;
		}
		aux=aux->next;
	}

}

// int socket -> socket of the requesting user
void printUsers(int sockCmd){	// function used to print all users connected to the server
	struct user *aux;
	char temp[MAXLINE];

	// printing all users in the list
	aux = list;
	sprintf(temp, "*******Connected Users*******\n");
	write(sockCmd, temp, strlen(temp));
	while(aux -> next != NULL){
		sprintf(temp, "Name: %s, socket MSG: %d, state: %d\n", aux -> username, aux -> sockMsg,aux -> state); //ADDED USERN AND ANOTHER SOCKET
		write(sockCmd, temp, strlen(temp));//WRITE ON SOCKET CMD
		aux = aux -> next;
	}
	sprintf(temp, "*****************************\n");
	write(sockCmd, temp, strlen(temp));
}

// int socket -> socket CMD of the requesting user
void printAvailableUsers(int sockCmd,char *name){	// function used to print all users with state = 0 NAME ADDED
	struct user *aux;
	char temp[MAXLINE];

	aux = list;
	sprintf(temp, "*******Available Users*******\n");
	write(sockCmd, temp, strlen(temp));

	while(aux -> next != NULL){
		if(aux -> state == 0 && strcmp(aux->username, name) != 0){	// check if the state of the user is 0 and if the user is not the requesting user WITH NAME 
			sprintf(temp, "Name: %s, socket MSG: %d\n", aux -> username, aux ->sockMsg);
			write(sockCmd, temp, strlen(temp));
		}
		aux = aux -> next;
	}

	sprintf(temp, "*****************************\n");
	write(sockCmd, temp, strlen(temp));
}

// struct user **head -> head of the list
// int socket -> socket of the user to remove
void removeUser(struct user **head, char *name){ // function used to remove a user from the server CONTROL ON NAME NOT SOCKET ID
	struct user *aux;
	struct user *prec, *succ;
	// removing user from the list

	if(strcmp((*head)->username,name)==0){ //CONTROL NAME
		prec = *head;
		*head = (*head) -> next;
		close(prec->sockMsg);  
		close(prec->sockCmd); //CLOSE TWO SOCKETS
		free(prec);
		return;
	}

	aux = (*head) -> next;
	prec = *head;
	while(aux != NULL){
		succ = aux -> next;
		if(strcmp(/*(*head)*/aux->username,name)==0){ //CONTROL NAME
			close(aux->sockCmd);	// closing Command socket
			close(aux->sockMsg);    // closing Message socket
			free(aux);	// freeing memory
			prec -> next = succ;
			return;
		}
		prec = aux;
		aux = aux -> next;
	}
}

// int socket -> target socket
int freeUser(char *name){	// function used to check the state of a target user
	struct user *aux;
	aux = list;

	while(aux != NULL){
		if(strcmp(name,aux->username)==0){
			if(aux -> state == 0){
				return 1;
			} else return 0;
		}
		aux = aux -> next;
	}
	return 0;
}

// int socket -> target socket
// int newState -> new state
void changeState(char *name, int newState){	// function used to set state of a target to newState
	struct user *aux;
	aux = list;

	while(aux != NULL){
		if(strcmp(name,aux->username)==0){
			aux -> state = newState;
			return;
		}
		aux = aux -> next;
	}
}

void interupt_handler(int signo){	// handler of SIGINT
	char quit[5];
	struct user *aux, *temp;
	aux = list;

	sprintf(quit, "quit");
	// closing all the sockets and freeing memory
	while(aux != NULL){
		write(aux -> sockCmd, quit, strlen(quit));
		close(aux -> sockMsg);
		close(aux -> sockCmd); //CLOSE OTHER SOCKET
		temp = aux;
		aux = aux -> next;
		free(temp);

	}

	fflush(f);
	close(fd);
	exit(0);

}


//CHECK IF IS ALREADY LOGGED
int alreadyLogged(char *name){

	struct user *aux;
	aux = list;

	while(aux != NULL){
		if( strcmp(name, aux->username) == 0){ 
			return 1;
		}
		aux = aux -> next;
		return 0;
	}
}


//CHECK LOGIN

int check(char *username,char *password){

	char user[MAXLINE];
	char pass[MAXLINE];
	int find = 0;

	fseek(f, 0, SEEK_SET);

	while(fscanf(f, "%s", user) != EOF && !find){

		fscanf(f,"%s",pass);
		if(strcmp(pass, password) == 0 && strcmp(user, username) == 0){
			find = 1;
		}

	}

	return find;
}

int checkUsername(char *username){
	char user[MAXLINE];
	int find = 0;

	fseek(f, 0, SEEK_SET);

	while(fscanf(f, "%s", user) != EOF && !find){
		if(strcmp(user, username) == 0){
			find = 1;
		}
	}

	return find;
}


void closer(struct user *aux){
	char temp[100];
	sprintf(temp, "quit");

	write(aux -> sockCmd, temp, strlen(temp));
	removeUser(&list, aux -> username);
	//pthread_exit((void *) code);
}

// void *x -> user structure
void *client_CMDhandler( void *x){	// function used to manage user cmd
	struct user *self = (struct user *) x;
	char buffer[MAXLINE], temp[MAXLINE], line[MAXLINE];
	int ret;
	char targetName[MAXLINE];
	struct user *target;

	struct sembuf op;
	op.sem_num = 0;
	op.sem_flg = 0;

	sprintf(temp, "Connected with name %s, MSG socket %d, CMD socket %d\ntype help for help\n", self->username, self->sockMsg, self->sockCmd);
	write(self->sockCmd, temp, strlen(temp));

	while(1){	// main loop
		memset(buffer, 0, MAXLINE); //reading the command
		memset(temp, 0, MAXLINE); //reading the command
		ret = read(self->sockCmd, buffer, MAXLINE); //READ ON SOCK CMD

		if(ret > 0){

			printf("Client %s, %d wrote %s\n", self->username, self->sockCmd, buffer); //debug on server NAME AND SOCKET

			if(strcmp(buffer, "list") == 0){	// user requested list of all connected users

				op.sem_op = -1;
red1:
				ret = semop(semList ,&op, 1);
				if(ret == -1){
					if(errno == EINTR) goto red1;
					if(errno != EINTR){
						closer(self);
						pthread_exit((void *) -1);
					}
					
				}

				printUsers(self->sockCmd); 	//CHANGE PARAM

				op.sem_op = 1;
red2:
				ret = semop(semList, &op, 1);
				if(ret == -1){
					if(errno == EINTR) goto red2;
					if(errno != EINTR){
						closer(self);
						pthread_exit((void *) -1);
					}
				} 	


			} else if(strcmp(buffer, "help") == 0){	// user requested list of all commands

				sprintf(temp, "========================\ntype:\nlist for list of users in the server\nhelp for help\nconnect [username] to connect to the user\nquit to disconnect from the server\navl for available users\n========================\n\n");

				write(self->sockCmd, temp, strlen(temp)); //CHANGE PARAM

			} else if(strncmp(buffer, "connect ", 8) == 0 && strlen(buffer) > 8){	// user requested connection to another user
				strtok(buffer, " ");
				strcpy(targetName, strtok(NULL, " ")); // finding the target of the user
				printf("traget name %s\n", targetName);
				if(strcmp(targetName, self->username) == 0){
					sprintf(temp, "Can't connect to self\n");
					write(self->sockCmd, temp, strlen(temp));
				} else {	
					op.sem_op = -1;
red7:
					ret=semop(semList, &op, 1);
					if(ret == -1){
						if(errno == EINTR) goto red7;
						if(errno != EINTR){
							closer(self);
							pthread_exit((void *) -1);
						}
					} 

					if(self->state == 0){  //CHECK IF I AM FREE
						if(freeUser(targetName)){      //CHECK IF MY TARGET IS FREE
							self->target = findTarget(targetName);
							self->state = 1;
							self->target->state = 1;
							self->target->target = self;

							printf("me name: %s, pass: %s, sockCmd: %d, sockMsg: %d\n", self->target->target->username, self->target->target->pass, self->target->target->sockCmd, self->target->target->sockMsg);
							printf("Check if target is connected to me\n");

							printf("target name: %s, pass: %s, sockCmd: %d, sockMsg: %d\n", self->target->username, self->target->pass, self->target->sockCmd, self->target->sockMsg);
							printf("Check if I'm connected to the target\n");		

							sprintf(temp, "Connection successful with %s\n", self->target->username);
							write(self->sockMsg, temp, strlen(temp));
							sprintf(temp, "Connection successful with %s\n", self->username);
							write(self->target->sockMsg, temp, strlen(temp));	
						} else {

							sprintf(temp, "Unable to establish connection\n");
							printf("%s", temp);
							write(self->sockCmd, temp, strlen(temp));
						}

					} else {

						sprintf(temp, "connection impossible\n");
						printf("%s", temp);
						write(self->sockCmd, temp, strlen(temp));
					}

					op.sem_op = 1;
red8:
					ret = semop(semList, &op, 1);
					if(ret == -1){
						if(errno == EINTR) goto red8;
						if(errno != EINTR){
							closer(self);
							pthread_exit((void *) -1);
						}
					} 	
				}		




			} else if(strcmp(buffer, "quit") == 0){	// user requested to disconnect from the server
				op.sem_op=-1;
red3:
				ret=semop(semList, &op ,1);
				if(ret == -1){
					if(errno == EINTR) goto red3;
					if(errno != EINTR){
						closer(self);
						pthread_exit((void *) -1);
					}
				} 


				if(freeUser(self->username)){

					removeUser(&list, self->username); // remove user from the list of users CHANGE PARAM				    	

				} else {
					self->target->state = 0;
					printf("target quit %s\n", self->target->username);
					sprintf(buffer, "change");
					write(self->target->sockCmd, buffer, strlen(buffer));
					printf("trying to eliminare %s\n", self->username);
					removeUser(&list, self->username); // remove user from the list of users CHANGE PARAM
					printf("eliminated\n");
				}
				op.sem_op = 1;
red4:
				ret = semop(semList, &op, 1);
				if(ret == -1){
					if(errno == EINTR) goto red4;
					if(errno != EINTR){
						closer(self);
						pthread_exit((void *) -1);
					}
				}
				pthread_exit((void *)0);         //closing managment thread  

			} else if(strcmp(buffer, "avl") == 0){	// user requested to list all free users
				op.sem_op = -1;
red5:
				ret = semop(semList, &op, 1);
				if(ret == -1){
					if(errno == EINTR) goto red5;
					if(errno != EINTR){
						closer(self);
						pthread_exit((void *) -1);
					}
				}

				printAvailableUsers(self->sockCmd, self->username); // print users with state equal to 1 //CHANGE PARAM
				op.sem_op = 1;
red6:
				ret = semop(semList, &op, 1);
				if(ret == -1){
					if(errno == EINTR) goto red6;
					if(errno != EINTR){
						closer(self);
						pthread_exit((void *) -1);
					}
				}
			} else if(strcmp(buffer, "close") == 0){
				op.sem_op = -1;
red9:
				ret = semop(semList, &op, 1);
				if(ret == -1){
					if(errno == EINTR) goto red9;
					if(errno != EINTR){
						closer(self);
						pthread_exit((void *) -1);
					}
				} 

				if(self->state == 1){

					sprintf(buffer, "change");
					write(self->target->sockCmd, buffer, strlen(buffer));			

					self->target->state = 0;
					self->state = 0;


				}

				op.sem_op = 1;
red10:
				ret = semop(semList, &op, 1);
				if(ret == 1){
					if(errno == EINTR) goto red10;
					if(errno != EINTR){
						closer(self);
						pthread_exit((void *) -1);
					}
				}				

			}
			else if(strcmp(buffer, "close")) {

				sprintf(temp, "Command not recognised\n");
				write(self->sockCmd, temp, strlen(temp));

			}
		}
	}
}	




void *client_MSGhandler(void *x){
	struct user *self=(struct user *)x;
	char line[MAXLINE];
	char temp[MAXLINE];

	while(1){

		memset(line, 0, MAXLINE);
		read(self->sockMsg, line, MAXLINE);

		if(strcmp(line, "quit") == 0){
			pthread_exit((void*)0);
		}
		else{
			printf("Client %s - %d writing on msg: %s\n", self->username, self->target->sockMsg, line);
		  	sprintf(temp, "%s: %s\n", self->username, line);	
			write(self->target->sockMsg, temp, strlen(temp));
		}
	}
}

void *logThread(void *x){
	struct user *aux = (struct user *)x;
	char text[MAXLINE];
	struct sembuf op, opFile;
	int ret, result;
	pthread_t tid;

	op.sem_num = 0;
	op.sem_flg = 0;
	opFile.sem_num = 0;
	opFile.sem_flg = 0;

	while(1){

		memset(text, 0, MAXLINE);
		read(aux->sockCmd, text, MAXLINE);

		printf("Received %s\n", text);

		if(strcmp(text, "log") == 0){

			// login operation	
			read(aux -> sockCmd, aux -> username, MAXLINE);
			printf("user %s\n", aux -> username);

			read(aux -> sockCmd, aux -> pass, MAXLINE);
			printf("pass %s\n", aux -> pass);

			opFile.sem_op = -1;
redFile1:
			ret = semop(semFile, &opFile, 1);
			if(ret == -1){
				if(errno == EINTR) goto redFile1;
				if(errno != EINTR){
					printf("semop error\n");
					close(aux -> sockCmd);
					close(aux -> sockMsg);
					free(aux);
					pthread_exit((void *) -1);
				}
			}
			result = check(aux -> username, aux -> pass);


			opFile.sem_op = 1;
redFile2:
			ret = semop(semFile, &opFile, 1);
			if(ret == -1){
				if(errno == EINTR) goto redFile2;
				if(errno != EINTR){
					printf("semop error\n");
					close(aux -> sockCmd);
					close(aux -> sockMsg);
					free(aux);
					pthread_exit((void *) -1);
				}
			}

			if(result){ 

				op.sem_op = -1;		   
red:
				ret = semop(semList, &op, 1);
				if(ret == -1){
					if(errno == EINTR) goto red;
					if(errno != EINTR){
					printf("semop error\n");
					close(aux -> sockCmd);
					close(aux -> sockMsg);
					free(aux);
					pthread_exit((void *) -1);
					}

				}
				if(!alreadyLogged(aux -> username)){

					sprintf(text, "Logged correctly\n");
					write(aux -> sockCmd, text, strlen(text));

					aux = addUser(&list, aux);

					if(pthread_create(&tid, NULL, client_CMDhandler, (void *)aux) != 0){
						printf("Unable to spawn CMD handler\n");
						pthread_exit((void *) -1);
					}
					if(pthread_create(&tid, NULL, client_MSGhandler, (void *)aux) != 0){
						printf("Unable to spawn MSG handler\n");
						pthread_exit((void *) -1);
					}
				} else {
					sprintf(text, "already logged\n");
					write(aux -> sockCmd, text, strlen(text));
					close(aux -> sockCmd);
					free(aux);
				}

				op.sem_op = 1;
red0:
				ret = semop(semList, &op, 1);
				if(ret == -1){
					if(errno == EINTR) goto red0;
					if(errno != EINTR){
					printf("semop error\n");
					close(aux -> sockCmd);
					close(aux -> sockMsg);
					free(aux);
					pthread_exit((void *) -1);
					}
				}    	
			} else {
				sprintf(text, "wrong username/password\n");
				write(aux -> sockCmd, text, MAXLINE);
				free(aux);
			}
			pthread_exit((void *) 0);
		} else if(strcmp(text, "reg") == 0) {

			read(aux -> sockCmd, aux -> username, 100);
			printf("reg username %s\n", aux -> username);

			read(aux -> sockCmd, aux -> pass, 100);
			printf("red password %s\n", aux -> pass);

			opFile.sem_op = -1;
			ret = semop(semFile, &opFile, 0);

			result = checkUsername(aux -> username);
			// Forse qualche comportamento strano
			/*
			* opFile.sem_op = 1;
			* semop(semFile, &opFile, 0);
			*/
			if(result){
				sprintf(text, "Username not available\n");
				write(aux -> sockCmd, text, strlen(text));
				
				opFile.sem_op = 1;
goup:				ret=semop(semFile, &opFile, 1);
				if(ret==-1){
					if(errno != EINTR){
					printf("semop error\n");
					close(aux -> sockCmd);
					close(aux -> sockMsg);
					free(aux);
					pthread_exit((void *) -1);
					}
					if(errno==EINTR)goto goup;
				}
			} else {
				/*
				* opFile.sem_op = -1;
				* semop(semFile, &opFile, 0);
				*/

				fseek(f, 0, SEEK_END);
				fprintf(f, "%s %s\n", aux -> username, aux -> pass);
				fflush(f);
				printf("Written line %s %s\n", aux -> username, aux -> pass);

				opFile.sem_op = 1;
goup0:				ret=semop(semFile, &opFile, 1);
				if(ret==-1){
					if(errno != EINTR){
					printf("semop error\n");
					close(aux -> sockCmd);
					close(aux -> sockMsg);
					free(aux);
					pthread_exit((void *) -1);
				}
					if(errno==EINTR) goto goup0;
				}

				sprintf(text, "Registered correctly\n");
				write(aux -> sockCmd, text, strlen(text));
			}

		} else if(strcmp(text, "quit") == 0) {

			close(aux->sockMsg);
			close(aux->sockCmd);
			free(aux);

			pthread_exit((void *)0);

		} else printf("Not recognised\n");
	}
}

// port number requested as parameter
int main(int argc, char *argv[]){
	int server_socket, port, client_sockCmd,client_sockMsg;
	int client_len;
	char buffer[MAXLINE];
	char text[MAXLINE];
	char checkCmd[100], checkMsg[100];
	struct sockaddr_in serv_addr, client_addr;
	int n;
	struct user *aux;
	pthread_t tid;

	char user[MAXLINE];
	char pass[MAXLINE];

	int ret;


	if (argc < 2) {
		printf("Wrong usage\nuse the first argument to set a valid port\nserver [port]\n");
		exit(1);
	}

	printf("server starting...\n");

	signal(SIGINT, interupt_handler);

	list = malloc(sizeof(struct user)); // allocating the list of users
	if(list == NULL){
		printf("malloc error\n");
		exit(-1);
	}

	semList = semget(IPC_PRIVATE, 1, IPC_CREAT|0666);
	if(semList == -1){
		printf("semList creation error\n");
		exit(-1);
	}
	semctl(semList, 0, SETVAL, 1);

	semFile = semget(IPC_PRIVATE, 1, IPC_CREAT|0666);
	if(semFile == -1){
		printf("semFile creation error\n");
		exit(-1);
	}
	semctl(semFile, 0, SETVAL, 1);

	server_socket = socket(AF_INET, SOCK_STREAM, 0); // creating socket
	if (server_socket < 0){
		printf("server error: failed to create socket\n");
		exit(-1);
	}	

	memset((char *) &serv_addr, 0, sizeof(serv_addr));
	port = atoi(argv[1]);
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(port);

	if (bind(server_socket, (struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0){ // binding the socket
		printf("server error: failed to bind the socket\n");
		exit(-1);
	}

	if(listen(server_socket,5) < 0){ // setting up the socket to a listening socket
		printf("server error: failed to set listening socket\n");
		exit(-1);
	}

	client_len = sizeof(client_addr);

	printf("server started\n");

	// Forse
	fd = open("credenziali.txt", O_RDWR | O_CREAT, 0666);
	if(fd == -1){
		printf("open error\n");
		exit(-1);
	}	
	f = fdopen(fd, "w+"); // BISOGNA USARE FSEEK
	//
	while(1){

		client_sockCmd = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);

		if(client_sockCmd < 0){
			printf("server error: unable to accept CMD connection\n");
		}
		else{
			printf("CMD socket accepted\n");
			////////////
			read(client_sockCmd, checkCmd, 100);
			printf("client_sockCmd %s\n", checkCmd);
			////////////
			sprintf(text,"CMD socket accepted\n");
			write(client_sockCmd,text,strlen(text));

		}

		client_sockMsg = accept(server_socket,(struct sockaddr *)&client_addr,&client_len);



		if(client_sockMsg < 0){
			printf("server error: unable to accept MSG connection\n");
		} else {

			read(client_sockMsg, checkMsg, 100);
			printf("client_sockMsg %s\n", checkMsg);

			if(strcmp("sockCmd", checkCmd) == 0 && strcmp("sockMsg", checkMsg) == 0){
				ret = 0;
				printf("MSG socket accepted\n");
				sprintf(text,"MSG socket accepted\n");
				write(client_sockCmd,text,strlen(text));
			} else {
				ret = 1;
				sprintf(text, "\nerror: sockets do not match\n");
				write(client_sockCmd, text, strlen(text));
				write(client_sockMsg, text, strlen(text));

				sprintf(text, "quit");
				write(client_sockCmd, text, strlen(text));
				write(client_sockMsg, text, strlen(text));

				close(client_sockCmd);
				close(client_sockMsg);
			}
		}


		if(ret != 1){
			aux = malloc(sizeof(struct user));
			memset(aux -> pass, 0, 100);
			memset(aux -> username, 0, 100);
			aux -> sockCmd = client_sockCmd;
			aux -> sockMsg = client_sockMsg;
			pthread_create(&tid, NULL, logThread, (void *)aux);
		}

	}

	close(server_socket);

	return 0; 
}
