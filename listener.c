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

extern Buffer* event_buffer;
extern Buffer* event_header_buffer;
extern Buffer* run_header_buffer;

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
        if(!buffer_isempty(event_buffer)) {
            printf("Warning: exiting with non-empty event buffer\n");
            buffer_status(event_buffer);
        }
        if(!buffer_isempty(event_header_buffer)) {
            printf("Warning: exiting with non-empty event header buffer\n");
            buffer_status(event_header_buffer);
        }
        if(!buffer_isempty(run_header_buffer)) {
            printf("Warning: exiting with non-empty run header buffer\n");
            buffer_status(run_header_buffer);
        }
        buffer_clear(event_buffer);
        buffer_clear(event_header_buffer);
        buffer_clear(run_header_buffer);
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
    int MAX_BUFFER_LEN = sizeof(Event); // maximum size
    void* packet_buffer = malloc(MAX_BUFFER_LEN);

    while(1) {
        memset(packet_buffer, 0, MAX_BUFFER_LEN);
        int r = recv(sock, packet_buffer, MAX_BUFFER_LEN, 0);
        if(r<0) {
            die("ERROR reading from socket");
            break;
        }
        else if(r==0) {
            printf("Client terminated connection...\n");
            break;
        }
        else {
            PacketType packet_type = packet_id(packet_buffer);
            printf("got packet of type %i\n", packet_type);
            if(packet_type == PMTBUNDLE) {
                XL3Packet* p = realloc(packet_buffer, sizeof(XL3Packet));
                // fixme: check packet type to ensure megabundle
                int nbundles = p->cmdHeader.num_bundles;
                printf("xl3 packet with %i bundles\n", nbundles);
                int ibundle;
                for(ibundle=0; ibundle<nbundles; ibundle++) {
                    PMTBundle* pmtb = (PMTBundle*) &(p->payload[ibundle]); // errrrrrm?
                    uint32_t gtid = pmtbundle_gtid(pmtb);
                    uint32_t pmtid = pmtbundle_pmtid(pmtb);
                    Event* e;
                    RecordType r;
                    buffer_at(event_buffer, gtid, &r, (void*)&e);
                    if(e == NULL) {
                        pthread_mutex_lock(&(event_buffer->mutex));
                        e = malloc(sizeof(Event));
                        buffer_insert(event_buffer, gtid, DETECTOR_EVENT, (int*)e);
                        pthread_mutex_unlock(&(event_buffer->mutex));
                    }
                    e->pmt[pmtid] = *pmtb;                    
                }
            //case MTCINFO: break;
            //case CAENINFO: break;
            //case TRIG: break;
            //case EPED: break;
            //case RHDR: break;
            //case CAST: break;
            //case CAAC: break;
            //default:
            }
            else {
                printf("unknown packet type\n");
                // do something
                break;
            }
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

