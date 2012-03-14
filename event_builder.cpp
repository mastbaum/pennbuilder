#include <iostream>
#include <queue>

#include <pthread.h>
#include <jemalloc/jemalloc.h>

#include <listener.h>
#include <orca.h>
#include <shipper.h>
#include <ds.h>

/** Event Builder for SNO+, C++ edition
 *
 *  Enqueues incoming raw data in ring buffers, and writes out to disk and
 *  a socket as events are finished.
 *
 *  Andy Mastbaum (mastbaum@hep.upenn.edu), June 2011
 */ 

#define EVENT_BUFFER_SIZE 500000
#define EVENT_HEADER_BUFFER_SIZE 50
#define RUN_HEADER_BUFFER_SIZE 20

Buffer event_buffer(EVENT_BUFFER_SIZE);
std::deque<RAT::DS::GenericRec*> event_header_buffer;
std::deque<RAT::DS::GenericRec*> run_header_buffer;

int main(int argc, char *argv[])
{
    int port;

    if (argc > 1) {
        port = atoi(argv[1]);
    }
    else {
        printf("Usage: %s <port> [orca_address orca_port]\n", argv[0]);
        return 1;
    }

    OrcaURL* url = NULL;
    if (argc == 4) {
        url = new OrcaURL();
        url->host = argv[2];
        url->port = atoi(argv[3]);
    }

    // launch listener (input) and shipper (output) threads
    pthread_t tlistener;
    pthread_create(&tlistener, NULL, listener, (void*)&port);

    pthread_t tshipper;
    pthread_create(&tshipper, NULL, shipper, NULL);

    pthread_t torcalistener;
    if (url) {
        pthread_create(&torcalistener, NULL, orca_listener, (void*)url);
    }

    // wait for threads to join before exiting
    pthread_join(tlistener, NULL);
    pthread_join(tshipper, NULL);
    if (url) {
        pthread_join(torcalistener, NULL);
    }

    delete url;

    event_header_buffer.clear();
    run_header_buffer.clear();

    return 0;
}

