#include <stdio.h>
#include <stdlib.h>     
#include<sys/socket.h>
#include <netinet/in.h> // Needed to use struct sockaddr_in
#include <strings.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/select.h>
#include <sys/time.h>
#include <pthread.h>

#define RECEIVE_SIZE 1024
#define SENDING_SIZE 60
#define MAX_SERVERS 10
#define ERROR_COMMAND "\\error"
#define FILE_COMMAND "\\filename"
#define SIZE_COMMAND "\\size"
#define OFFSET_COMMAND "\\offset"
#define ACK_COMMAND "\\ack"
#define EXIT_COMMAND "\\exit"

#define TIMEOUT 500

typedef struct {
	char * IP;
	int Port;
	struct sockaddr_in servaddr;
	socklen_t len;
}server_info;

typedef struct {
	int socketfd;
	int thread_Num;
	const char * FILENAME;
	int offset;	//how far for the server to jump forward
	int chunk;	//the amount of file to receive
	const char* filename;
	pthread_mutex_t m;
	server_info server;
	
}client_thread;


int create_socket(server_info *info){
	inet_pton(AF_INET, info->IP , &info->servaddr.sin_addr);
	int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	return sockfd;
}


int get_file_size(char * buffer){
	char * command = strtok(buffer, " ");
	if(strcmp(SIZE_COMMAND,command) == 0)
		return atoi(strtok(NULL, ""));
	else
		return -1;
}

/*writes the client threads to file*/
void client_write_to_file(FILE *fp,char *buffer,int offset, int amount_to_write){
	int count = 0;
	fseek(fp,offset,SEEK_SET);
	while(count < amount_to_write){
		fputc(*buffer,fp);
		buffer++;
		count++;
	}
}
//sends a message to the given server
void send_message(char * buffer,int sockfd,struct sockaddr_in *servaddr, socklen_t len){
	sendto(sockfd, buffer, strlen(buffer),0, (struct sockaddr *) servaddr, len);
}

//client thread message
void *thread_child_client(void *clients){
	client_thread *client = (client_thread*)clients;
	char send_buff[SENDING_SIZE];
	server_info si = client->server;
	//first number is the offset next is the amount to send
	sprintf(send_buff, "%s %d %d",OFFSET_COMMAND,client->offset,client->chunk);
	send_message(send_buff,client->socketfd,&si.servaddr,si.len);
	char file_buffer[client->chunk];
	int index;
	while((index = recvfrom(client->socketfd, file_buffer, client->chunk, 0,  
													(struct sockaddr *)&si.servaddr, &si.len)) > 0){
			file_buffer[index] = 0;		
			sprintf(send_buff, "%s",ACK_COMMAND);
			send_message(send_buff,client->socketfd,&si.servaddr,si.len);
			break;
	}
	sprintf(send_buff, "%s",ACK_COMMAND);
	send_message(send_buff,client->socketfd,&si.servaddr,si.len);

	pthread_mutex_lock(&client->m);	
	FILE *client_copy_fp = fopen(client->FILENAME,"r+");
	if(client_copy_fp == NULL){
		client_copy_fp = fopen(client->FILENAME,"w");
		if(client_copy_fp == NULL){
			printf("%s\n", "we have a big problem");
		}
	}
	client_write_to_file(client_copy_fp,file_buffer,client->offset, client->chunk);
	printf("Child %d wrote to file Start:%d End:%d\n", client->thread_Num,client->offset, client->offset+client->chunk);
	fclose(client_copy_fp);	
	pthread_mutex_unlock(&client->m);	
	//printf("Child %d wrote to file Start:%d End:%d\n", client->thread_Num,client->offset, client->offset+client->chunk);
	return NULL;
}




