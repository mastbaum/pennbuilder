#ifndef __PENNBUILDER_DS__
#define __PENNBUILDER_DS__

#include <stdint.h>
#include <time.h>

class PackedEvent;

inline uint32_t get_bits(uint32_t x, uint32_t position, uint32_t count) {
  uint32_t shifted = x >> position;
  uint32_t mask = ((uint32_t)1 << count) - 1;
  return shifted & mask;
}

/// PMTBundle contains raw PMT data packed into 3 32-bit words (96 bits)
struct XL3Bundle {
    uint32_t word[3];
};

void pmtbundle_print(XL3PMTBundle* p);
uint32_t pmtbundle_gtid(XL3PMTBundle* p);
uint32_t pmtbundle_pmtid(XL3PMTBundle* p);

/// Event plus metadata
struct EventRecord {
    bool has_bundles;
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
            this->elem = calloc(this->size, sizeof(T*));
        }
        ~Buffer() {
            for (int i=0; i<this->size; i++)
                delete elem[i];
                elem[i] = NULL;
        }

        uint64_t write;
        uint64_t read;
        uint32_t size;
        T** elem;
};

#endif

