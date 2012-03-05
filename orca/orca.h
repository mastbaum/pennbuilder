#ifndef __PENNBUILDER_ORCA__
#define __PENNBUILDER_ORCA__

#include "ORCompoundDataProcessor.hh"
#include "ORFileWriter.hh"
#include "ORMTCDecoder.hh"
#include "ORPMTDecoder.hh"
#include "ORRunDecoder.hh"
#include "ORCaen1720Decoder.hh"
#include "PackedEvent.hh"

#include <map>
#include <string>
#include <vector>

class ORBuilderProcessor : public ORCompoundDataProcessor {
  public:
    ORBuilderProcessor(std::string label = "SNOPackedFile.root");
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

	unsigned int fCaenOffset;
	unsigned int fCaenLastGTId;
	unsigned int pMTCCount;
        unsigned int pCaenCount;
        unsigned int pPMTCount;
        unsigned int fEventOrder;

        unsigned int fMTCDataId;
        unsigned int fCaenDataId;
        unsigned int fPMTDataId;
        unsigned int fRunDataId;
};

void* orca_listener(void* arg);

#endif
