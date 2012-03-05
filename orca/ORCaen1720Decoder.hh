// ORCaen1720Decoder.hh

#ifndef _ORCaen1720Decoder_hh_
#define _ORCaen1720Decoder_hh_

#include "ORVDataDecoder.hh"

class ORCaen1720Decoder : public ORVDataDecoder
{
  public:
    ORCaen1720Decoder() {}
    virtual ~ORCaen1720Decoder() {}

	virtual inline UInt_t CrateOf(UInt_t* /*record*/)
		{ return 0; }

	virtual inline UInt_t CardOf(UInt_t* /*record*/)
		{ return 0; }

	virtual inline UInt_t EventSize(UInt_t* record)
		{ return record[2] & 0x0fffffff; }

	virtual inline UInt_t ChannelMask(UInt_t* record)
		{ return record[3] & 0xff; } 	

	virtual inline UInt_t Pattern(UInt_t* record)
		{ return (record[3] >> 8) & 0xffff; }

	virtual inline UInt_t EventCount(UInt_t* record)
		{ return record[4] & 0xffffff; }

	virtual inline UInt_t Clock(UInt_t* record)
		{ return record[5]; }
	
	//get ready for packed2.5 and zero suppression
	virtual UInt_t TraceLength(UInt_t* record, UInt_t /*Channel*/);

	//virtual void CopyTrace(UInt_t* record, std::vector<UShort_t>* Waveform, UInt_t numSamples);
	virtual void CopyTrace(UInt_t* record, UShort_t* Waveform, UInt_t numSamples);

        virtual std::string GetDataObjectPath()
                { return "ORCaen1720Model:CAEN"; }

};

#endif

