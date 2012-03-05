////////////////////////////////////////////////////////////////////
// Last svn revision: $Id: PackedEvent.hh 227 2010-04-09 14:57:20Z orebi $
////////////////////////////////////////////////////////////////////
/// \class DS::PackedEvent
///
/// \brief  DS for the packed data format
///          
/// \author Gabriel Orebi Gann <orebi@hep.upenn.edu>
///
/// REVISION HISTORY:\n
///     09 Apr 2010 : Gabriel Orebi Gann - extend structure to
///                   hold header info, as well as detector events
///
///  \detail  This structure holds the packed data that comes out of
///  the event builder i.e. raw data in packed binary format.  The
///  top level class, ``PackedRec'', holds the record type and
///  a ``GenericRec'' record, which can be a detector event 
///  (``PackedEvent'') or header information (RHDR, CAST, CAAC, TRIG,
///  EPED).
///
///
///
///
////////////////////////////////////////////////////////////////////

#ifndef __RAT_DS_PackedEvent__
#define __RAT_DS_PackedEvent__

#include <vector>
#include <string>
#include <TObject.h>

namespace RAT {
  namespace DS {
  using namespace std;

// Generic record type
class GenericRec : public TObject   {
  public:
    
  // ROOT junk
  ClassDef(GenericRec,2)
};

enum ERecordType {
    kRecEmpty,
    kRecPMT,
    kRecRHDR,
    kRecCAAC,
    kRecCAST,
    kRecTRIG,
    kRecEPED
};

// Top level class, holding record type, and the record itself
class PackedRec : public TObject {
  public:
    PackedRec() : TObject(){ Init(); };
    //PackedRec(const PackedRec &rhs) : TObject() { Init(); CopyObj(rhs); };
    virtual ~PackedRec() {};
    //virtual PackedRec &operator=(const PackedRec &rhs) { CopyObj(rhs); return *this; };
    // Record types:
    // 0 = empty
    // 1 = detector event
    // 2 = RHDR
    // 3 = CAAC
    // 4 = CAST
    // 5 = TRIG
    // 6 = EPED
    UInt_t RecordType;
    GenericRec *Rec;

  // ROOT junk
  ClassDef(PackedRec,2)

  protected:
  
    virtual void Init() {
      RecordType = 0;
      Rec = 0;
    }
  
    //virtual void CopyObj(const PackedRec &rhs) {
    //  RecordType = rhs.RecordType;
    //}

};


// PMT bundle, 3 words per pmt hit
class PMTBundle { 
  public:
	PMTBundle(): Word(3) {}
	PMTBundle(const std::vector<UInt_t> &aBundle): Word(aBundle) {}
	virtual ~PMTBundle() {}
	virtual PMTBundle &operator=(const PMTBundle &rhs) { CopyObj(rhs); return *this; }

	std::vector<UInt_t> Word; 

  protected:
	virtual void CopyObj(const PMTBundle &rhs) {
		Word = rhs.Word;
	}

	ClassDef(PMTBundle,2)
};


// CAEN
#define kNCaenChan	8
class CaenTrace {
  public:
	CaenTrace(): Waveform(0) {}
	virtual ~CaenTrace() {}
	virtual CaenTrace &operator=(const CaenTrace &rhs) { CopyObj(rhs); return *this; }

	std::vector<UShort_t> Waveform;

  protected:
	virtual void CopyObj(const CaenTrace &rhs) {
		Waveform = rhs.Waveform;
	}

	ClassDef(CaenTrace, 2)
};

class CaenBundle { 
  public:
	CaenBundle(): ChannelMask(0), Pattern(0), EventCount(0), Clock(0), Trace(8) {}
	virtual ~CaenBundle() {}
	virtual CaenBundle &operator=(const CaenBundle &rhs) { CopyObj(rhs); return *this; }

	UInt_t ChannelMask;
	UInt_t Pattern;
	UInt_t EventCount;
	UInt_t Clock;
	std::vector<CaenTrace> Trace;
 
  protected:
	virtual void CopyObj(const CaenBundle &rhs) {
		ChannelMask = rhs.ChannelMask;
		Pattern = rhs.Pattern;
		EventCount = rhs.EventCount;
		Clock = rhs.Clock;
		Trace = rhs.Trace;
	}

	ClassDef(CaenBundle,2)
};

const int kNheaders = 6;

// Detector event class, inheriting from the generic record base class
class PackedEvent : public GenericRec {
protected:
  virtual void Init() {
    for(int i=0;i<kNheaders;++i){
      MTCInfo[i] = 0;
    }
    RunID = 0;
    SubRunID = 0;
    NHits = 0;
    EVOrder = 0;
    RunMask = 0;
    PackVer = 0;
    MCFlag = 0;
    DataType = 0;
    ClockStat10 = 0;
  }

public:
  PackedEvent() : GenericRec(), PMTBundles(), Caen() { Init(); };
  PackedEvent(const PackedEvent &rhs) : GenericRec() { Init(); CopyObj(rhs); };
  virtual ~PackedEvent() {};
  virtual PackedEvent &operator=(const PackedEvent &rhs) { CopyObj(rhs); return *this; };

