#include <pthread.h>

typedef unsigned long uint64_t; 
typedef unsigned int uint32_t;
typedef unsigned short uint16_t;

#define NPMTS 10000

/** Event Builder structs */

/// PMTBundle contains raw PMT data packed into 3 32-bit words (96 bits)
typedef struct
{
    int pmtid; // cheat for testing
    int gtid;
    uint32_t word1;
    uint32_t word2;
    uint32_t word3;
} PMTBundle;

void pmtbundle_print(PMTBundle* p);

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
    PMTBundle* pmt[NPMTS]; // using a pointer array saves space for nhit < 3333
    MTCData mtc;
    CAENData caen;
    pthread_mutex_t mutex;
} Event;

void event_clear(Event* e);

#define BUFFER_SIZE 2000
#define NUM_OF_ELEMS (BUFFER_SIZE-1)

/** Ring FIFO buffer
 *
 *  Based on example found at 
 *  http://en.wikipedia.org/wiki/Circular_buffer.
 */
typedef struct
{
    uint64_t end;
    uint64_t start;
    uint64_t offset; // index-gtid offset (first gtid)
    uint64_t size;
    Event* keys[BUFFER_SIZE];
    pthread_mutex_t mutex;
} Buffer;
 
Buffer* buffer_alloc(Buffer** pb);
int buffer_isfull(Buffer* b);
int buffer_isempty(Buffer* b);
int buffer_push(Buffer* b, Event* key);
int buffer_pop(Buffer* b, Event* pk);
void buffer_status(Buffer* b);
void buffer_clear(Buffer* b);
int buffer_at(Buffer* b, unsigned int id, Event** pk);
int buffer_insert(Buffer* b, unsigned int id, Event* pk);

