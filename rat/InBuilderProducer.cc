#include <RAT/InBuilderProducer.hh>
#include <RAT/ProcBlock.hh>
#include <RAT/DS/Root.hh>
#include <RAT/DS/Run.hh>
#include <RAT/DS/EV.hh>
#include <RAT/DS/RunStore.hh>
#include <RAT/SignalHandler.hh>
#include <RAT/Log.hh>
#include <RAT/DB.hh>
#include <RAT/BitManip.hh>

#include <G4UIdirectory.hh>
#include <G4UIcmdWithAString.hh>

#include <TTree.h>

#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/un.h>
#include <stdio.h>

namespace RAT {

    InBuilderProducer::InBuilderProducer()
    {
        fMainBlock = 0;
        Init();
    }

    InBuilderProducer::InBuilderProducer(ProcBlock *block)
    {
        SetMainBlock(block);
        Init();
    }

    void InBuilderProducer::Init()
    {
        // Build commands
        G4UIdirectory* DebugDir = new G4UIdirectory("/rat/inbuilder/");
        DebugDir->SetGuidance("Read in CDAB files to populate the RAT data structure");

        // info message command
        fListenCmd = new G4UIcmdWithAString("/rat/inbuilder/read", this);
        fListenCmd->SetGuidance("Filename from which to read");
        fListenCmd->SetParameterName("path", false);  // required

        DBLinkPtr PMTBank= DB::Get()->GetLink("PMTINFO");
        fPMTtype = PMTBank->GetIArray("type");

        fEventID = -1;
        fRunID = 0;
        fRunMask = 0;
        fEvRHDR = -1;
        fCheckRHDR = false;
        fEvTRIG = -1;
        fCheckTRIG = false;
        fEvEPED = -1;
        fCheckEPED = false;
    }

    G4String InBuilderProducer::GetCurrentValue(G4UIcommand* /*command*/)
    {
        Log::Die("invalid inbuilder \"get\" command");
    }

    void InBuilderProducer::SetNewValue(G4UIcommand* command, G4String newValue)
    {
        if (command == fListenCmd) {
            if (!fMainBlock)
                Log::Die("inbuilder: No main block declared! (should never happen)");
            else if (!Read(newValue))
                Log::Die(dformat("inbuilder: Error listening on path %d ", newValue.c_str()));
        } else
            Log::Die("invalid inbuilder \"set\" command");
    }

