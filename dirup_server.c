#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <libgen.h>

#define BUF_SIZE 1024

struct socket_info{
	int sd;
	struct sockaddr_in sa;
};

struct Header{
	char path[256];
	int filesize;
};
void error_handling(char *message);

char ip[32];
void _makedir(char * path){
	#ifdef DEBUG
		printf("path: %s\n",path);
	#endif
	struct stat dircheck;
	if(strcmp(path, ".")==0 || stat(path, &dircheck) == 0){
		 return;
	}
	char * cp = strdup(path);
	char * parentpath = dirname(path);
	
	_makedir(parentpath);

	#ifdef DEBUG
		printf("Make Dir: %s\n",cp);
	#endif

	mkdir(cp,0755);

}

void makedir(char * path){
	char * cp = strdup(path);
	char *dir_path = dirname(cp);
	#ifdef DEBUG
		printf("Sending dir_path: %s\n", dir_path);
	#endif
	_makedir(dir_path);
	
	free(cp);
}
void * makeFile(void * sck){
	struct socket_info *sock_info = malloc(sizeof(struct socket_info));
	*sock_info = *((struct socket_info *)sck);
	int clnt_sd = sock_info->sd;
	free(sck);

	#ifdef DEBUG
		printf("From Main() to makeFile(), socket: %d, sock_info: %s\n", clnt_sd, inet_ntoa(sock_info->sa.sin_addr));
	#endif

	struct Header *header = malloc(sizeof(struct Header));
	
	//Receive path
	char buf_path[256];
	int total_sent = sizeof(header->path);
	int recv_bytes = 0;
	char *p = buf_path;
	
	while(total_sent > 0 && 0 < ( recv_bytes = recv(clnt_sd, p, total_sent, 0))){
		total_sent -= recv_bytes;
		p += recv_bytes;
	}
	strcpy(header->path, buf_path);
	#ifdef DEBUG
		printf("It is for checking Cancel msg. %s %d\n", header->path,strlen(header->path));
	#endif
	if(strcmp(header->path, "Cancel") == 0){
		printf("File upload cancelation message is received from %s",ip);
		shutdown(clnt_sd, SHUT_RD);
		write(clnt_sd, "OK", sizeof("OK"));
		close(clnt_sd);
		free(header);
		return NULL;
	}
	#ifdef DEBUG
		printf("header->path: %s\n", header->path);
	#endif
	//Receive filesize
	int buf_size = 0;
	total_sent = sizeof(header->filesize);
	recv_bytes = 0;
	p = (char *)&buf_size;
	while(total_sent > 0 && 0 < (recv_bytes = recv(clnt_sd, p, total_sent, 0))){
		total_sent -= recv_bytes;
		p += recv_bytes;
	}
	if(recv_bytes == -1){
		fprintf(stderr, "ERROR: receive filesize\n");
		return NULL;
	}
	header->filesize = buf_size;	
	#ifdef DEBUG
		printf("header->filesize: %d\n", header->filesize);
	#endif

	//Receivce file data
	makedir(header->path);
	FILE *fd = fopen(header->path, "w+");
	if(fd == NULL){
		fprintf(stderr, "ERROR: file open()\n");
		return NULL;
	}

	char buf[1024];
	int recv_size = 0;
	while( (recv_size = recv(clnt_sd, buf, sizeof(buf), 0))){
		fwrite(buf, 1, recv_size, fd);
	}	
	shutdown(clnt_sd, SHUT_RD);
	

	//Success msg 보내기
	send(clnt_sd, "Thankyou", sizeof("Thankyou"),0);
	printf("[%s]Received from %s ... %dbytes\n",header->path, inet_ntoa(sock_info->sa.sin_addr), header->filesize);

	close(clnt_sd);
	fclose(fd);
	free(header);
	return NULL;
}


int main(int argc, char *argv[])
{
	int serv_sock, clnt_sock;
	
	struct sockaddr_in serv_adr;
	struct sockaddr_in clnt_adr;
	socklen_t clnt_adr_sz;
	
	if (argc != 2) {
		printf("Usage : %s <port>\n", argv[0]);
		exit(1);
	}
	
	serv_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (serv_sock == -1)
		error_handling("socket() error");

	memset(&serv_adr, 0, sizeof(serv_adr));
	serv_adr.sin_family = AF_INET;
	serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_adr.sin_port = htons(atoi(argv[1]));

	if (bind(serv_sock, (struct sockaddr *)&serv_adr, sizeof(serv_adr)) == -1)
		error_handling("bind() error");
		
	printf("Server Listen...\n");

	if (listen(serv_sock, 16))
		error_handling("listen() error");
	
	clnt_adr_sz = sizeof(clnt_adr);
	
	pthread_t tid;
	while(1){
		int *clnt_sock = malloc(sizeof(int));
		*clnt_sock = accept(serv_sock, (struct sockaddr *)&clnt_adr, &clnt_adr_sz);
		if (*clnt_sock == -1){
			error_handling("accept() error");
			free(clnt_sock);
			continue;
		}else{
			#ifdef DEBUG
				printf("connection\n");
			#endif
		}
		struct socket_info *si = malloc(sizeof(struct socket_info));
		si->sd = *clnt_sock;
		si->sa = clnt_adr;	
		strcpy(ip, inet_ntoa(si->sa.sin_addr));
		pthread_create(&tid, NULL, makeFile, si);
		pthread_detach(tid);
	}
	close(clnt_sock);
	close(serv_sock);
	return 0;
}

void error_handling(char *message)
{
	fputs(message, stderr);
	fputc('\n', stderr);
}
