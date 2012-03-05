#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <jemalloc/jemalloc.h>
#include <TFile.h>
#include <TTree.h>
#include "PackedEvent.hh"
#include "listener.h"
#include "ds.h"

extern Buffer* event_buffer;
extern Buffer* event_header_buffer;
extern Buffer* run_header_buffer;

extern pthread_mutex_t writer_mutex;
extern TFile* outfile;
extern TTree* tree;
extern RAT::DS::PackedRec* rec;

int sockfd;
int thread_sockfd[NUM_THREADS];

void close_sockets() {
    int i;
    for (i=0; i<NUM_THREADS; i++)
        close(thread_sockfd[i]);
    close(sockfd);
}

void handler(int signal) {
    if (signal == SIGINT) {
        printf("\nCaught CTRL-C (SIGINT), Exiting...\n");
        if (!buffer_isempty(event_buffer)) {
            printf("Warning: exiting with non-empty event buffer\n");
            buffer_status(event_buffer);
        }
        if (!buffer_isempty(event_header_buffer)) {
            printf("Warning: exiting with non-empty event header buffer\n");
            buffer_status(event_header_buffer);
        }
        if (!buffer_isempty(run_header_buffer)) {
            printf("Warning: exiting with non-empty run header buffer\n");
            buffer_status(run_header_buffer);
        }

        printf("Closing sockets...\n");
        close_sockets();

        if (outfile && tree) {
            printf("Closing run file...\n");
            pthread_mutex_lock(&writer_mutex);
            outfile->cd();
            tree->Write();
            outfile->Close();
            pthread_mutex_unlock(&writer_mutex);
        }

        exit(0);
    }
    else
        printf("\nCaught signal %i\n", signal);
}

void die(const char *msg) {
    perror(msg);
    exit(1);
}

void accept_xl3packet(void* packet_buffer) {
    XL3Packet* p = (XL3Packet*) realloc(packet_buffer, sizeof(XL3Packet));
    // fixme: check packet type to ensure megabundle
    int nbundles = p->cmdHeader.num_bundles;
    int ibundle;
    XL3PMTBundle* pmtb = (XL3PMTBundle*) (p->payload);
    for(ibundle=0; ibundle<nbundles; ibundle++) {
        uint32_t gtid = pmtbundle_gtid(pmtb) + p->cmdHeader.packet_num + (p->cmdHeader.packet_type<<16);
        uint64_t keyid = buffer_keyid(event_buffer, gtid);
        uint32_t chan = get_bits(pmtb->word[0], 16, 5);
        uint32_t card = get_bits(pmtb->word[0], 26, 4);
        uint32_t crate = get_bits(pmtb->word[0], 21, 5);
        uint32_t pmtid = 512*crate + 32*card + chan;

        EventRecord* er;
        RAT::DS::PackedEvent* e;
        RecordType r;
        buffer_at(event_buffer, gtid, &r, (void**)&er);
        pthread_mutex_lock(&(event_buffer->mutex_buffer[keyid]));
        if (!er) {
            er = new EventRecord();
            e = new RAT::DS::PackedEvent();
            e->EVOrder = gtid;
            er->gtid = gtid;
            er->arrival_time = clock();
            er->event = e;
            buffer_insert(event_buffer, gtid, DETECTOR_EVENT, (void*)er);
        }
        pthread_mutex_unlock(&(event_buffer->mutex_buffer[keyid]));

        if (er && er->gtid!=gtid) {
            printf("accept_xl3packet: buffer overflow! ignoring gtid %i\n", gtid);
            continue;
        }

        // convert bundle from xl3 packet format to rat ds format
        RAT::DS::PMTBundle rpmtb;
        for (int i=0; i<3; i++)
            rpmtb.Word[i] = pmtb->word[i];
        er->event->PMTBundles.push_back(rpmtb);
        pthread_mutex_lock(&(event_buffer->mutex_buffer[keyid]));
        er->event->NHits++;
        pthread_mutex_unlock(&(event_buffer->mutex_buffer[keyid]));
        pmtb++;
    }

    free(p);
}

void* listener_child(void* psock) {
    int sock = *((int*) psock);
    void* packet_buffer = malloc(MAX_BUFFER_LEN);

    signal(SIGINT, &handler);
    while(1) {
        memset(packet_buffer, 0, MAX_BUFFER_LEN);
        int r = recv(sock, packet_buffer, MAX_BUFFER_LEN, 0);
        if (r<0) {
            die("listener_child: error reading from socket");
            return NULL;
        }
        else if (r==0) {
            printf("listener_child: client terminated connection on socket %i\n", sock);
            return NULL;
        }
        else {
            PacketType packet_type = (PacketType) ((PacketHeader*) packet_buffer)->type;
            if (packet_type == XL3_PACKET)
                accept_xl3packet(packet_buffer);
            else if (packet_type == TRIG_PACKET)
                continue;
            else if (packet_type == EPED_PACKET)
                continue;
            else if (packet_type == RHDR_PACKET)
                continue;
            else {
                printf("listener_child: unknown packet type %i on socket %i\n", packet_type, sock);
                // do something
                break;
            }
        }
    }
}

void* listener(void* ptr) {
    int portno;
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;

    portno = *((int*)ptr);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        die("listener: error opening socket");

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    if (bind(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0)
        die("listener: error on binding");

    listen(sockfd, 5);

    clilen = sizeof(cli_addr);
    pthread_t threads[NUM_THREADS];

    signal(SIGINT, &handler);
    int thread_index = 0;
    while(1) {
        int newsockfd = accept(sockfd, (struct sockaddr*) &cli_addr, &clilen);
        if (newsockfd < 0)
            die("listener: error on accept");
        else {
            thread_sockfd[thread_index] = newsockfd;
            printf("listener: spawning listener_child thread with index %i\n", thread_index);
            pthread_create(&(threads[thread_index]),
                           NULL,
                           listener_child,
                           (void*)&(thread_sockfd[thread_index]));
            thread_index++;
        }
    }

    close_sockets();
}

