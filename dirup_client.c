#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <dirent.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <libgen.h>
#include <unistd.h>
#define PATH_MAX_LENGTH 256
#define MAX_COUNT_THREAD 5


pthread_mutex_t mutex;
pthread_mutex_t handler_mutex;
int t_count;
int v_idx;
char visited_list[1024][256];

char IP[16];
int port;
char path[PATH_MAX_LENGTH];
int file_cnt;

typedef struct{
	char path[256];
	int filesize;
}Header;

void handler_func(char * h_path){
	
	struct dirent *dir;
	#ifdef DEBUG
		printf("In handler_func, path: %s\n",h_path);
	#endif
	DIR *dp = opendir(h_path);
	struct stat sb;	
	char current_path[PATH_MAX_LENGTH];

	char pathname[PATH_MAX_LENGTH];
	strcpy(pathname, h_path);
	while( (dir = readdir(dp)) ){
		if(strncmp(dir->d_name, ".", 1) == 0) continue;
		if(pathname[strlen(pathname)] == '/'){
			sprintf(current_path,"%s%s",pathname,dir->d_name);
		}else{
			sprintf(current_path, "%s/%s",pathname,dir->d_name);
		}
		#ifdef DEBUG
			printf("current_path: %s\n",current_path);
		#endif

		if(stat(current_path,&sb) == -1){
			fprintf(stderr, "ERROR: stat()\n");
			return; 
		}
		if( (sb.st_mode & S_IFMT) == S_IFREG){ //It means that "If this path is regular file"
			int i = 0;
			int exist_flag = 0;
			for(i=0; i < v_idx; i++){
				if(strcmp(current_path, visited_list[i]) == 0){
					exist_flag = 1;
					break;
				}
			}
			if(exist_flag == 0){
				printf("[%s] Upload canceled\n", current_path);
				file_cnt++;
			}
		}else if((sb.st_mode & S_IFMT) == S_IFDIR){
			handler_func(current_path);
		}
	}
}


int connection(){
        struct sockaddr_in serv_adr;

        int sd = socket(PF_INET, SOCK_STREAM, 0);

        memset(&serv_adr, 0, sizeof(serv_adr));
        serv_adr.sin_family = AF_INET;
        serv_adr.sin_addr.s_addr = inet_addr(IP);
        serv_adr.sin_port = htons(port);

        if(connect(sd, (struct sockaddr*)&serv_adr, sizeof(serv_adr)) == 0 ){
		return sd;
	}
	return -1;
}

void sigint_handler(){
	pthread_mutex_lock(&handler_mutex);
	printf("Control+C Pressed!\n");
	handler_func(path);	
	printf("%d/%d files has been upload!\n",v_idx, file_cnt);	
	int sig_socket = connection();
	write(sig_socket, "Cancel",sizeof("Cancel"));
	shutdown(sig_socket, SHUT_WR);
	char buf[8];
	read(sig_socket,buf, sizeof(buf));
	if(strcmp(buf, "OK") != 0){
		fprintf(stderr, "ERROR: don't receive OK\n");
	}	
	close(sig_socket);
	sleep(5);
	exit(1);
	pthread_mutex_unlock(&handler_mutex);
}
int send_check(char * buf, int size, int serv_sd){
	size_t total_sent = size;
	size_t bytes_sent = 0;
	char *buf_p = buf;
	while(total_sent > 0 && 0 < (bytes_sent = send(serv_sd, buf_p, total_sent, 0))){
		total_sent -= bytes_sent;
		buf_p += bytes_sent;
		
	}
	if(bytes_sent == -1){
		return 1;
	}

	return 0;
}

