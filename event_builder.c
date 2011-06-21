#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include "listener.h"
#include "ds.h"

/** Event Builder for SNO+, C edition
 *  
 *  Enqueues incoming raw data in a ring buffer, and writes out to RAT files
 *  as events are finished (per XL3 flag) or buffer is filling up.
 *
 *  Andy Mastbaum (mastbaum@hep.upenn.edu), June 2011
 */ 
 
Buffer* b;

int main(int argc, char *argv[])
{
    int port;
    if(argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        exit(1);
    }
    else
        port = atoi(argv[1]);

    buffer_alloc(&b);

    pthread_t tlistener;
    pthread_create(&tlistener, NULL, listener, (void*)&port);

    //pthread_t tshipper;
    //pthread_create(&tshipper, NULL, ship, NULL);

    pthread_join(tlistener, NULL);
    //pthread_join(tshipper, NULL);

    buffer_status(b);
    buffer_clear(b);
    free(b);
    return 0;
}

