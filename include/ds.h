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
    uint32_t word[3];
} PMTBundle;

void pmtbundle_print(PMTBundle* p);

/// MTCData contains trigger information. Format unknown AToW. (192 bits)
typedef struct
{
    uint32_t word[6];
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
    short type;
    PMTBundle pmt[NPMTS]; // using a pointer array saves space for nhit < 3333
    MTCData mtc;
    CAENData caen;
    uint32_t run_id;
    uint32_t subrun_id;
    uint32_t nhits;
    uint32_t evorder; //?
    uint64_t runmask;
    char pack_ver;
    char mcflag;
    char datatype; //?
    char clockstat; //?

    pthread_mutex_t mutex; // get me out of here!
} Event;

void event_clear(Event* e);

typedef struct
{
    short type;
    uint32_t gtdelay_coarse;
    uint32_t gtdelay_fine;
    uint32_t qped_amp;
    uint32_t qped_width;
    uint32_t pattern_id;
    uint32_t caltype;
    uint32_t event_id;  // GTID of first events in this bank's validity
    uint32_t run_id;    // Double-check on the run
} EPED;

typedef struct
{
    short type;
    // Arrays correspond to:
    // N100Lo, N100Med, N100Hi, N20, N20LB, ESUMLo, ESUMHi, OWLn, OWLELo, OWLEHi
    uint32_t trigmask;
    uint16_t threshold[10];
    uint16_t trig_zero_offset[10];
    uint32_t pulser_rate;
    uint32_t mtc_csr;
    uint32_t lockout_width;
    uint32_t prescale_freq;
    uint32_t event_id;  // GTID of first events in this bank's validity
    uint32_t wun_id;    // Double-check on the run
} TRIG;

typedef struct
{
    short type;
    uint32_t date;
    uint32_t time;
    char daq_ver;
    uint32_t calib_trial_id;
    uint32_t srcmask;
    uint32_t runmask;
    uint32_t cratemask;
    uint32_t first_event_id;
    uint32_t valid_event_id;
    uint32_t run_id;    // Double-check on the run
} RHDR;

#define MAX_ROPES 10
typedef struct
{
    short type;
    uint16_t source_id;
    uint16_t source_stat;
    uint16_t nropes;
    float manip_pos[3];
    float manip_dest[3];
    float srcpos_uncert1;
    float srcpos_uncert2[3];
    float lball_orient;
    int rope_id[MAX_ROPES];
    float rope_len[MAX_ROPES];
    float rope_targ_len[MAX_ROPES];
    float rope_vel[MAX_ROPES];
    float rope_tens[MAX_ROPES];
    float rope_err[MAX_ROPES];
} CAST;

typedef struct
{
    short type;
    float av_pos[3];
    float av_roll[3];  // roll, pitch and yaw
    float av_rope_length[7];
} CAAC;

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

