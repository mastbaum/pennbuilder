#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <pthread.h>

#define NUM_THREADS 20

int sockfd, thread_sockfd[NUM_THREADS];

inline void close_sockets()
{
    int i;
    for(i=0; i<NUM_THREADS; i++)
        close(thread_sockfd[i]);
    close(sockfd);
}

inline void handler(int signal)
{
    if(signal == SIGINT) {
        printf("\nCaught CTRL-C (SIGINT), Exiting...\n");
        close_sockets();
        exit(0);
    }
    else
        printf("\nCaught signal %i\n", signal);
}

inline void die(const char *msg)
{
    perror(msg);
    exit(1);
}

inline void* dostuff(void* psock)
{
    int sock = *((int*) psock);
    int BUFFER_LEN = 256;
    char buffer[BUFFER_LEN];

    while(1) {
        memset(buffer, 0, BUFFER_LEN);
        int r = recv(sock, &buffer, BUFFER_LEN, 0);
        if(r<0) {
            die("ERROR reading from socket");
            break;
        }
        else if(r==0) {
            printf("Client terminated connection...\n");
            break;
        }
        else
            printf("Here is the message: %s\n",buffer);
    }
}

int main(int argc, char *argv[])
{
     int portno;
     socklen_t clilen;
     struct sockaddr_in serv_addr, cli_addr;

     if (argc < 2) {
         fprintf(stderr,"ERROR, no port provided\n");
         exit(1);
     }
     portno = atoi(argv[1]);

     sockfd = socket(AF_INET, SOCK_STREAM, 0);
     if(sockfd < 0) die("ERROR opening socket");

     memset(&serv_addr, 0, sizeof(serv_addr));
     serv_addr.sin_family = AF_INET;
     serv_addr.sin_addr.s_addr = INADDR_ANY;
     serv_addr.sin_port = htons(portno);

     if(bind(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0)
         die("ERROR on binding");

     listen(sockfd, 5);

     clilen = sizeof(cli_addr);
     signal(SIGINT, &handler);
     pthread_t threads[NUM_THREADS];
     int thread_index = 0; //FIXME: want NUM_THREADS currently, not cumulative
     while(1) {
         int newsockfd = accept(sockfd, (struct sockaddr*) &cli_addr, &clilen);
         if(newsockfd < 0) die("ERROR on accept");
         else {
             thread_sockfd[thread_index] = newsockfd;
             printf("spawning thread with index %i\n", thread_index);
             pthread_create(&(threads[thread_index]),
                            NULL,
                            dostuff,
                            (void*)&(thread_sockfd[thread_index]));
             thread_index++;
         }
     }

     close_sockets();
     return 0;
}

