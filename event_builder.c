#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <jemalloc/jemalloc.h>
#include "listener.h"
#include "shipper.h"
#include "ds.h"

/** Event Builder for SNO+, C edition
 *
 *  Enqueues incoming raw data in ring buffers, and writes out to disk and/or
 *  a socket as events are finished.
 *
 *  Andy Mastbaum (mastbaum@hep.upenn.edu), June 2011
 */ 

Buffer* event_buffer;
Buffer* event_header_buffer;
Buffer* run_header_buffer;
uint32_t last_gtid[NPMTS];

int main(int argc, char *argv[])
{
    int port;
    if(argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        exit(1);
    }
    else
        port = atoi(argv[1]);

    // initialization
    buffer_alloc(&event_buffer, 10000);
    buffer_alloc(&event_header_buffer, 50);
    buffer_alloc(&run_header_buffer, 20);
    memset(&last_gtid, 0, NPMTS*sizeof(uint32_t));

    // fake RHDR for testing
    RHDR* rh = malloc(sizeof(RHDR));
    rh->run_id = 123456;
    rh->first_event_id = 0;
    buffer_push(run_header_buffer, RUN_HEADER, rh);

    // launch listener (input) and shipper (output) threads
    pthread_t tlistener;
    pthread_create(&tlistener, NULL, listener, (void*)&port);
    pthread_t tshipper;
    pthread_create(&tshipper, NULL, shipper, NULL);

    // wait for threads to join before exiting
    pthread_join(tlistener, NULL);
    pthread_join(tshipper, NULL);

    // cleanup
    buffer_status(event_buffer);
    buffer_clear(event_buffer);
    free(event_buffer);

    buffer_status(event_header_buffer);
    buffer_clear(event_header_buffer);
    free(event_header_buffer);

    buffer_status(run_header_buffer);
    buffer_clear(run_header_buffer);
    free(run_header_buffer);

    return 0;
}

