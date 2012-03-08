#ifndef __PENNBUILDER_SHIPPER__
#define __PENNBUILDER_SHIPPER__

#define QUEUE_DELAY 0.1
#define SKIP_GTID_DELAY 1.0
#define DISPATCHER_ADDRESS "tcp://*:5024"
#define MAX_RHDR_WAIT 100000*50

// CDABHeader: a header that precedes each structure in a CDAB file
typedef struct {
    uint32_t record_type;
    uint32_t size;
} CDABHeader;

void* shipper(void* ptr);

#endif

