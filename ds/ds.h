#ifndef __PENNBUILDER_DS__
#define __PENNBUILDER_DS__

#include <stdint.h>
#include <time.h>
#include <jemalloc/jemalloc.h>

#include <PackedEvent.hh>

class PackedEvent;

inline uint32_t get_bits(uint32_t x, uint32_t position, uint32_t count) {
  uint32_t shifted = x >> position;
  uint32_t mask = ((uint32_t)1 << count) - 1;
  return shifted & mask;
}

/* PMTBundle contains raw PMT data packed into 3 32-bit words (96 bits) */
struct XL3PMTBundle {
    uint32_t word[3];
};

void pmtbundle_print(XL3PMTBundle* p);
uint32_t pmtbundle_gtid(XL3PMTBundle* p);
uint32_t pmtbundle_pmtid(XL3PMTBundle* p);

/* Event plus metadata */
struct EventRecord {
    bool has_pmt;
    bool has_mtc;
    bool has_caen;
    uint32_t gtid;
    clock_t arrival_time;
    RAT::DS::PackedEvent* event;
};

/** Decorated array */
template <class T>
class Buffer {
    public:
        Buffer(uint32_t _size) : write(0), read(0), size(_size) {
            this->elem = static_cast<T*>(calloc(this->size, sizeof(T)));
            this->mutex = static_cast<pthread_mutex_t*>(calloc(this->size, sizeof(pthread_mutex_t)));
            for (int i=0; i<this->size; i++)
                pthread_mutex_init(&mutex[i], NULL);
        }
        ~Buffer() {
            for (int i=0; i<this->size; i++) {
                delete this->elem[i];
                this->elem[i] = NULL;
                pthread_mutex_destroy(&mutex[i]);
            }
        }

        uint64_t write;
        uint64_t read;
        uint32_t size;
        pthread_mutex_t* mutex;
        T* elem;
};

/** Builder state */
class BuilderStats {
    public:
        BuilderStats() : run_id(0), events_written(0), events_with_pmt(0), events_with_mtc(0), events_with_caen(0) {
            filename = (char*) calloc(100, sizeof(char));
        }
        uint64_t events_written;
        uint64_t events_with_pmt;
        uint64_t events_with_mtc;
        uint64_t events_with_caen;
        uint64_t records_received;
        uint64_t records_unhandled;
        uint32_t run_id;
        bool run_active;
        char* filename;
};

#endif