    bool InBuilderProducer::Read(G4String path)
    {
        FILE* cdab_in = fopen(path.c_str(),"rb");
        Log::Assert(cdab_in, dformat("InBuilderProducer: Unable to open cdab file %s", path.c_str()));

        DS::Run *run = new DS::Run();
        DS::RunStore::AddNewRun(run);
        bool rundata_set = false;
        RAT::DS::Root *ds = NULL;
        uint64_t eventcount = 0;

        uint32_t current_gtid = 0;
        while(!SignalHandler::IsTermRequested() && current_gtid < 2000) {
            CDABHeader chdr;
            fread(&chdr, sizeof(CDABHeader), 1, cdab_in);
            debug << "InBuilderProducer: Reading CDAB header:\n";
            debug << " type = " << chdr.record_type << "\n";
            debug << " size = " << chdr.size << "\n";

            if(chdr.record_type == (int) RUN_HEADER) {
                debug << "InBuilderProducer: Parsing RHDR\n";
                RHDR* rhdr = (RHDR*) malloc(sizeof(RHDR));
                fread(rhdr, chdr.size, 1, cdab_in);
                UnPackRHDR(rhdr, run);
            }
            else if(chdr.record_type == (int) DETECTOR_EVENT) {
                debug << "InBuilderProducer: Parsing DETECTOR_EVENT\n";
                Event* event = (Event*) malloc(sizeof(Event));
                fread(event, chdr.size, 1, cdab_in);
                if(ds == NULL)
                    ds = new RAT::DS::Root();
                if(!rundata_set) {
                    run->SetRunID(event->run_id);
                    run->SetSubRunID(event->subrun_id);
                    run->SetRunType(event->runmask);
                    run->SetMCFlag(event->mcflag);
                    run->SetPackVer(event->pack_ver);
                    run->SetDataType(event->datatype);
                    fRunID = event->run_id;
                    fRunMask = event->runmask;
                    rundata_set = true;
                }

                current_gtid = event->gtid;

                // Set root/ds level info
                ds->SetRunID(event->run_id);
                ds->SetSubRunID(event->subrun_id);

                // Get the event level info
                fEVOrder = event->evorder;
                //      if(fEVOrder != eventcount)
                //        InBuilderProducer::Die("",1,eventcount,fEVOrder);
                fNHits = event->nhits;
                //fClockStat10 = event->clock_stat_10; //?
                for(int ih=0; ih<6; ih++)
                    fMTCInfo[ih] = event->mtc.word[ih];

                // Create a new EV object, and fill with unpacked info
                DS::EV *ev = ds->AddNewEV();

                InBuilderProducer::UnpackHeader(ev);

                for(int inh = 0; inh<NPMTS; inh++) {
                    // Get the next PMTBundle in line, and its 3 words
                    PMTBundle bundle = event->pmt[inh];
                    for(int ip=0; ip<3; ip++) {
                        fWord[ip] = bundle.word[ip];
                    }

                    // kludge?, no data for pmt
                    if(!fWord[0])
                        continue;

                    UInt_t ichan = InBuilderProducer::GetBits(fWord[0], 16, 5);
                    UInt_t icard = InBuilderProducer::GetBits(fWord[0], 26, 4);
                    UInt_t icrate = InBuilderProducer::GetBits(fWord[0], 21, 5);
                    UInt_t lcn = InBuilderProducer::GetLCN(icrate, icard, ichan);
                    int type = fPMTtype[lcn];

                    // Create a new PMTUnCal, and fill it with unpacked info
                    DS::PMTUnCal *pmt = ev->AddNewPMTUnCal(type);

                    InBuilderProducer::UnpackPMT(pmt, lcn);
                }
                free(event);

                // Nhits should be set automatically by the addition of PMTUnCals
                if(fNHits != ev->GetNhits())
                    warn << "InBuilderProducer: NHIT != hit PMT count\n";

                fMainBlock->DSEvent(ds);
                delete ds;
                ds = NULL;
                eventcount++;
            }
            else if(chdr.record_type == AV_STATUS_HEADER) {
                debug << "InBuilderProducer: Parsing AV_STATUS_HEADER\n";
                //DS::CAAC *branchCAAC = dynamic_cast<DS::CAAC *>(branchRec->Rec);
                //InBuilderProducer::UnPackCAAC(branchCAAC, run);
            }
            else if(chdr.record_type == MANIPULATOR_STATUS_HEADER) {
                debug << "InBuilderProducer: Parsing MANIPULATOR_STATUS_HEADER\n";
                //DS::CAST *branchCAST = dynamic_cast<DS::CAST *>(branchRec->Rec);
                //InBuilderProducer::UnPackCAST(branchCAST, run);
            }
            else if(chdr.record_type == TRIG_BANK_HEADER) {
                debug << "InBuilderProducer: Parsing TRIG_BANK_HEADER\n";
                //DS::TRIG *branchTRIG = dynamic_cast<DS::TRIG *>(branchRec->Rec);
                //if(ds==0)ds = new RAT::DS::Root();
                //InBuilderProducer::UnPackTRIG(branchTRIG, ds);
            }
            else if(chdr.record_type == EPED_BANK_HEADER) {
                debug << "InBuilderProducer: Parsing EPED_BANK_HEADER\n";
                //DS::EPED *branchEPED = dynamic_cast<DS::EPED *>(branchRec->Rec);
                //if(ds==0)ds = new RAT::DS::Root();
                //InBuilderProducer::UnPackEPED(branchEPED, ds);
            }
            else {
                Log::Die(dformat("InBuilderProducer: Encountered record of unknown type %i",chdr.record_type));
            }
        }
        return true;
    }

    bool InBuilderProducer::CheckOrder(std::string id, int prev, int next, int header)
    {

        // The GTID in the header should equal the GTID of the next event
        //                           and
        // IF the GTID did not wrap (which it does at 24 bits)
        //                               be > the prev event
        // Otherwise
        //                               be < the prev event

        bool ok = true;

        // Check wrap:
        bool wrap = true;
        if(next>prev)wrap = false;

        if(header!=next){ok = false;}
        if(wrap && (header>=prev)){ok = false;}
        if(!wrap && (header<=prev)){ok = false;}
        if(!ok)InBuilderProducer::Die(id, 5, prev, next, header);
        return true;

    }