  // 6 words for the event header from the MTC
  UInt_t MTCInfo[kNheaders];
  
  // Other event info to store
  UInt_t RunID;
  UInt_t SubRunID;
  UInt_t NHits;
  UInt_t EVOrder;
  ULong64_t RunMask;
  char PackVer;
  char MCFlag;
  char DataType;
  char ClockStat10;
    
  // Vector of PMT bundles
  std::vector<PMTBundle> PMTBundles;
  CaenBundle Caen;

protected:
  
  
  virtual void CopyObj(const PackedEvent &rhs) {
    for(int i=0;i<kNheaders;++i){
      MTCInfo[i] = rhs.MTCInfo[i];
    }
    RunID = rhs.RunID;
    SubRunID = rhs.SubRunID;
    NHits = rhs.NHits;
    EVOrder = rhs.EVOrder;
    RunMask = rhs.RunMask;
    PackVer = rhs.PackVer;
    MCFlag = rhs.MCFlag;
    DataType = rhs.DataType;
    ClockStat10 = rhs.ClockStat10;
    PMTBundles = rhs.PMTBundles;
    Caen = rhs.Caen;
  }

  // ROOT junk
  ClassDef(PackedEvent,2)

};


// Various ``header info'' classes, each inheriting from the generic record base class
// -------------------------------

class EPED : public GenericRec {
public:
  EPED() : GenericRec() { Init(); };
  EPED(const EPED &rhs) : GenericRec() { Init(); CopyObj(rhs); };
  virtual ~EPED() {};
  virtual EPED &operator=(const EPED &rhs) { CopyObj(rhs); return *this; };

  // Members
  UInt_t GTDelayCoarse;
  UInt_t GTDelayFine;
  UInt_t QPedAmp;
  UInt_t QPedWidth;
  UInt_t PatternID;
  UInt_t CalType;
  UInt_t EventID;  // GTID of first events in this bank's validity
  UInt_t RunID;    // Double-check on the run

protected:
  
  virtual void Init() {
    GTDelayCoarse = 0;
    GTDelayFine = 0;
    QPedAmp = 0;
    QPedWidth = 0;
    PatternID = 0;
    CalType = 0;
    EventID = 0;
    RunID = 0;
  }
  
  virtual void CopyObj(const EPED &rhs) {
    GTDelayCoarse = rhs.GTDelayCoarse;
    GTDelayFine = rhs.GTDelayFine;
    QPedAmp = rhs.QPedAmp;
    QPedWidth = rhs.QPedWidth;
    PatternID = rhs.PatternID;
    CalType = rhs.CalType;
    EventID = rhs.EventID;
    RunID = rhs.RunID;
  }

  // ROOT junk
  ClassDef(EPED,2)
};

class TRIG : public GenericRec {
public:
  TRIG() : GenericRec() { Init(); };
  TRIG(const TRIG &rhs) : GenericRec() { Init(); CopyObj(rhs); };
  virtual ~TRIG() {};
  virtual TRIG &operator=(const TRIG &rhs) { CopyObj(rhs); return *this; };

  // Members
  // Arrays correspond to:
  // N100Lo, N100Med, N100Hi, N20, N20LB, ESUMLo, ESUMHi, OWLn, OWLELo, OWLEHi
  UInt_t TrigMask;
  UShort_t Threshold[10];
  UShort_t TrigZeroOffset[10];
  UInt_t PulserRate;
  UInt_t MTC_CSR;
  UInt_t LockoutWidth;
  UInt_t PrescaleFreq;
  UInt_t EventID;  // GTID of first events in this bank's validity
  UInt_t RunID;    // Double-check on the run

protected:
  
  virtual void Init() {
    for(int i=0;i<10;++i){
      Threshold[i] = 0;
      TrigZeroOffset[i] = 0;
    }
    TrigMask = 0;
    PulserRate = 0;
    MTC_CSR = 0;
    LockoutWidth = 0;
    PrescaleFreq = 0;
    EventID = 0;
    RunID = 0;
  }
  
  virtual void CopyObj(const TRIG &rhs) {
    for(int i=0;i<10;++i){
      Threshold[i] = rhs.Threshold[i];
      TrigZeroOffset[i] = rhs.TrigZeroOffset[i];
    }
    TrigMask = rhs.TrigMask;
    PulserRate = rhs.PulserRate;
    MTC_CSR = rhs.MTC_CSR;
    LockoutWidth = rhs.LockoutWidth;
    PrescaleFreq = rhs.PrescaleFreq;
    EventID = rhs.EventID;
    RunID = rhs.RunID;
  }

