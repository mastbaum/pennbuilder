////////////////////////////////////////////////////////////////////
/// \class RAT:InBuilderProducer
///
/// \brief Unpacker, to unpack the packed data format (CDAB) into the RAT DS
///          
/// \author Andy Mastbaum <mastbaum@hep.upenn.edu>
///
/// REVISION HISTORY:\n
///     08 July 2011: Andy Mastbaum - New producer for CDAB files, largely
///                   based on G.D.Orebi-Gann's UnpackEvent producer.
///
///  \detail  This class unpacks files from the packed data format
///           that is output by the event builder, and populates the
///           full RAT DS.  This can be run on either real data, or
///           on packed MC files.
///
////////////////////////////////////////////////////////////////////

#ifndef __RAT_InBuilderProducer__
#define __RAT_InBuilderProducer__

#include <string>
#include <stdint.h>

#include <RAT/Producer.hh>
#include <RAT/DS/EV.hh>
#include <RAT/DS/PMTUnCal.hh>
#include <RAT/DS/PackedEvent.hh>
#include <RAT/DS/Root.hh>
#include <RAT/DS/Run.hh>
#include <globals.hh>

#define NPMTS 19 * 16 * 32

class G4UIcmdWithAString;

namespace RAT {

    extern const int kNheaders;
    extern const int kNpmt;

    class InBuilderProducer : public Producer {
        public:
            InBuilderProducer();
            InBuilderProducer(ProcBlock *block);
            virtual ~InBuilderProducer() {}

            virtual bool Read(G4String path);
            virtual G4String GetCurrentValue(G4UIcommand * command);
            virtual void SetNewValue(G4UIcommand * command,G4String newValue);

            /** Event Builder structs */

            /// PMTBundle contains raw PMT data packed into 3 32-bit words (96 bits)
            typedef struct
            {
                uint32_t word[3];
            } PMTBundle;

            /// MTCData contains trigger information (192 bits)
            typedef struct
            {
                uint32_t word[6];
            } MTCData;

            /// CAENData contains digitized trigger sums for up to 8 channels (12.8k bits)
            typedef struct
            {
                uint32_t header[4];
                uint32_t data[8][55]; // v1720 packs data like so (2.5 samples/word)
            } CAENData;

            /// Event contains all data for a single SNO+ detector event
            typedef struct
            {
                PMTBundle pmt[NPMTS];
                MTCData mtc;
                CAENData caen;
                uint32_t gtid;
                struct timespec builder_arrival_time;
                uint32_t run_id;
                uint32_t subrun_id;
                uint32_t nhits;
                uint32_t evorder;
                uint64_t runmask;
                uint8_t pack_ver;
                uint8_t mcflag;
                uint8_t datatype;
                uint8_t clockstat;
            } Event; 

            /// Run-level header RHDR
            typedef struct
            {
                uint32_t type;
                uint32_t date;
                uint32_t time;
                uint32_t daq_ver;
                uint32_t calib_trial_id;
                uint32_t srcmask;
                uint32_t runmask;
                uint32_t cratemask;
                uint32_t first_event_id;
                uint32_t valid_event_id;
                uint32_t run_id;
            } RHDR;

            /// Header identifying each entry in a CDAB file
            typedef struct {
                uint32_t record_type;
                uint32_t size;
            } CDABHeader;

            /// Identifiers for CDAB record types
            typedef enum {
                EMPTY,
                DETECTOR_EVENT,
                RUN_HEADER,
                AV_STATUS_HEADER,
                MANIPULATOR_STATUS_HEADER,
                TRIG_BANK_HEADER,
                EPED_BANK_HEADER
            } RecordType;

        protected:
            void Init();
            void UnpackHeader(DS::EV *ev);
            void UnpackPMT(DS::PMTUnCal *pmt, int lcn);
            void UnPackRHDR(RHDR *rhdr, DS::Run* &run);
            //  void UnPackCAAC(DS::CAAC *branchCAAC, DS::Run* &run);
            //  void UnPackCAST(DS::CAST *branchCAST, DS::Run* &run);
            //  void UnPackTRIG(DS::TRIG *branchTRIG, DS::Root* &ds);
            //  void UnPackEPED(DS::EPED *branchEPED, DS::Root* &ds);
            bool CheckOrder(std::string id, int prev, int next, int header);

            void Die(std::string message, int flag_err, int info1, int info2, int info3=0, int return_code=1);

            int GetCCC(int lcn);
            int GetLCN(UInt_t icrate, UInt_t icard, UInt_t ichan);
            int GetCrate(int lcn);
            int GetChannel(int lcn);
            int GetCard(int lcn);
            int GetBits(int arg, int loc, int n); // returns n bits at location loc in arg
            UInt_t GetBits(UInt_t arg, int loc, int n); // returns n bits at location loc in arg
            ULong64_t GetBits(ULong64_t arg, int loc, int n); // returns n bits at location loc in arg
            int SetBits(int arg, int loc, int val); // sets bits from (least sig) location loc in arg
            int ClearBit(int arg, int newbit);
            int SetBit(int arg, int newbit);
            bool TestBit(int word, int bit);
            bool CheckLength(int arg, int length);

            std::string fFileName;
            G4UIcmdWithAString *fListenCmd;
            UInt_t fMTCInfo[6];
            UInt_t fWord[3];
            Int_t fNHits;
            UInt_t fEVOrder;
            char fClockStat10;

            Int_t fEventID;
            Int_t fEvRHDR;
            Int_t fEvTRIG;
            Int_t fEvEPED;
            bool fCheckRHDR;
            bool fCheckTRIG;
            bool fCheckEPED;
            UInt_t fRunID;
            ULong64_t fRunMask;
            std::vector<int> fPMTtype;
    };

} // namespace RAT

#endif
