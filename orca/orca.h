#ifndef __PENNBUILDER_ORCA__
#define __PENNBUILDER_ORCA__

#include <stdint.h>

#include <ORCompoundDataProcessor.hh>
#include <ORMTCDecoder.hh>
#include <ORPMTDecoder.hh>
#include <ORRunDecoder.hh>
#include <ORCaen1720Decoder.hh>

#include <PackedEvent.hh>

struct OrcaURL {
    char* host;
    int port;
};

class ORBuilderProcessor : public ORCompoundDataProcessor {
    public:
        ORBuilderProcessor(std::string label="");
        virtual ~ORBuilderProcessor();
        virtual EReturnCode StartRun();
        virtual EReturnCode ProcessDataRecord(UInt_t* record);
        virtual EReturnCode EndRun();

    protected:
        ORMTCDecoder fMTCDecoder;
        ORPMTDecoder fPMTDecoder;
        ORCaen1720Decoder fCaenDecoder;
        ORRunDecoder fRunDecoder;

        ORDataProcessor* fMTCProcessor;
        ORDataProcessor* fPMTProcessor;
        ORDataProcessor* fCaenProcessor;
        ORDataProcessor* fRunProcessor;

        bool fFirstGTIdSet;
        uint32_t fCurrentGTId;
        uint64_t fEventOrder;

        uint32_t fCaenOffset;
        uint32_t fCaenLastGTId;

        UInt_t fMTCDataId;
        UInt_t fCaenDataId;
        UInt_t fPMTDataId;
        UInt_t fRunDataId;
};

void* orca_listener(void* arg);

#endif

