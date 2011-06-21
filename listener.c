#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <jemalloc/jemalloc.h>
#include "listener.h"
#include "ds.h"

extern Buffer* b;

void close_sockets()
{
    int i;
    for(i=0; i<NUM_THREADS; i++)
        close(thread_sockfd[i]);
    close(sockfd);
}

void handler(int signal)
{
    if(signal == SIGINT) {
        printf("\nCaught CTRL-C (SIGINT), Exiting...\n");
        close_sockets();
        if(!buffer_isempty(b)) {
            printf("Warning: exiting with non-empty buffer\n");
            buffer_status(b);
        }
        buffer_clear(b);
        exit(0);
    }
    else
        printf("\nCaught signal %i\n", signal);
}

void die(const char *msg)
{
    perror(msg);
    exit(1);
}

void* listener_child(void* psock)
{
    int sock = *((int*) psock);
    int BUFFER_LEN = sizeof(Event); // maximum size
    PacketHeader* packet_buffer = malloc(BUFFER_LEN);

    while(1) {
        memset(packet_buffer, 0, BUFFER_LEN);
        int r = recv(sock, packet_buffer, BUFFER_LEN, 0);
        if(r<0) {
            die("ERROR reading from socket");
            break;
        }
        else if(r==0) {
            printf("Client terminated connection...\n");
            break;
        }
        else {
            //continue;
            //printf("Received PMTBundle on socket %i\n", sock);
            // determine packet type
            char packet_type = packet_buffer->;
            if(packet_type == EVENT) { //switch
                Event* e = (Event*) packet_buffer;                
            }
/*
            void* p;
            RecordType r;
            buffer_at(b, p.gtid, &r, &p);
            pthread_mutex_t m = b->mutex;
            if(r == EVENT) { // switch
                Event* e = (Event*) p;
                pthread_mutex_lock(&(b->mutex));
                if(e == NULL) {
                    e = malloc(sizeof(Event));
                    e->pmt[p.pmtid] = p;
                    buffer_insert(b, p.gtid, r, e);
                }
                else {
                    e->pmt[p.pmtid] = p;
                }
            }
*/
        }
    }
}

void* listener(void* ptr)
{
     int portno;
     socklen_t clilen;
     struct sockaddr_in serv_addr, cli_addr;

     portno = *((int*)ptr);

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
                            listener_child,
                            (void*)&(thread_sockfd[thread_index]));
             thread_index++;
         }
     }

     close_sockets();
}

