#include <pthread.h>
#include <stdint.h>
#include <time.h>

#define NPMTS 19 * 16 * 32

/** Event Builder structs */

/// PMTBundle contains raw PMT data packed into 3 32-bit words (96 bits)
typedef struct
{
    uint32_t word[3];
} PMTBundle;

void pmtbundle_print(PMTBundle* p);
uint32_t pmtbundle_pmtid(PMTBundle* p);
uint32_t pmtbundle_gtid(PMTBundle* p);

/// MTCData contains trigger information. Format unknown AToW. (192 bits)
typedef struct
{
    uint32_t word[6];
} MTCData;

/// CAENData contains digitized trigger sums for up to 8 channels (12.8k bits)
typedef struct
{
    uint32_t header[4];
    uint32_t data[8][55]; // v1720 packs data like so (2.5 samples/word)
} CAENData;

/// Event contains all data for SNO+ event. (973k bits = 120 KB)
/// At 120 KB/event, a PC with 24GB of RAM can store 200000 events in memory
typedef struct
{
    PMTBundle pmt[NPMTS];
    MTCData mtc;
    CAENData caen;
    uint32_t gtid;
    struct timespec builder_arrival_time;
    uint32_t run_id;
    uint32_t subrun_id;
    uint32_t nhits;
    uint32_t evorder;
    uint64_t runmask;
    uint8_t pack_ver;
    uint8_t mcflag;
    uint8_t datatype;
    uint8_t clockstat;
} Event;

typedef struct
{
    uint16_t type;
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
    uint16_t type;
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
    uint32_t type;
    uint32_t date;
    uint32_t time;
    uint32_t daq_ver;
    uint32_t calib_trial_id;
    uint32_t srcmask;
    uint32_t runmask;
    uint32_t cratemask;
    uint32_t first_event_id;
    uint32_t valid_event_id;
    uint32_t run_id;
} RHDR;

#define MAX_ROPES 10
typedef struct
{
    uint16_t type;
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
    uint16_t type;
    float av_pos[3];
    float av_roll[3];  // roll, pitch and yaw
    float av_rope_length[7];
} CAAC;

#define BUFFER_SIZE 10000
#define NUM_OF_ELEMS (BUFFER_SIZE-1)

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
    uint64_t end;
    uint64_t start;
    uint64_t offset; // index-gtid offset (first gtid)
    uint64_t size;
    void** keys;
    RecordType* type;
    pthread_mutex_t mutex;
} Buffer;
 
Buffer* buffer_alloc(Buffer** pb, int size);
int buffer_isfull(Buffer* b);
int buffer_isempty(Buffer* b);
int buffer_push(Buffer* b, RecordType type, void* key);
int buffer_pop(Buffer* b, RecordType* type, void** pk);
void buffer_status(Buffer* b);
void buffer_clear(Buffer* b);
int buffer_at(Buffer* b, unsigned int id, RecordType* type, void** pk);
int buffer_insert(Buffer* b, unsigned int id, RecordType type, void* pk);

