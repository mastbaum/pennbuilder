#include <stdio.h>
#include <string.h>
#include <jemalloc/jemalloc.h>
#include "ds.h"

/** Event Builder for SNO+, C edition
 *  
 *  Enqueues incoming raw data in a ring buffer, and writes out to RAT files
 *  as events are finished (per XL3 flag) or buffer is filling up.
 *
 *  Ring buffer based on example found at 
 *  http://en.wikipedia.org/wiki/Circular_buffer.
 *  
 *  Andy Mastbaum (mastbaum@hep.upenn.edu), June 2011
 */ 
 
int main(int argc, char *argv[])
{
    Buffer* b;
    uint64_t a;
    int empty;

    buffer_alloc(&b);

    printf("%d %d %d\n", sizeof(uint16_t), sizeof(uint32_t), sizeof(uint64_t));

    Event* e;
    buffer_at(b, 32, &e);
    printf("%li\n", e->pmt[1234]->word1);

    buffer_clear(b);
    free(b);
    return 0;
}

