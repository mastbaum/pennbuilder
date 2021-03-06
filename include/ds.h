#ifndef __PENNBUILDER_DS__
#define __PENNBUILDER_DS__

#include <pthread.h>
#include <stdint.h>
#include <time.h>

/** Data structure structs 
 *
 *  Largely copied from RAT::DS:PackedEvent
 */

#define NPMTS 19 * 16 * 32
#define MAX_ROPES 10

uint32_t get_bits(uint32_t x, uint32_t position, uint32_t count);

/// PMTBundle contains raw PMT data packed into 3 32-bit words (96 bits)
typedef struct
{
    uint32_t word[3];
} PMTBundle;

// print some pmt bundle info (address, gtid, pmtid) for debugging
void pmtbundle_print(PMTBundle* p);

// extract ids from packed pmt bundle
uint32_t pmtbundle_pmtid(PMTBundle* p);
uint32_t pmtbundle_gtid(PMTBundle* p);

/// CAENData contains digitized trigger sums for up to 8 channels (12.8k bits)
typedef struct
{
    uint32_t header[4];
    uint32_t data[8][55]; // v1720 packs data like so (2.5 samples/word)
} CAENData;

/** Ring FIFO buffer
 *
 *  Data (keys) stored as void*, type given in field type, as defined in enum
 *  RecordType.
 *
 *  Based on example found at http://en.wikipedia.org/wiki/Circular_buffer.
 */

typedef enum {
    EMPTY,
    DETECTOR_EVENT,
    RUN_HEADER,
    AV_STATUS_HEADER,
    MANIPULATOR_STATUS_HEADER,
    TRIG_BANK_HEADER,
    EPED_BANK_HEADER
} RecordType;

typedef struct
{
    uint64_t write;
    uint64_t read;
    uint64_t offset; // index-gtid offset (first gtid)
    uint64_t size;
    void** keys;
    RecordType* type;

    pthread_mutex_t* mutex_buffer; // lock elements individually
    pthread_mutex_t mutex_write;
    pthread_mutex_t mutex_read;
    pthread_mutex_t mutex_offset;
    pthread_mutex_t mutex_size;
} Buffer;

// allocate memory for and initialize a ring buffer
int buffer_alloc(Buffer** pb, int size);

// print buffer status information for debugging
void buffer_status(Buffer* b);

// re-initialize a buffer; frees memory held by (pointer) elements
void buffer_clear(Buffer* b);

// returns the array index corresponding to gtid id
uint64_t buffer_keyid(Buffer* b, unsigned int id);

// get an element out of the buffer at gtid id
int buffer_at(Buffer* b, unsigned int id, RecordType* type, void** pk);

// insert an element into the buffer at gtid id. mutex locking done by user.
int buffer_insert(Buffer* b, unsigned int id, RecordType type, void* pk);

int buffer_isfull(Buffer* b);
int buffer_isempty(Buffer* b);
int buffer_push(Buffer* b, RecordType type, void* key);
int buffer_pop(Buffer* b, RecordType* type, void** pk);

#endif

