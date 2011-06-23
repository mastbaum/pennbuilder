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
#include "shipper.h"
#include "ds.h"

extern Buffer* event_buffer;
extern Buffer* event_header_buffer;
extern Buffer* run_header_buffer;
extern uint32_t last_gtid[NPMTS];

void handler(int signal);

void* shipper(void* ptr)
{
    signal(SIGINT, &handler);
    while(1) {
        uint32_t min_gtid = UINT_MAX;
        uint32_t ipmt;
        for(ipmt=0; ipmt<NPMTS; ipmt++)
            if(min_gtid > last_gtid[ipmt]) // && pmt_enabled[pmtid]
                min_gtid = last_gtid[ipmt];

        if(event_buffer->keys[event_buffer->start])
            while(((Event*) event_buffer->keys[event_buffer->start])->gtid <= min_gtid) {
                Event* e;
                RecordType r;
                buffer_pop(event_buffer, &r, (void*)&e);
                if(!e) {
                    printf("Sender ran out of buffer\n");
                    break;
                }
                else {
                    // do something with e
                    printf("popping e: %p, gtid %i\n", e, e->gtid);
                    free(e);
                }
            }
    }
}
