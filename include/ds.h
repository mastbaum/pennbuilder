typedef unsigned long uint64_t; 
typedef unsigned int uint32_t;
typedef unsigned short uint16_t;

#define NPMTS 10000

/** Event Builder structs */

/// PMTBundle contains raw PMT data packed into 3 32-bit words (96 bits)
typedef struct
{
    uint32_t word1;
    uint32_t word2;
    uint32_t word3;
} PMTBundle;

inline void pmtbundle_print(PMTBundle* p)
{
    printf("PMTBundle at %p:\n", p);
    printf("  word1 =  %u:\n", p->word1);
    printf("  word2 =  %u:\n", p->word2);
    printf("  word3 =  %u:\n", p->word3);
}

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
} Event;

inline void event_clear(Event* e)
{
    int i;
    for(i=0; i<NPMTS; i++)
        if(e->pmt[i]) {
            free(e->pmt[i]);
            e->pmt[i] = NULL;
        }
}

#define BUFFER_SIZE 2000
#define NUM_OF_ELEMS (BUFFER_SIZE-1)

/** Ring FIFO buffer */
typedef struct
{
    uint64_t end;
    uint64_t start;
    uint64_t offset; // index-gtid offset (first gtid)
    uint64_t size;
    Event* keys[BUFFER_SIZE];
} Buffer;
 
Buffer* buffer_alloc(Buffer** pb)
{
    int sz = sizeof(Buffer);
    *pb = malloc(sz);
    if(*pb)
    {
        printf("Initializing buffer: keys[%d] (%d)\n", BUFFER_SIZE, sz);
        (*pb)->size = BUFFER_SIZE;
        (*pb)->end = 0;
        (*pb)->start  = 0;
        (*pb)->offset = 0;

        int i;
        for(i=0; i<(*pb)->size; i++) {
            Event* e = malloc(sizeof(Event));
            int j;
            for(j=0;j<10000;j++) {
                PMTBundle* p = malloc(sizeof(PMTBundle));
                p->word1 = i*100;
                e->pmt[j] = p;
            }
            (*pb)->keys[i] = e;
        }
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
 
inline int buffer_push(Buffer* b, Event* key)
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
        pk = b->keys[b->start];
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

inline void buffer_clear(Buffer* b)
{
    int i;
    for(i=0; i<b->size; i++)
        if(b->keys[i]) {
            event_clear(b->keys[i]);
            free(b->keys[i]);
            b->keys[i] = NULL;
        }
    b->end = 0;
    b->start = 0;
}

// random access
inline int buffer_at(Buffer* b, unsigned int id, Event** pk)
{
    int keyid = (id - b->offset) % b->size;
    if (keyid < b->size) {
        (*pk) = b->keys[keyid];
        return pk == NULL ? 1 : 0;
    }
    else
        return 1;
}

inline int buffer_insert(Buffer* b, unsigned int id, Event* pk)
{
    int keyid = (id - b->offset) % b->size;
    if (!b->keys[keyid] && (keyid < b->size)) {
        b->keys[keyid] = pk;
        return 0;
    }
    else
        return 1;
}