  // ROOT junk
  ClassDef(TRIG,2)
};


class RHDR : public GenericRec {
public:
  RHDR() : GenericRec() { Init(); };
  RHDR(const RHDR &rhs) : GenericRec() { Init(); CopyObj(rhs); };
  virtual ~RHDR() {};
  virtual RHDR &operator=(const RHDR &rhs) { CopyObj(rhs); return *this; };

  // Members
  UInt_t Date;
  UInt_t Time;
  char DAQVer;
  UInt_t CalibTrialID;
  UInt_t SrcMask;
  UInt_t RunMask;
  UInt_t CrateMask;
  UInt_t FirstEventID;
  UInt_t ValidEventID;
  UInt_t RunID;    // Double-check on the run
  

protected:
  
  virtual void Init() {
    Date = 0;
    Time = 0;
    DAQVer = 0;
    CalibTrialID = 0;
    SrcMask = 0;
    RunMask = 0;
    CrateMask = 0;
    FirstEventID = 0;
    ValidEventID = 0;
    RunID = 0;    // Double-check on the run
  }
  
  virtual void CopyObj(const RHDR &rhs) {
    Date = rhs.Date;
    Time = rhs.Time;
    DAQVer = rhs.DAQVer;
    CalibTrialID = rhs.CalibTrialID;
    SrcMask = rhs.SrcMask;
    RunMask = rhs.RunMask;
    CrateMask = rhs.CrateMask;
    FirstEventID = rhs.FirstEventID;
    ValidEventID = rhs.ValidEventID;
    RunID = rhs.RunID;    // Double-check on the run
  }

  // ROOT junk
  ClassDef(RHDR,2)

};

class CAST : public GenericRec {
public:
  CAST() : GenericRec() { Init(); };
  CAST(const CAST &rhs) : GenericRec() { Init(); CopyObj(rhs); };
  virtual ~CAST() {};
  virtual CAST &operator=(const CAST &rhs) { CopyObj(rhs); return *this; };

  // Members
  UShort_t SourceID;
  UShort_t SourceStat;
  UShort_t NRopes;
  float ManipPos[3];
  float ManipDest[3];
  float SrcPosUncert1;
  float SrcPosUncert2[3];
  float LBallOrient;
  std::vector<int> RopeID;
  std::vector<float> RopeLen;
  std::vector<float> RopeTargLen;
  std::vector<float> RopeVel;
  std::vector<float> RopeTens;
  std::vector<float> RopeErr;

protected:
  
  virtual void Init() {
    for(int i=0;i<3;++i){
      ManipPos[i] = 0.;
      ManipDest[i] = 0.;
      SrcPosUncert2[i] = 0.;
    }
    SourceID = 0;
    SourceStat = 0;
    NRopes = 0;
    SrcPosUncert1 = 0.;
    LBallOrient = 0.;
    RopeID.resize(0);
    RopeLen.resize(0);
    RopeTargLen.resize(0);
    RopeVel.resize(0);
    RopeTens.resize(0);
    RopeErr.resize(0);
  }
  
  virtual void CopyObj(const CAST &rhs) {
    for(int i=0;i<3;++i){
      ManipPos[i] = rhs.ManipPos[i];
      ManipDest[i] = rhs.ManipDest[i];
      SrcPosUncert2[i] = rhs.SrcPosUncert2[i];
    }
    SourceID = rhs.SourceID;
    SourceStat = rhs.SourceStat;
    NRopes = rhs.NRopes;
    SrcPosUncert1 = rhs.SrcPosUncert1;
    LBallOrient = rhs.LBallOrient;
    RopeID = rhs.RopeID;
    RopeLen = rhs.RopeLen;
    RopeTargLen = rhs.RopeTargLen;
    RopeVel = rhs.RopeVel;
    RopeTens = rhs.RopeTens;
    RopeErr = rhs.RopeErr;
  }

  // ROOT junk
  ClassDef(CAST,2)

};

class CAAC : public GenericRec {
public:
  CAAC() : GenericRec() { Init(); };
  CAAC(const CAAC &rhs) : GenericRec() { Init(); CopyObj(rhs); };
  virtual ~CAAC() {};
  virtual CAAC &operator=(const CAAC &rhs) { CopyObj(rhs); return *this; };

  // Members
  float AVPos[3];
  float AVRoll[3];  // roll, pitch and yaw
  float AVRopeLength[7];

protected:
  
  virtual void Init() {
    for(int i=0;i<3;++i){
      AVPos[i] = 0.;
      AVRoll[i] = 0.;
    }
    for(int i=0;i<7;++i){
      AVRopeLength[i] = 0.;
    }
  }
  
  virtual void CopyObj(const CAAC &rhs) {
    for(int i=0;i<3;++i){
      AVPos[i] = rhs.AVPos[i];
      AVRoll[i] = rhs.AVRoll[i];
    }
    for(int i=0;i<7;++i){
      AVRopeLength[i] = rhs.AVRopeLength[i];
    }
  }


  // ROOT junk
  ClassDef(CAAC,2)
};
  } // namespace DS
} // namespace RAT

#endif

