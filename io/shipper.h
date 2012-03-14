#ifndef __PENNBUILDER_SHIPPER__
#define __PENNBUILDER_SHIPPER__

#define QUEUE_DELAY 5.0
#define SKIP_GTID_DELAY 5.0
#define DISPATCHER_ADDRESS "tcp://*:5024"

void* shipper(void* ptr);

#endif

