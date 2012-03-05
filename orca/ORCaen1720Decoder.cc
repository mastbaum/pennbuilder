// ORCaen1720Decoder.cc

#include "TROOT.h"
#include "ORCaen1720Decoder.hh"
#include "ORLogger.hh"

//returns number of samples
UInt_t ORCaen1720Decoder::TraceLength(UInt_t* record, UInt_t /*Channel*/) {
	UInt_t numChan = 0;
	UInt_t chanMask = ChannelMask(record);
	for (; chanMask; numChan++) chanMask &= chanMask - 1;

	return (EventSize(record) - 4) / numChan * 2; // 4 longs header, then 2 samples per long
}

//void ORCaen1720Decoder::CopyTrace(UInt_t* record, std::vector<UShort_t>* Waveform, UInt_t numSamples) {
void ORCaen1720Decoder::CopyTrace(UInt_t* record, UShort_t* Waveform, UInt_t numSamples) {
	UShort_t* sample = (UShort_t*) record;
	//iterative way, because of the comming packed2.5 mode
	for (unsigned int i = 0; i < numSamples; i++) {
		Waveform[i] = sample[i] & 0x0fff;
		//(*Waveform)[i] = sample[i] & 0x0fff;
	}
}

