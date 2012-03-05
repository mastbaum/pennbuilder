#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>
#include <jemalloc/jemalloc.h>
#include "PackedEvent.hh"

#include "listener.h"
#include "orca.h"
#include "shipper.h"
#include "ds.h"

/** Event Builder for SNO+, C++ edition
 *
 *  Enqueues incoming raw data in ring buffers, and writes out to disk and
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
    std::string orcahost = "localhost";
    int orcaport = 1234;

    if(argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        exit(1);
    }
    else
        port = atoi(argv[1]);

    // initialization
    assert(buffer_alloc(&event_buffer, EVENT_BUFFER_SIZE));
    assert(buffer_alloc(&event_header_buffer, EVENT_HEADER_BUFFER_SIZE));
    assert(buffer_alloc(&run_header_buffer, RUN_HEADER_BUFFER_SIZE));

    // launch listener (input) and shipper (output) threads
    pthread_t tlistener;
    pthread_create(&tlistener, NULL, listener, (void*)&port);

    OrcaURL* url = new OrcaURL();
    url->host = orcahost;
    url->port = orcaport;
    pthread_t torcalistener;
    pthread_create(&torcalistener, NULL, orca_listener, (void*)&url);

    pthread_t tshipper;
    pthread_create(&tshipper, NULL, shipper, NULL);

    // wait for threads to join before exiting
    pthread_join(tlistener, NULL);
    pthread_join(torcalistener, NULL);
    pthread_join(tshipper, NULL);

    return 0;
}