    void InBuilderProducer::UnPackRHDR(RHDR* rhdr, DS::Run* &run)
    {
        //  if(rhdr->run_id != fRunID)
        //    InBuilderProducer::Die("RunID, RHDR",4,rhdr->run_id,fRunID);
        //  if(rhdr->runmask != fRunMask)
        //    InBuilderProducer::Die("RunMask, RHDR",4,rhdr->runmask,fRunMask);
        run->SetDate(rhdr->date);
        run->SetTime(rhdr->time);
        run->SetDAQVer(rhdr->daq_ver);
        run->SetCalibTrialID(rhdr->calib_trial_id);
        run->SetSrcMask(rhdr->srcmask);
        run->SetRunType(rhdr->runmask);
        run->SetCrateMask(rhdr->cratemask);
        run->SetFirstEventID(rhdr->first_event_id);
        run->SetValidEventID(rhdr->valid_event_id);
        run->SetRunID(rhdr->run_id);
        fEvRHDR = rhdr->first_event_id;
        fCheckRHDR = true;
    }

    void InBuilderProducer::UnpackHeader(DS::EV *ev)
    {
        // 6 MTCInfo words + clockstat10
        ULong64_t clockCount50;
        ULong64_t clockCount10;
        UInt_t trigError;
        UInt_t trigType;
        UInt_t eventID;

        ULong64_t clock10part1 = fMTCInfo[0];
        ULong64_t clock10part2 = InBuilderProducer::GetBits(fMTCInfo[1],0,21);
        clockCount10 = (clock10part2 << 32) + clock10part1;

        ULong64_t clock50part1 = InBuilderProducer::GetBits(fMTCInfo[1],21,11);
        ULong64_t clock50part2 = fMTCInfo[2];
        clockCount50 = (clock50part2 << 11) + clock50part1;

        eventID = InBuilderProducer::GetBits(fMTCInfo[3],0,24);

        UInt_t trigpart1 = InBuilderProducer::GetBits(fMTCInfo[3],24,8);
        UInt_t trigpart2 = InBuilderProducer::GetBits(fMTCInfo[4],0,19);
        trigType = (trigpart2 << 8) + trigpart1;

        trigError = InBuilderProducer::GetBits(fMTCInfo[5],17,15);

        // CHECK record ordering: we have the previous/current event's GTID (fEventID/eventID)
        bool ok = true;
        if(fCheckRHDR){
            ok = InBuilderProducer::CheckOrder("Run header",fEventID,eventID,fEvRHDR);
            fCheckRHDR = false;
        }
        if(!ok)InBuilderProducer::Die("Run header",6,0,0);
        if(fCheckTRIG){
            ok = InBuilderProducer::CheckOrder("TRIGInfo", fEventID,eventID,fEvTRIG);
            fCheckTRIG = false;
        }
        if(!ok)InBuilderProducer::Die("TRIGInfo",6,0,0);
        if(fCheckEPED){
            ok = InBuilderProducer::CheckOrder("EPEDInfo", fEventID,eventID,fEvEPED);
            fCheckEPED = false;
        }
        if(!ok)InBuilderProducer::Die("EPEDInfo",6,0,0);

        // Set eventID check value  
        fEventID = eventID;

        //ev->SetClockStat10(fClockStat10);
        ev->SetTrigError(trigError);
        ev->SetTrigType(trigType);
        ev->SetEventID(eventID);
        ev->SetClockCount50(clockCount50);
        ev->SetClockCount10(clockCount10);


        // Set UT from 10MHz clock counts
        ULong64_t Period = 100;  // 10MHz period in ns
        ULong64_t Total = clockCount10 * Period;
        ULong64_t NNsec = Total%(ULong64_t)1e9;
        ULong64_t NSecs = Total/1e9;
        ULong64_t NDays = NSecs/86400;
        NSecs = NSecs - (86400*NDays);

        UInt_t ndays = (UInt_t)NDays;
        UInt_t nsecs = (UInt_t)NSecs;
        UInt_t nns = (UInt_t)NNsec;

        if(Total != (ULong64_t)NNsec + (ULong64_t)(1e9*NSecs) + (ULong64_t)(86400*1e9*NDays)){
            //printf("\033[31m Total of %55u != %32u ns, %32u s, %32u d \033[m\n",Total, NNsec, NSecs, NDays);
        }

        ev->SetUTDays(ndays);
        ev->SetUTSecs(nsecs);
        ev->SetUTNSecs(nns);

    }

