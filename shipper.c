#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <limits.h>
#include <jemalloc/jemalloc.h>
//#include "shipper.h"
#include "ds.h"

extern Buffer* event_buffer;
extern Buffer* event_header_buffer;
extern Buffer* run_header_buffer;
extern uint32_t last_gtid[NUM_PMTS];

void handler(int signal);

void* sender(void* ptr)
{
    signal(SIGINT, &handler);
    while(1) {
        uint32_t min_gtid = UINT_MAX;
        uint32_t ipmt;
        for(ipmt=0; ipmt<NUM_PMTS; ipmt++)
            if(min_gtid > last_gtid[ipmt]) // && pmt_enabled[pmtid]
                min_gtid = last_gtid[ipmt];

        while(((Event*) event_buffer->keys[event_buffer->start])->gtid <= min_gtid) {
            Event* e;
            RecordType r;
            buffer_pop(event_buffer, e->gtid, &r, &e);
            // do something with e
            printf("popping e: %p, gtid %i\n", e, e->gtid);
            free(e);
        }
    }
}

