#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
//#include <jemalloc/jemalloc.h>
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

#define EVENT_BUFFER_SIZE 10000
#define EVENT_HEADER_BUFFER_SIZE 50
#define RUN_HEADER_BUFFER_SIZE 20

Buffer* event_buffer;
Buffer* event_header_buffer;
Buffer* run_header_buffer;

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
    buffer_alloc(&event_buffer, EVENT_BUFFER_SIZE);
    buffer_alloc(&event_header_buffer, EVENT_HEADER_BUFFER_SIZE);
    buffer_alloc(&run_header_buffer, RUN_HEADER_BUFFER_SIZE);

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

    return 0;
}