int main(int argc, char const *argv[]){
	const char * name;
	int num_chunks;
	const char * FILENAME;
	if(argc > 3){
		name = argv[1];
		num_chunks = atoi(argv[2]);
		FILENAME = argv[3];
	}else{
		printf("%s\n", "not enough args");
		return -1;
	}
	int socketfd[MAX_SERVERS];
	server_info SI[MAX_SERVERS];
	int sockfd;
	int receivelen;
	int index;
	int fsize;
	int total_servers = 0;
	struct timeval tv;
	fd_set cl_set;
	struct sockaddr_in servaddr;



	//getting the info from server-info.txt
	FILE *fp_server = fopen(name,"r");
	if (fp_server == NULL)
		printf("%s\n", "Houston we have a problem");
	char server_info_line[100] = {0};
	int server_count =0;
	//puts all the server info into a struct
	while (fgets(server_info_line, 100, fp_server)){
       SI[server_count].IP = strtok(server_info_line," ");
       SI[server_count].Port = atoi(strtok(NULL,""));
       SI[server_count].len = sizeof(SI[server_count].servaddr);
			 bzero(&SI[server_count].servaddr, sizeof(SI[server_count].servaddr));
	     SI[server_count].servaddr.sin_family = AF_INET;
	     SI[server_count].servaddr.sin_port = htons(SI[server_count].Port);
       server_count++;
  }
  fclose(fp_server);

	char send_buff[SENDING_SIZE];
	char receive_buff[RECEIVE_SIZE];
  /************Start of setting up the threads*******************/
  int count = 0;
	int errors = 0;

	//setting the timout values values
	tv.tv_sec = 0;
	tv.tv_usec = 5000;


	for(count = 0; count < server_count; count ++){
		int socket = create_socket(&SI[count]);	
		FD_ZERO(&cl_set);
		FD_SET(socket,&cl_set);
		if(socket > 0)
			socketfd[count] = socket;
		else if(socket < 0){
			printf("%s\n", "This is the biggest problem yet");
		}
		else {
			errors++;
			continue;
		}
		//sending name over the name
		sprintf(send_buff, "%s %s",FILE_COMMAND,FILENAME);
		sendto(socket, send_buff, strlen(send_buff),0, (struct sockaddr *) &SI[count].servaddr, SI[count].len);
		//select(sockfd,&cl_set,NULL,NULL,&tv);
		if(select(socket+1,&cl_set,NULL,NULL,&tv) > 0){
			if(FD_ISSET(socket, &cl_set)){
				index = recvfrom(socket, receive_buff, RECEIVE_SIZE, 0,  (struct sockaddr *)&SI[count].servaddr, &SI[count].len);
				receive_buff[index] = 0;
				fsize = get_file_size(receive_buff);
				if(fsize == -1){
					printf("%s\n", "No such file");
					return -1;
				}
			}else{
				printf("%s\n", "TIMEOUT");
			}
		}else{
			printf("TIMOUT NO SERVER @: %s %d\n",SI[count].IP,SI[count].Port);
			errors++;
		}
		
	}
	total_servers = count-errors;
	printf("Total servers: %d\n", total_servers);
	
	/*SETTING UP THE CLIENT THREADs*/
	count = 0;
	pthread_t t;
	pthread_mutex_t m	=	PTHREAD_MUTEX_INITIALIZER;
	//while(count < total_servers && count < num_chunks){
	while(count < num_chunks){
		client_thread *t_client;
		t_client = malloc(sizeof(client_thread));
		t_client->socketfd = socketfd[count%total_servers];
		t_client->server = SI[count%total_servers];
		t_client->FILENAME = FILENAME;
		t_client->chunk = (fsize/num_chunks);
		t_client->offset = t_client->chunk * count;
		t_client->thread_Num = count;
		t_client->m = m;
		t_client->filename = FILENAME;
		
		//this is the file size can't evenly devided.
		if(count == num_chunks - 1){
			t_client->chunk+= fsize % num_chunks;
		}
		//running the threads
	  if(pthread_create(&t, NULL, thread_child_client, t_client)!= 0) {
				fprintf(stderr, "Error creating thread\n");
				return 1;
		}
		if(pthread_join(t, NULL)!= 0) {
			fprintf(stderr, "Error joining thread\n");
			return 2;
	}
		count ++;
	}

	printf("%s\n", "all the child threads are dead");
	
	return 0;
}

