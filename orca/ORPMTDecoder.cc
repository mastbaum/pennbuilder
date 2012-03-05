// ORPMTDecoder.cc

#include "TROOT.h"
#include "ORPMTDecoder.hh"
#include "ORLogger.hh"
#include <netinet/in.h>

ORPMTDecoder::ORPMTDecoder() {

	if (0x0000ABCD == htonl(0x0000ABCD)) fMustSwap = kFALSE;
	else fMustSwap = kTRUE;

}


void ORPMTDecoder::Swap(UInt_t* dataRecord)
{
	if (fMustSwap) {
		for(size_t i=1; i<LengthOf(dataRecord);i++) {
		ORUtils::Swap(dataRecord[i]);
		}
	}
}