pthread_mutex_t con;
void * sendFile(void * file_header){
	signal(SIGINT, &sigint_handler);

	Header *header = malloc(sizeof(Header));
	memcpy(header, file_header, sizeof(Header));
	free(file_header);
	#ifdef DEBUG
		printf("From findFile to sendFile, path: %s, size:%d \n", header->path, header->filesize);
	#endif	
	pthread_mutex_lock(&con);
	int serv_sd = connection();
	pthread_mutex_unlock(&con);	

	if(serv_sd == -1){
		fprintf(stderr, "ERROR: connect(): %s\n",(char *)path);
		return NULL;
	}


	FILE *fd = fopen(header->path, "rb");
	if(fd == NULL){
		fprintf(stderr, "ERROR: open()\n");
		free(header);
		return NULL;
	}
	//Send path	
	char *send_path = malloc(sizeof(header->path));
	strcpy(send_path, header->path);
	if(send_check(send_path, sizeof(header->path), serv_sd)){
		fprintf(stderr, "ERROR: send path\n");
		fclose(fd);
		free(header);
		return NULL;
	}
	free(send_path);
	
	//Send size
	int send_filesize = header->filesize;
	if(send_check((char*)&send_filesize, sizeof(send_filesize),serv_sd)){
		fprintf(stderr,"ERROR: send the size\n");
		fclose(fd);
		free(header);
		return NULL;
	}
	
	//Send file data	
	char buf[1024];
	int read_size = 0;
	while( (read_size = fread(buf, 1, sizeof(buf), fd))){
		if(send_check(buf, read_size, serv_sd)){
			fprintf(stderr,"ERROR: send the file : %s\n",(char *)path);
			fclose(fd);
			free(header);
			return NULL; 
		}
	}
	shutdown(serv_sd, SHUT_WR);	

	//Send success message
	recv(serv_sd, buf,sizeof(buf), 0);
	if(strcmp(buf,"Thankyou") == 0){
		printf("[%s] Upload completed!.. %dbytes\n",header->path, header->filesize);
	}else{
		printf("Can't send success message\n");
		fclose(fd);
		free(header);
		return NULL;
	}

	pthread_mutex_lock(&mutex);
	file_cnt++;
	pthread_mutex_unlock(&mutex);
	
	fclose(fd);
	free(header);	
	t_count--;
	return NULL;
}

int findFile(char * pathname){
	#ifdef DEBUG
		printf("From main() to findFile(), pathname: %s\n", pathname);
	#endif
	struct stat dirCheck;
	if(stat(pathname, &dirCheck)== -1){
		fprintf(stderr,"ERROR: path is not directory\n");
		return 1;
	}
	
	struct dirent *dir;
	DIR *dp = opendir(pathname);
	struct stat sb;	
	char current_path[PATH_MAX_LENGTH];

	pthread_t tid;
	pthread_t tid_list[1024];
	int t_idx=0;
	while( (dir = readdir(dp)) ){
		if(strncmp(dir->d_name, ".", 1) == 0) continue;
		Header *set= malloc(sizeof(Header));
		if(pathname[strlen(pathname)] == '/'){
			sprintf(current_path,"%s%s",pathname,dir->d_name);
		}else{
			sprintf(current_path, "%s/%s",pathname,dir->d_name);
		}
		#ifdef DEBUG
			printf("current_path: %s\n",current_path);
		#endif

		if(stat(current_path,&sb) == -1){
			fprintf(stderr, "ERROR: stat()\n");
			return 1; 
		}
		if( (sb.st_mode & S_IFMT) == S_IFREG){ //It means that "If this path is regular file"
			strcpy(set->path, current_path);
			set->filesize = sb.st_size;
			while(t_count > MAX_COUNT_THREAD){}
			#ifdef DEBUG
				printf("This is File. path: %s, size: %d\n", set->path, set->filesize);
			#endif

			if( (0 != pthread_create(&tid, NULL, (void *)sendFile, (void*)set))){
				fprintf(stderr,"ERROR : pthread_create()\n");
				return 1;
			}
			strcpy(visited_list[v_idx], current_path);
			tid_list[t_idx] = tid;
			v_idx++;
			t_idx++;
			t_count++;
			sleep(1);
		}else if((sb.st_mode & S_IFMT) == S_IFDIR){
			findFile(current_path);
		}
	}
	int i = 0;
	for(i=0; i<t_idx; i++){
		pthread_join(tid_list[i],NULL);
	}
	
	return 0;
}




int main(int argc, char *argv[]){
	/*struct sigaction sa;
    	sa.sa_handler = *(sigint_handler);
    	sigemptyset(&sa.sa_mask);
    	sa.sa_flags = 0;
    	if (sigaction(SIGINT, &sa, NULL) == -1) {
       		perror("sigaction");
        	exit(1);
    	}

    	sigset_t mask, oldmask;
    	sigemptyset(&mask);
    	sigaddset(&mask, SIGINT);
    	if (pthread_sigmask(SIG_BLOCK, &mask, &oldmask) != 0) {
        	perror("pthread_sigmask");
        	exit(1);
    	}
	pthread_t thread;
        pthread_create(&thread, NULL, sigint_handler, &mask);
	*/
	pthread_mutex_init(&handler_mutex, NULL);
	pthread_mutex_init(&mutex, NULL);
	if (argc != 4) {
                printf("Usage: %s <IP> <port> <path> \n", argv[0]);
                exit(EXIT_FAILURE);
        }	
	strcpy( IP, argv[1]);
	port = atoi(argv[2]);
	strcpy( path, argv[3]);
	printf("IP: %s\n", IP);
	printf("Port: %d\n",port);
	printf("Path: %s\n",path);

	char pathname[PATH_MAX_LENGTH];
	strcpy(pathname, path);
	if(findFile(pathname) == 0){
		printf("%d/%d files has been upload!\n",v_idx, file_cnt);
	}else{

	}


	return 0;
}
