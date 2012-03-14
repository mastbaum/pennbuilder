#include <stdint.h>

#include <ds.h>

void pmtbundle_print(XL3PMTBundle* p) {
    printf("PMTBundle at %p:\n", p);
    printf("  pmtid =  %i\n", pmtbundle_pmtid(p));
    printf("  gtid  =  %i\n", pmtbundle_gtid(p));
    int i;
    for(i=0; i<3; i++)
        printf("  word%i =  %u\n", i, p->word[i]);
}

uint32_t pmtbundle_gtid(XL3PMTBundle* p) {
    uint32_t gtid1 = get_bits(p->word[0], 0, 16);
    uint32_t gtid2 = get_bits(p->word[2], 12, 4);
    uint32_t gtid3 = get_bits(p->word[2], 28, 4);
    return (gtid1 + (gtid2<<16) + (gtid3<<20));
}

uint32_t pmtbundle_pmtid(XL3PMTBundle* p) {
    int ichan = get_bits(p->word[0], 16, 5);
    int icard = get_bits(p->word[0], 26, 4);
    int icrate = get_bits(p->word[0], 21, 5);
    return (512*icrate + 32*icard + ichan);
}

