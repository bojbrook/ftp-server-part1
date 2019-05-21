#include <stdio.h>
#include <stdlib.h>     // Needed for OS X
#include<sys/socket.h>
#include <netinet/in.h> // Needed to use struct sockaddr_in
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#include <strings.h>
#include <sys/select.h>
#include <pthread.h>

#define FILE_COMMAND "\\filename"
#define SIZE_COMMAND "\\size"
#define ACK_COMMAND "\\ack"
#define OFFSET_COMMAND "\\offset"
#define ERROR_COMMAND "\\error"


#define SENDING_AMOUNT 1024
#define RECEVING_AMOUNT 60


typedef struct {
	int socketfd;
	int serverNum;
	struct sockaddr_in servaddr;
	struct sockaddr_in cliaddr;
	char * inputbuffer;
	char * filename;
	socklen_t len;
}server_thread;

int file_size(char * filename){
	FILE *fp = fopen(filename, "r");
	if(fp == NULL)
		return -1;
	int count = 0;
	while(getc(fp) != EOF)
		count++;
	fclose(fp);
	return count;
}
void send_ack(int sockfd,struct sockaddr_in *cliaddr, socklen_t len){
	sendto(sockfd, ACK_COMMAND, strlen(ACK_COMMAND),0, (struct sockaddr *) cliaddr, len);
}
void send_message(char * buffer,int sockfd,struct sockaddr_in *cliaddr, socklen_t len){
	sendto(sockfd, buffer, strlen(buffer),0, (struct sockaddr *) cliaddr, len);
}

//server client thread function
void *thread_child_server(void *servers){
		server_thread *server = (server_thread*)servers;

		/*setting up select values*/
		struct timeval tv;
		fd_set cl_set;
		tv.tv_sec = 1;
		tv.tv_usec = 0;
		FD_ZERO(&cl_set);
		FD_SET(server->socketfd,&cl_set);
		
		int offset  = atoi(strtok(NULL, " "));
		int sending_amount = atoi(strtok(NULL, ""));
		printf("Server #%d %d %d\n", server->serverNum,offset,sending_amount+offset);
		
		//starting to send the file over
		int total_sent = 0;
		char ch;
		int current =0;
		
		char file_buffer[sending_amount];
		char buff[RECEVING_AMOUNT];
		FILE *fp = fopen(server->filename,"r");
		if(fp == NULL){
			printf("%s\n", "WE have a big problem");
			pthread_exit(0);
		}
		fseek(fp,offset,SEEK_SET);	//jumping to offset
		while(current <= sending_amount && (ch = fgetc(fp)) != EOF){
			file_buffer[current] = ch;
			current++;
			total_sent++;
			if(current >= sending_amount){
				//printf("%s\n", "Sending message");
				send_message(file_buffer,server->socketfd,&server->cliaddr,server->len);
				if(select(server->socketfd,&cl_set,NULL,NULL,&tv) > 0){
					recvfrom(server->socketfd, buff, RECEVING_AMOUNT, 0,  (struct sockaddr *)&server->cliaddr, &server->len);
				}else{
					send_message(file_buffer,server->socketfd,&server->cliaddr,server->len);
				}
				current = 0;
			}
		}
		fclose(fp);
		send_message(file_buffer,server->socketfd,&server->cliaddr,server->len);
	return NULL;
}

int main(int argc, char const *argv[])
{
	const int port = atoi(argv[1]);
	int sockfd;
	int fsize, index;
	int total_servers = 0;
	char buff[RECEVING_AMOUNT] = {0};
	char send_buff[SENDING_AMOUNT] = {0};
	char * filename;


	/*Binding Socket ADDRESS to port*/
	struct sockaddr_in servaddr,cliaddr;
	sockfd=socket(AF_INET, SOCK_DGRAM, 0);
	bzero(&servaddr,sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(port);
	bind(sockfd, (struct sockaddr *) &servaddr,sizeof(servaddr));

	socklen_t len = sizeof(cliaddr);
	

	while(1){
		//reveiving file name & sending back size
		index = recvfrom(sockfd, buff, RECEVING_AMOUNT, 0,  (struct sockaddr *)&cliaddr, &len);
		buff[index] = 0;
		char * command = strtok(buff, " ");
		if(strcmp(command,FILE_COMMAND) == 0){
			//getting the name
			char *temp_name = strtok(NULL, " ");
			filename = malloc(sizeof(temp_name));
			memcpy(filename,temp_name,strlen(temp_name)+1);
			//printf("%s\n", filename);
			fsize = file_size(filename);
			printf("%s %d\n", filename, fsize);
			if (fsize == -1){
				printf("%s\n", "WE have a big problem");
				send_message(ERROR_COMMAND,sockfd,&cliaddr,len);
				continue;
			}
			sprintf(send_buff,"%s %d",SIZE_COMMAND,fsize);
			send_message(send_buff,sockfd,&cliaddr,len);
		}
		
		//offset + amount to send
		if(strcmp(command,OFFSET_COMMAND) == 0){
			pthread_t t;
			server_thread *t_server;
			t_server = malloc(sizeof(server_thread));
			t_server->socketfd = sockfd;
			t_server->cliaddr = cliaddr;
			t_server->len = len;
			t_server->inputbuffer = buff;
			t_server->filename = filename;
			t_server->serverNum = total_servers;
			total_servers++;
			//running the threads
		  if(pthread_create(&t, NULL, thread_child_server, t_server)!= 0) {
					fprintf(stderr, "Error creating thread\n");
					return 1;
			}
		}
	}

	
		
	return 0;
}