    void InBuilderProducer::UnpackPMT(DS::PMTUnCal *pmt, int lcn)
    {
        // 3 pmt words
        UInt_t cell = InBuilderProducer::GetBits(fWord[1], 12, 4);

        char chanflags = 0;
        if(InBuilderProducer::TestBit(fWord[0],30))chanflags = InBuilderProducer::SetBit(chanflags,0);
        if(InBuilderProducer::TestBit(fWord[0],31))chanflags = InBuilderProducer::SetBit(chanflags,1);
        if(InBuilderProducer::TestBit(fWord[1],28))chanflags = InBuilderProducer::SetBit(chanflags,2);
        if(InBuilderProducer::TestBit(fWord[1],29))chanflags = InBuilderProducer::SetBit(chanflags,3);
        if(InBuilderProducer::TestBit(fWord[1],30))chanflags = InBuilderProducer::SetBit(chanflags,4);
        if(InBuilderProducer::TestBit(fWord[1],31))chanflags = InBuilderProducer::SetBit(chanflags,5);

        UShort_t qhs = InBuilderProducer::GetBits(fWord[1], 16, 12);
        UShort_t qhl = InBuilderProducer::GetBits(fWord[2], 0, 12);
        UShort_t qlx = InBuilderProducer::GetBits(fWord[1], 0, 12);
        UShort_t tac = InBuilderProducer::GetBits(fWord[2], 16, 12);

        // Flip final bit of Q,T (to fix the fact that the ADCs are just plain weird)
        BitManip bits;
        qhs = bits.FlipBit(qhs,11);
        qhl = bits.FlipBit(qhl,11);
        qlx = bits.FlipBit(qlx,11);
        tac = bits.FlipBit(tac,11);

        // EventID is bits 0-15 of word 1, 12-15 and 28-31 of word3
        Int_t evID = InBuilderProducer::GetBits(fWord[0], 0, 16);
        Int_t evID1 = InBuilderProducer::GetBits(fWord[2], 12, 4);
        Int_t evID2 = InBuilderProducer::GetBits(fWord[2], 28, 4);
        evID = evID + (evID1<<16) + (evID2<<20);
        //if(evID!=fEventID)InBuilderProducer::Die("EventID, PMT",4,evID,fEventID);

        pmt->SetID(lcn);
        pmt->SetCellID(cell);
        pmt->SetChanFlags(chanflags);
        pmt->SetsQHS(qhs);
        pmt->SetsQHL(qhl);
        pmt->SetsQLX(qlx);
        pmt->SetsPMTt(tac);
    }


    int InBuilderProducer::GetLCN(UInt_t icrate, UInt_t icard, UInt_t ichan)
    {
        int lcn = 512*icrate + 32*icard + ichan;
        return lcn;
    }

    int InBuilderProducer::GetCCC(int lcn)
    {
        int icrate = InBuilderProducer::GetBits(lcn, 9, 5);
        int icard = InBuilderProducer::GetBits(lcn, 5, 4);
        int ichan = InBuilderProducer::GetBits(lcn, 0, 5);
        int test = 512*icrate + 32*icard + ichan;
        if(test!=lcn){
            G4cout<<"ERROR IN CALCULATION OF CCC ID #"<<G4endl;
        }
        int ccc     = 1024*icard + 32*icrate + ichan;
        return ccc;
    }

    int InBuilderProducer::GetCard(int lcn)
    {
        int icrate = InBuilderProducer::GetBits(lcn, 9, 5);
        int icard = InBuilderProducer::GetBits(lcn, 5, 4);
        int ichan = InBuilderProducer::GetBits(lcn, 0, 5);
        int test = 512*icrate + 32*icard + ichan;
        if(test!=lcn){
            G4cout<<"ERROR IN CALCULATION OF CCC ID #"<<G4endl;
        }
        //int ccc     = 1024*icard + 32*icrate + ichan;
        return icard;
    }

    int InBuilderProducer::GetCrate(int lcn)
    {
        int icrate = InBuilderProducer::GetBits(lcn, 9, 5);
        int icard = InBuilderProducer::GetBits(lcn, 5, 4);
        int ichan = InBuilderProducer::GetBits(lcn, 0, 5);
        int test = 512*icrate + 32*icard + ichan;
        if(test!=lcn){
            G4cout<<"ERROR IN CALCULATION OF CCC ID #"<<G4endl;
        }
        //int ccc     = 1024*icard + 32*icrate + ichan;
        return icrate;
    }

    int InBuilderProducer::GetChannel(int lcn)
    {
        int icrate = InBuilderProducer::GetBits(lcn, 9, 5);
        int icard = InBuilderProducer::GetBits(lcn, 5, 4);
        int ichan = InBuilderProducer::GetBits(lcn, 0, 5);
        int test = 512*icrate + 32*icard + ichan;
        if(test!=lcn){
            G4cout<<"ERROR IN CALCULATION OF CCC ID #"<<G4endl;
        }
        //int ccc     = 1024*icard + 32*icrate + ichan;
        return ichan;
    }


