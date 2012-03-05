// ORPMTDecoder.hh

#ifndef _ORPMTDecoder_hh_
#define _ORPMTDecoder_hh_

#include "ORVDataDecoder.hh"

class ORPMTDecoder : public ORVDataDecoder
{
  public:
    ORPMTDecoder();
    virtual ~ORPMTDecoder() {}

	//FEC are SNO+ custom HW, data stream makes EventBuilder happy
	//we BREAK ORCA conventions, a modified kLongForm is used,
	//record[0] kept, record[1] used for additional XL3 prms
	//XL3 megabundle is variable in length
	//extraction works per PMT bundle, so call it with the proper offset

	virtual inline UInt_t CrateOf(UInt_t* /*record*/)
		{ return 0; }
	virtual inline UInt_t CardOf(UInt_t* /*record*/)
		{ return 0; }

	virtual inline UInt_t GTId(UInt_t* record)
		{ return (record[0] & 0x0000ffff) | ((record[2] << 4) & 0x000f0000) | ((record[2] >> 8) & 0x00f00000); }

	virtual inline UChar_t Crate(UInt_t* record)
		{ return (record[0] >> 21) & 0x1fUL; }

	virtual inline UChar_t Card(UInt_t* record)
		{ return (record[0] >> 26) & 0x0fUL; }

	virtual inline UChar_t Channel(UInt_t* record)
		{ return (record[0] >> 16) & 0x1fUL; }

	virtual inline UChar_t Cell(UInt_t* record)
		{ return (record[1] >> 12) & 0x0fUL; }

	virtual inline UShort_t QHL(UInt_t* record)
		{ return (record[2] & 0x0fffUL) ^ 0x0800UL; }

	virtual inline UShort_t QHS(UInt_t* record)
		{ return ((record[1] >> 16) & 0x0fffUL) ^ 0x0800UL; }

	virtual inline UShort_t QLX(UInt_t* record)
		{ return (record[1] & 0x0fffUL) ^ 0x0800UL; }

	virtual inline UShort_t TAC(UInt_t* record)
		{ return ((record[2] >> 16) & 0x0fffUL) ^ 0x0800UL; }

	virtual inline Bool_t CGT16(UInt_t* record)
		{ return ((record[0] >> 30) & 0x1UL); }

	virtual inline Bool_t CGT24(UInt_t* record)
		{ return ((record[0] >> 31) & 0x1UL); }

	virtual inline Bool_t ES16(UInt_t* record)
		{ return ((record[1] >> 31) & 0x1UL); }

	virtual inline Bool_t Missed(UInt_t* record)
		{ return ((record[1] >> 28) & 0x1UL); }

	virtual inline Bool_t NC(UInt_t* record)
		{ return ((record[1] >> 29) & 0x1UL); }

	virtual inline Bool_t LGI(UInt_t* record)
		{ return ((record[1] >> 30) & 0x1UL); }

	virtual inline UInt_t Wrd0(UInt_t* record)
		{ return record[0]; }

	virtual inline UInt_t Wrd1(UInt_t* record)
		{ return record[1]; }

	virtual inline UInt_t Wrd2(UInt_t* record)
		{ return record[2]; }

	virtual std::string GetDataObjectPath()
		{ return "ORXL3Model:Xl3MegaBundle"; }

	virtual void Swap(UInt_t* dataRecord);

  private:
	bool fMustSwap;
};

#endif

