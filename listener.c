#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <jemalloc/jemalloc.h>
#include "listener.h"
#include "ds.h"

extern Buffer* event_buffer;
extern Buffer* event_header_buffer;
extern Buffer* run_header_buffer;
extern uint32_t last_gtid[NPMTS];
extern FILE* outfile;

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
        printf("Closing sockets...\n");
        close_sockets();
        printf("Closing run file...\n");
        fclose(outfile);
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

void accept_xl3packet(void* packet_buffer)
{
    XL3Packet* p = realloc(packet_buffer, sizeof(XL3Packet));
    // fixme: check packet type to ensure megabundle
    int nbundles = p->cmdHeader.num_bundles;
    int ibundle;
    PMTBundle* pmtb = (PMTBundle*) (p->payload);
    for(ibundle=0; ibundle<nbundles; ibundle++) {
        uint32_t gtid = pmtbundle_gtid(pmtb) + p->cmdHeader.packet_num + (p->cmdHeader.packet_type<<16);
        uint32_t chan = get_bits(pmtb->word[0], 16, 5);
        uint32_t card = get_bits(pmtb->word[0], 26, 4);
        uint32_t crate = get_bits(pmtb->word[0], 21, 5);
        uint32_t pmtid = 512*crate + 32*card + chan;

        // fake to look more like the sequencer for demo purposes
        int last_card = card >= read_pos[crate] ? card: card + NFECS;
        //printf("read %d,%d (%d), last time was %d\n",crate,card,last_card,read_pos[crate]);
        int icard;
        for (icard = read_pos[crate]+1;icard<last_card;icard++){
           uint32_t first_pmtid = 512*crate+32*icard;
           int ipmtid;
           for (ipmtid = first_pmtid; ipmtid<first_pmtid+32;ipmtid++)
               if (crate_gtid_last[crate] > last_gtid[ipmtid])
                   last_gtid[ipmtid] = crate_gtid_last[crate];
        }
        crate_gtid_last[crate] = gtid;
        read_pos[crate] = card;

        /*
        // if we have skipped a card twice, the entire crate must be finished
        // up to the gtid that card was on last, and we can update the 
        // last_gtid array.
        if(card <= read_pos[crate]) {
            int icard;
            for(icard=0; icard<NFECS; icard++)
                if((1<<icard) & crate_skipped[crate]) {
                    uint32_t first_pmtid = 512*crate + 32*card;
                    int ipmtid;
                    for(ipmtid=first_pmtid; ipmtid<first_pmtid+32; ipmtid++) 
                        last_gtid[ipmtid] = crate_gtid_last[crate];
                    if(crate_gtid_current[crate] > crate_gtid_last[crate])
                        crate_gtid_last[crate] = crate_gtid_current[crate];
                    crate_skipped[crate] = 1<<card;
                    read_pos[crate] = card;
                }
                if(gtid > crate_gtid_current[crate])
                    crate_gtid_current[crate] = gtid;
                crate_skipped[crate] |= 1<<card;
                read_pos[crate] = card;
        }
        */ 
        // similarly, look at sequencer skips
        int last_chan = chan > seq_pos[crate][card] ? chan: chan + NCHANS;
        int ichan;
        for(ichan=seq_pos[crate][card]+1; ichan<last_chan; ichan++) {
            int ch = ichan % 32;
            uint32_t ccid = 512*crate + 32*card;
            if (last_gtid[ccid+seq_pos[crate][card]] > last_gtid[ccid + ch])
                last_gtid[ccid + ch] = last_gtid[ccid + seq_pos[crate][card]];
        }
        seq_pos[crate][card] = chan;

        if(gtid > last_gtid[pmtid])
            last_gtid[pmtid] = (uint32_t)gtid;
        if(gtid < last_gtid[pmtid])
            printf("Warning! Got GTID %d on pmt %d, is < last_gtid %d\n", gtid, pmtid, last_gtid[pmtid]);

        Event* e;
        RecordType r;
        buffer_at(event_buffer, gtid, &r, (void*)&e);
        if(!e) {
            e = malloc(sizeof(Event));
            e->gtid = gtid;
            struct timespec t;
            clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t);
            e->builder_arrival_time = t;
            buffer_insert(event_buffer, gtid, DETECTOR_EVENT, (void*)e);
        }

        if(e && e->gtid!=gtid) {
            printf("Buffer overflow! Ignoring GTID %i\n", gtid);
            continue;
        }

        e->pmt[pmtid] = *pmtb;
        pthread_mutex_lock(&(event_buffer->mutex));
        e->nhits++;
        pthread_mutex_unlock(&(event_buffer->mutex));
        pmtb++;
    }
}

void* listener_child(void* psock)
{
    int sock = *((int*) psock);
    void* packet_buffer = malloc(MAX_BUFFER_LEN);

    signal(SIGINT, &handler);
    while(1) {
        memset(packet_buffer, 0, MAX_BUFFER_LEN);
        int r = recv(sock, packet_buffer, MAX_BUFFER_LEN, 0);
        if(r<0) {
            die("Error reading from socket");
            break;
        }
        else if(r==0) {
            printf("Client terminated connection on socket %i\n", sock);
            break;
        }
        else {
            PacketType packet_type = ((PacketHeader*) packet_buffer)->type;
            if(packet_type == XL3_PACKET)
                accept_xl3packet(packet_buffer);
            else if(packet_type == MTC_PACKET)
                continue;
            else if(packet_type == CAEN_PACKET)
                continue;
            else if(packet_type == TRIG_PACKET)
                continue;
            else if(packet_type == EPED_PACKET)
                continue;
            else if(packet_type == RHDR_PACKET)
                continue;
            else if(packet_type == CAST_PACKET)
                continue;
            else if(packet_type == CAAC_PACKET)
                continue;
            else {
                printf("Unknown packet type %i on socket %i\n", packet_type, sock);
                // do something
                break;
            }
        }
    }
}

void* listener(void* ptr)
{
    memset(&read_pos, 0, NCRATES * sizeof(uint16_t));
    memset(&seq_pos, 0, NCRATES * NFECS * sizeof(uint32_t));
    memset(&crate_gtid_current, 0, NCRATES * sizeof(uint32_t));
    memset(&crate_gtid_last, 0, NCRATES * sizeof(uint32_t));
    memset(&crate_skipped, 0, NCRATES * sizeof(uint16_t));

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
    int thread_index = 0;
    while(1) {
        int newsockfd = accept(sockfd, (struct sockaddr*) &cli_addr, &clilen);
        if(newsockfd < 0) die("ERROR on accept");
        else {
            thread_sockfd[thread_index] = newsockfd;
            printf("Spawning listener_child thread with index %i\n", thread_index);
            pthread_create(&(threads[thread_index]),
                    NULL,
                    listener_child,
                    (void*)&(thread_sockfd[thread_index]));
            thread_index++;
        }
    }

    close_sockets();
}

