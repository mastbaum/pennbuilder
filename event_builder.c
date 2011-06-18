#include <stdio.h>
#include <string.h>
#include <malloc.h>

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
 
#define BUFFER_SIZE 20000
#define NUM_OF_ELEMS (BUFFER_SIZE-1)

typedef unsigned long uint64_t; 
typedef unsigned int uint32_t;
typedef unsigned short uint16_t;

/** Event Builder structs */

/// PMTBundle contains raw PMT data packed into 3 32-bit words (96 bits)
typedef struct
{
    uint32_t word1;
    uint32_t word2;
    uint32_t word3;
} PMTBundle;

/// MTCData contains trigger information. Format unknown AToW. (192 bits)
typedef struct
{
    uint64_t word1;
    uint64_t word2;
    uint64_t word3;
} MTCData;

/// CAENData contains digitized trigger sums for up to 8 channels (12.8k bits)
typedef struct
{
    // 100 samples * 12 bits * 8 channels
    uint16_t data[8][100];
} CAENData;

/// Event contains all data for SNO+ event. (973k bits = 120 KB)
/// At 120 KB/event, a PC with 24GB of RAM can store 200000 events in memory
typedef struct
{
    PMTBundle pmt[10000];
    MTCData mtc;
    CAENData caen;
} Event;

/** Ring FIFO buffer */
typedef struct
{
    uint64_t end;
    uint64_t start;
    uint64_t offset; // index-gtid offset (first gtid)
    uint64_t size;
    Event* keys[0];
} Buffer;
 
Buffer* buffer_alloc(Buffer** pb, int size)
{
    int sz = size * sizeof(Event) + sizeof(Buffer);
    *pb = (Buffer*) malloc(sz);
    if(*pb)
    {
        printf("Initializing buffer: keys[%d] (%d)\n", size, sz);
        (*pb)->size = size;
        (*pb)->end = 0;
        (*pb)->start  = 0;
        (*pb)->offset = 0;

        int i;
        for(i=0; i<size; i++)
            (*pb)->keys[i] = NULL;
    }
    return *pb;
}
 
inline int buffer_isfull(Buffer* b)
{
    return (((b->end + 1) % b->size) == b->start);
}
 
inline int buffer_isempty(Buffer* b)
{
    return (b->start == b->end);
}
 
inline int buffer_push(Buffer* b, uint64_t key)
{
    int full = buffer_isfull(b);
    if(!full)
    {
        b->keys[b->end] = key;
        b->end++;
        b->end %= b->size;
    }
    return !full;
}
 
inline int buffer_pop(Buffer* b, Event* pk)
{
    int empty = buffer_isempty(b);
    if(!empty)
    {
        *pk = b->keys[b->start];
        // legit? slow? have to malloc them now...
        free(b->keys[b->start]);
        b->keys[b->start] = NULL;
        b->start++;
        b->start %= b->size;
    }
    return !empty;
}
 
inline void buffer_status(Buffer* b)
{
    printf("write: %d, read: %d, full: %d, empty: %d\n", b->end,
                                                         b->start,
                                                         buffer_isfull(b),
                                                         buffer_isempty(b));
}

// for testing only
inline void buffer_clear(Buffer* b)
{
    int i;
    for(i=0; i<b->size; i++)
        b->keys[i] = 0;
    b->end = 0;
    b->start = 0;
}

// random access
inline int buffer_at(Buffer* b, unsigned int id, uint16_t* pk)
{
    keyid = (id - b->first_gtid) % b->size;
    if (keyid < b->size) {
        *pk = b->keys[keyid];
        return pk == NULL ? 0 : 1;
    }
    else
        return 0;
}

inline int buffer_insert(Buffer* b, unsigned int id, uint16_t* pk)
{
    keyid = (id - b->first_gtid) % b->size;
    if (keyid < b->size) {
        *pk = b->keys[keyid];
        return pk == NULL ? 0 : 1;
    }
    else
        return 0;
}

/** main */
int main(int argc, char *argv[])
{
    Buffer* b;
    uint64_t a;
    int empty;

    buffer_alloc(&b, BUFFER_SIZE);

    

/*
    int i;
    for(i=0; i<NUM_OF_ELEMS; i++) {
        //printf("push %d\n", i+1);
        if(!buffer_push(b, i+1))
            printf("buffer overflow\n");
    }
    while(buffer_pop(b, &a))
        continue;
        //printf("pop %d\n",a);
*/
    free(b);
    return 0;
}