    // Sets the bit at `newbit' in `arg' to 0
    int InBuilderProducer::ClearBit(int arg, int newbit)
    {
        int mask = 1 << newbit;
        int value = (~mask) & arg;
        return value;
    }

    // Sets the bit at `newbit' in `arg' to 1
    int InBuilderProducer::SetBit(int arg, int newbit)
    {
        int mask = 1 << newbit;
        int value = mask | arg;
        return value;
    }

    // Tests the value of the bit at `bit' in `word'
    bool InBuilderProducer::TestBit(int word, int bit)
    {
        // shift the bits in word to the right by bit
        // to put the bit-th bit at the least sig position i.e. at bit 0
        int shifted = word >> bit;
        bool value;
        if(shifted&1){value = true;}
        else{value = false;}
        return value;
    }

    // Get the n bits at location loc in arg, counting the first bit as ZERO
    int InBuilderProducer::GetBits(int arg, int loc, int n)
    {
        int shifted = arg >> loc;
        // Select the first (least significant) n of those bits
        int mask = ((ULong64_t)1 << n) - 1;
        int value = shifted & mask;
        return value;
    }
    // Version for a long int
    ULong64_t InBuilderProducer::GetBits(ULong64_t arg, int loc, int n)
    {
        ULong64_t shifted = arg >> loc;
        // Select the first (least significant) n of those bits
        ULong64_t mask = ((ULong64_t)1 << n) - 1;
        ULong64_t value = shifted & mask;
        return value;
    }
    // Version for an unsigned int
    UInt_t InBuilderProducer::GetBits(UInt_t arg, int loc, int n)
    {
        UInt_t shifted = arg >> loc;
        // Select the first (least significant) n of those bits
        UInt_t mask = ((ULong64_t)1 << n) - 1;
        UInt_t value = shifted & mask;
        return value;
    }


    // Set the bits from (least sig) location loc in arg to val, counting the first bit as ZERO
    int InBuilderProducer::SetBits(int arg, int loc, int val)
    {
        int shifted = val << loc;
        int value = shifted | arg;
        return value;
    }

    bool InBuilderProducer::CheckLength(int arg, int length)
    {
        int checkval = InBuilderProducer::GetBits(arg,0,length);
        bool value = true;
        if(checkval != arg)value=false;
        return value;
    }

    // Handle FATAL errors
    void InBuilderProducer::Die(std::string message, int flag_err, int info1, int info2, int info3, int return_code)
    {
        warn << "\033[31m UNPACKING PROBLEM: \033[m" <<newline;

        if(flag_err==1){  // Event out of order
            warn <<"Discrepancy in event order:"<<newline;
            warn<<"We're on event "<<info1<<" but it self-identifies as event "<<info2<<newline;
            warn<<"Something is wrong."<<newline;
            warn<<newline;
        }

        if(flag_err==2){  // NHits' don't match
            warn <<"Discrepancy in NHits: "<<newline;
            warn<<"This event claimed to have "<<info1<<" but we have added "<<info2<<" new PMTs"<<newline;
            warn<<"Something is wrong."<<newline;
            warn<<newline;
        }

        if(flag_err==3){  // 2 Header banks where there should be 1
            std::string bank = "run";
            if(message=="TRIGInfo"||message=="EPEDInfo")bank = "event";
            warn <<"Discrepancy in header info: "<<newline;
            warn<<"There are 2 "<<message<<" objects in this "<<bank;
            warn<<" when there should be <=1!"<<newline;
            warn<<"Something is wrong."<<newline;
            warn<<newline;
        }

        if(flag_err==4){  // Unmatched Info
            warn <<"Discrepancy in event/header info: "<<newline;
            warn<<message<<" does not match up: "<<info1<<" & "<<info2<<newline;
            warn<<"Something is wrong."<<newline;
            warn<<newline;
        }

        if(flag_err==5){  // Records out of order
            warn <<message<<" record in the packed file is out of order: "<<newline;
            warn<<"Valid for event "<<info3<<newline;
            warn<<"But comes between events "<<info1<<" & "<<info2<<" in the file"<<newline;
            warn<<"Something is wrong."<<newline;
            warn<<newline;
        }

        if(flag_err==6){  // Should never get here
            warn <<"How did you even get here in the code???"<<newline;
            warn<<"You should have died in the call to ``CheckOrder'' for "<<message<<newline;
            warn<<"Something is wrong."<<newline;
            warn<<newline;
        }
    }
} // namespace RAT

