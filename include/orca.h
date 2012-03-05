#ifndef __PENNUBUILDER_ORCA__
#define __PENNUBUILDER_ORCA__

#include <string>

#include<ORCompoundDataProcessor.hh>
#include<ORMTCDecoder.hh>
#include<ORPMTDecoder.hh>
#include<ORRunDecoder.hh>
#include<ORCaen1720Decoder.hh>

class OrcaReader;

class ORBuilderProcessor : public ORCompoundDataProcessor
{
    public:
        ORBuilderProcessor(std::string label = "SNOPackedFile.root");
        virtual ~ORBuilderProcessor();
        virtual EReturnCode StartRun();
        virtual EReturnCode ProcessDataRecord(UInt_t* record);
        virtual EReturnCode EndRun();

        void SetReader(OrcaReader* reader) { fReader = reader; }

    protected:
        ORMTCDecoder fMTCDecoder;
        ORPMTDecoder fPMTDecoder;
        ORCaen1720Decoder fCaenDecoder;
        ORRunDecoder fRunDecoder;
        UInt_t fMTCDataId; 
        UInt_t fPMTDataId; 
        UInt_t fCaenDataId;
        UInt_t fRunId;
        bool fMustSwap;

        ORDataProcessor* fMTCProcessor;
        ORDataProcessor* fPMTProcessor;
        ORDataProcessor* fCaenProcessor;
        ORDataProcessor* fRunProcessor;
        OrcaReader* fReader;
};

#endif

