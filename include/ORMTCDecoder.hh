// ORMTCDecoder.hh

#ifndef _ORMTCDecoder_hh_
#define _ORMTCDecoder_hh_

#include "ORVDataDecoder.hh"

class ORMTCDecoder : public ORVDataDecoder
{
  public:
    ORMTCDecoder() {}
    virtual ~ORMTCDecoder() {}

	//MTC is a SNO+ custom HW, data stream follows SNO conventions to make the EventBuilder happy
	//we use kLongForm for data record, and preserve record[0], but record[1] is DIFFERENT from the ORCA convention.
	//the reason is the geographic addressing SNO custom crates use, e.i. card adresses do NOT correspond to slots


    virtual inline UInt_t CrateOf(UInt_t* /*record*/)
      { return 0; }
    virtual inline UInt_t CardOf(UInt_t* /*record*/)
      { return 0; }

	virtual inline ULong64_t Cnt10Mhz(UInt_t* record)
		{ return (ULong64_t) (record[2] & 0x001fffff) << 32 | record[1]; }

	virtual inline ULong64_t Cnt50Mhz(UInt_t* record)
		{ return ((ULong64_t) record[3]) << 11 | (record[2] & 0xffe00000) >> 21; }

	virtual inline UInt_t GTId(UInt_t* record)
		{ return record[4] & 0x00ffffff; }

	virtual inline UInt_t GTMask(UInt_t* record)
		{ return ((record[4] >> 24) & 0xff) | ((record[5] & 0x0003ffff) << 8); }  

	virtual inline Bool_t MissTrg(UInt_t* record)
		{ return (record[5] & 0x00040000); }

	virtual inline UInt_t Vlt(UInt_t* record)
		{ return record[5] >> 19 & 0x01ff; }

	virtual inline UInt_t Slp(UInt_t* record)
		{ return record[5] >> 29 | ((record[6] & 0x7fUL) << 3 ); }

	virtual inline UInt_t Intg(UInt_t* record)
		{ return record[6] >> 7 & 0x3ffUL; }

        virtual inline UInt_t Wrd0(UInt_t* record)
                { return record[1]; }

        virtual inline UInt_t Wrd1(UInt_t* record)
                { return record[2]; }

        virtual inline UInt_t Wrd2(UInt_t* record)
                { return record[3]; }

        virtual inline UInt_t Wrd3(UInt_t* record)
                { return record[4]; }

        virtual inline UInt_t Wrd4(UInt_t* record)
                { return record[5]; }

        virtual inline UInt_t Wrd5(UInt_t* record)
                { return record[6]; }

    virtual std::string GetDataObjectPath() { return "ORMTCModel:MTC"; }
};

#endif
