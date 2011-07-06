#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <limits.h>
#include <jemalloc/jemalloc.h>
#include "shipper.h"
#include "ds.h"

extern Buffer* event_buffer;
extern Buffer* event_header_buffer;
extern Buffer* run_header_buffer;
extern uint32_t last_gtid[NPMTS];

#define MAX_RHDR_WAIT 100000*50
FILE* outfile;

void handler_shipper(int signal)
{
    if(signal == SIGINT)
        fclose(outfile);
};

struct timespec tdiff(struct timespec start, struct timespec end)
{
    struct timespec temp;
    if ((end.tv_nsec-start.tv_nsec)<0) {
        temp.tv_sec = end.tv_sec-start.tv_sec-1;
        temp.tv_nsec = 1000000000+end.tv_nsec-start.tv_nsec;
    } else {
        temp.tv_sec = end.tv_sec-start.tv_sec;
        temp.tv_nsec = end.tv_nsec-start.tv_nsec;
    }
    return temp;
}

void* shipper(void* ptr)
{
    struct timespec time_now;
    outfile = NULL;
    char filename[100];
    int run_active = 0;

    signal(SIGINT, &handler_shipper);
    while(1) {
        uint32_t min_gtid = UINT_MAX;
        uint32_t min_pmt = 0;
        uint32_t ipmt;
        for(ipmt=0; ipmt<NPMTS; ipmt++)
            if(min_gtid > last_gtid[ipmt]) { // && pmt_enabled[pmtid]
                min_gtid = last_gtid[ipmt];
                min_pmt = ipmt;
            }

        printf("min_gtid = %i, min_pmt = %i\n",min_gtid,min_pmt);

        Event* etemp = (Event*) event_buffer->keys[event_buffer->start];
        if(etemp) {
            clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &time_now);
            struct timespec event_arrival_time = etemp->builder_arrival_time;
            if(etemp->gtid > min_gtid) // || tdiff(time_now, event_arrival_time).tv_sec > 1))
                continue;

            // loop through run headers looking for an RHDR
            int irhdr = run_header_buffer->start;
            while(run_header_buffer->keys[irhdr]) {
                if(run_header_buffer->type[irhdr] == RUN_HEADER) {
                    RHDR* h = (RHDR*) run_header_buffer->keys[irhdr];
                    if(h->first_event_id <= etemp->gtid) {
                        if(run_active)
                            fclose(outfile);
                        int run_id = h->run_id;
                        sprintf(filename, "run_%i.cdab", run_id);
                        printf("Starting new run: ID %i, GTID %i (%s)\n", run_id, h->first_event_id, filename);
                        outfile = fopen(filename, "wb+");
                        run_active = 1;
                    }
                }
                irhdr++;
                irhdr %= run_header_buffer->size;
            }

            // hold until we get a run header
            if(!run_active) {
                printf("Waiting on run header...\n");
                continue;
            }

            while(run_header_buffer->keys[run_header_buffer->start]) {
                void* header = run_header_buffer->keys[run_header_buffer->start];
                RecordType r = run_header_buffer->type[run_header_buffer->start];
                int first_gtid;
                if(r == RUN_HEADER)
                    first_gtid = ((RHDR*) header)->first_event_id;
                if(r == AV_STATUS_HEADER)
                    first_gtid = 0; //((CAAC*) header)->first_event_id;
                if(r == MANIPULATOR_STATUS_HEADER)
                    first_gtid = 0; //((CAST*) header)->first_event_id;

                if(first_gtid <= etemp->gtid) {
                    buffer_pop(run_header_buffer, &r, &header);
                    if(r == RUN_HEADER) {
                        RHDR* rhdr = (RHDR*) header;
                        fwrite(rhdr, sizeof(rhdr), 1, outfile);
                    }
                    else if(r == AV_STATUS_HEADER) {
                        CAAC* caac = (CAAC*) header;
                        fwrite(caac, sizeof(caac), 1, outfile);
                    }
                    else if(r == MANIPULATOR_STATUS_HEADER) {
                        CAST* cast = (CAST*) header;
                        fwrite(cast, sizeof(cast), 1, outfile);
                    }
                    else {
                        printf("Encountered header of unknown type %i in run header buffer\n", r);
                        // do something
                    }
                    free(header);
                }
                else
                    break;
            }

            // ship event-level headers
            while(event_header_buffer->keys[event_header_buffer->start]) {
                void* header = event_header_buffer->keys[event_header_buffer->start];
                RecordType r = event_header_buffer->type[event_header_buffer->start];
                int first_gtid;
                if(r == TRIG_BANK_HEADER)
                    first_gtid = ((TRIG*) header)->event_id;
                if(r == EPED_BANK_HEADER)
                    first_gtid = ((EPED*) header)->event_id;

                if(first_gtid < etemp->gtid) {
                    buffer_pop(event_header_buffer, &r, &header);
                    if(r == TRIG_BANK_HEADER) {
                        TRIG* trig = (TRIG*) header;
                        fwrite(trig, sizeof(trig), 1, outfile);
                    }
                    else if(r == EPED_BANK_HEADER) {
                        EPED* eped = (EPED*) header;
                        fwrite(eped, sizeof(eped), 1, outfile);
                    }
                    else {
                        printf("Encountered header of unknown type %i in event header buffer\n", r);
                        // do something
                    }
                    free(header);
                }
                else
                    break;
            }

            // finally, ship the event data
            Event* e;
            RecordType r;
            buffer_pop(event_buffer, &r, (void*)&e);
            if(!e) { printf("omg\n"); continue; }
            //printf("popping e: %p, gtid %i\n", e, e->gtid);
            fwrite(e, sizeof(e), 1, outfile);
            free(e);

            clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &time_now);
        }
    }
}

