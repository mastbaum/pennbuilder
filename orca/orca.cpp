#include <iostream>
#include <queue>
#include <stdlib.h>
#include <stdint.h>

#include <ORRunContext.hh>

#include <orca.h>
#include <ds.h>
#include <PackedEvent.hh>

extern Buffer<EventRecord*>* event_buffer;
extern std::deque<RAT::DS::PackedRec*> event_header_buffer;
extern std::deque<RAT::DS::PackedRec*> run_header_buffer;
extern BuilderStats stats;

ORBuilderProcessor::ORBuilderProcessor(std::string /*label*/) {
    SetComponentBreakReturnsFailure();

    fMTCProcessor = new ORDataProcessor(&fMTCDecoder);
    AddProcessor(fMTCProcessor);

    fPMTProcessor = new ORDataProcessor(&fPMTDecoder);
    AddProcessor(fPMTProcessor);

    fCaenProcessor = new ORDataProcessor(&fCaenDecoder);
    AddProcessor(fCaenProcessor);

    fRunProcessor = new ORDataProcessor(&fRunDecoder);
    AddProcessor(fRunProcessor);

    stats.run_active = false;

    fFirstGTIdSet = false;
    fCurrentGTId = 0;
    fEventOrder = 0;

    fCaenOffset = 0;
    fCaenLastGTId = 0;
}

ORBuilderProcessor::~ORBuilderProcessor() {
    delete fMTCProcessor;
    delete fPMTProcessor;
    delete fCaenProcessor;
    delete fRunProcessor;
}

ORDataProcessor::EReturnCode ORBuilderProcessor::StartRun() {
    std::cout << "orca: starting run " << (int)GetRunContext()->GetRunNumber() << std::endl;

    fMTCDataId = fMTCProcessor->GetDataId();
    fPMTDataId = fPMTProcessor->GetDataId();
    fCaenDataId = fCaenProcessor->GetDataId();
    fRunDataId = fRunProcessor->GetDataId();

    // run header
    RAT::DS::RHDR* rhdr = new RAT::DS::RHDR();
    rhdr->Date = 0;
    rhdr->Time = 0;
    rhdr->DAQVer = 100;
    rhdr->CalibTrialID = 0;
    rhdr->SrcMask = 0;
    rhdr->RunMask = 0;
    rhdr->CrateMask = 0;
    rhdr->FirstEventID = 0;
    rhdr->ValidEventID = fCurrentGTId;
    rhdr->RunID = GetRunContext()->GetRunNumber();

    RAT::DS::PackedRec* pr = new RAT::DS::PackedRec();
    pr->RecordType = RAT::DS::kRecRHDR;
    pr->Rec = rhdr;

    run_header_buffer.push_back(pr);

    stats.run_active = true;

    stats.records_received++;
    return kSuccess;
}

ORDataProcessor::EReturnCode ORBuilderProcessor::EndRun() {
    std::cout << "orca: ending run " << (int)GetRunContext()->GetRunNumber() << std::endl;

    stats.run_active = false;

    stats.records_received++;
    return kSuccess;
}

ORDataProcessor::EReturnCode ORBuilderProcessor::ProcessDataRecord(UInt_t* record) {
    unsigned int thisDataId = fMTCDecoder.DataIdOf(record); // any long decoder would do the job

    if (thisDataId == fMTCDataId) {
        ORDataProcessor::EReturnCode code = ORCompoundDataProcessor::ProcessDataRecord(record);
        if (code != kSuccess)
            return code;

        uint32_t gtid = record[4] & 0xffffff;    
        //std::cout << "got mtc data, gtid: " << gtid << std::endl;
        uint64_t idx = gtid & 0x3ffff; // bottom 17 bits, per ph

        fCurrentGTId = gtid;

        // data from event already shipped :(
        if (event_buffer->elem[event_buffer->read] && gtid < event_buffer->elem[event_buffer->read]->gtid) {
            printf("orca: dropped late data for gtid %u\n", gtid);
            return kSuccess;
        }

        if (!fFirstGTIdSet) {
            fFirstGTIdSet = true;
            event_buffer->read = idx;
        }

        pthread_mutex_lock(&(event_buffer->mutex[idx]));

        EventRecord* er = event_buffer->elem[idx];

        if (er) {
            if (er->gtid != gtid) {
                printf("orca: buffer overflow! ignoring gtid %i\n", gtid);
                return kSuccess;
            }
            if (er->has_mtc) {
                printf("orca: duplicate mtc data for gtid %#x. data lost!\n", gtid);
                return kSuccess;
            }
        }
        else {
            er = new EventRecord();
            er->event = new RAT::DS::PackedEvent();
            er->arrival_time = 0;
            er->gtid = 0;
            er->has_mtc = true;
            er->has_pmt = false;
            er->has_caen = false;

            event_buffer->elem[idx] = er;
        }

        er->event->EVOrder = fEventOrder;
        fEventOrder++;

        er->event->MTCInfo[0] = fMTCDecoder.Wrd0(record);
        er->event->MTCInfo[1] = fMTCDecoder.Wrd1(record);
        er->event->MTCInfo[2] = fMTCDecoder.Wrd2(record);
        er->event->MTCInfo[3] = fMTCDecoder.Wrd3(record);
        er->event->MTCInfo[4] = fMTCDecoder.Wrd4(record);
        er->event->MTCInfo[5] = fMTCDecoder.Wrd5(record);

        er->gtid = gtid;
        er->arrival_time = clock(); // arrival time is time of last data received

        pthread_mutex_unlock(&(event_buffer->mutex[idx]));
    }
    else if (thisDataId == fPMTDataId) {
        fPMTDecoder.Swap(record);
	//std::cout << "got megabundle" << std::endl;

        unsigned int bundle_length = (fPMTDecoder.LengthOf(record) - 2) / 3;
        //printf("bundle_length %i\n", bundle_length);
        record += 2;
        for (; bundle_length != 0; bundle_length--) {
            uint32_t gtid = fPMTDecoder.GTId(record);
            uint64_t idx = gtid & 0x3ffff;

            fCurrentGTId = gtid;

            if (!fFirstGTIdSet) {
                fFirstGTIdSet = true;
                event_buffer->read = idx;
            }

            pthread_mutex_lock(&(event_buffer->mutex[idx]));

            EventRecord* er = event_buffer->elem[idx];

            if (er) {
                if (er->gtid != gtid) {
                    printf("Buffer overflow! Ignoring GTID %i\n", gtid);
                    return kSuccess;
                }
            }
            else {
                er = new EventRecord();
                er->event = new RAT::DS::PackedEvent();
                er->arrival_time = 0;
                er->gtid = 0;
                er->has_mtc = false;
                er->has_pmt = true;
                er->has_caen = false;

                event_buffer->elem[idx] = er;
            }

            RAT::DS::PMTBundle rpmtb;
            rpmtb.Word[0] = fPMTDecoder.Wrd0(record);
            rpmtb.Word[1] = fPMTDecoder.Wrd1(record);
            rpmtb.Word[2] = fPMTDecoder.Wrd2(record);
            er->event->PMTBundles.push_back(rpmtb);
            er->event->NHits++;

            record += 3;

	    pthread_mutex_unlock(&(event_buffer->mutex[idx]));
        }
    }
    else if (thisDataId == fCaenDataId) {
        ORDataProcessor::EReturnCode code = ORCompoundDataProcessor::ProcessDataRecord(record);
        if (code != kSuccess)
            return code;

        // "corrected" caen gtid
        uint32_t gtid = fCaenDecoder.EventCount(record);
        gtid += gtid >> 16;
        if (gtid & 0x0000ffff)
            gtid++;
        gtid += fCaenOffset;
        gtid &= 0x00ffffff;
	//std::cout << "got caen data, gtid: " << gtid << std::endl;

        uint64_t idx = gtid & 0x3ffff;

        fCurrentGTId = gtid;

        if (!fFirstGTIdSet) {
            fFirstGTIdSet = true;
            event_buffer->read = idx;
        }

        pthread_mutex_lock(&(event_buffer->mutex[idx]));

        EventRecord* er = event_buffer->elem[idx];

        if (er) {
            if (er->gtid != gtid) {
                printf("Buffer overflow! Ignoring GTID %i\n", gtid);
                return kSuccess;
            }
            if (er->has_caen) {
                printf("orca: duplicate caen data for gtid %#x. data lost!\n", gtid);
                return kSuccess;
            }
        }
        else {
            er = new EventRecord();
            er->event = new RAT::DS::PackedEvent();
            er->arrival_time = 0;
            er->gtid = 0;
            er->has_mtc = false;
            er->has_pmt = false;
            er->has_caen = true;

            event_buffer->elem[idx] = er;
        }

        UInt_t n = fCaenDecoder.LengthOf(record) - 2;

        RAT::DS::CaenBundle rcaen;
        rcaen.ChannelMask = fCaenDecoder.ChannelMask(record);
        rcaen.Pattern = fCaenDecoder.Pattern(record);
        rcaen.EventCount = fCaenDecoder.EventCount(record);
        rcaen.Clock = fCaenDecoder.Clock(record);

        UInt_t numSamples = fCaenDecoder.TraceLength(record, 0);
        record += 6; // 2 ORCA long header + 4 CAEN header

        for (unsigned int i=0; i<8; i++) {
            if ((1 << i) & rcaen.ChannelMask) {
                UShort_t* trace = (UShort_t*) malloc(sizeof(UShort_t)*numSamples);
                fCaenDecoder.CopyTrace(record, trace, numSamples);
                RAT::DS::CaenTrace rcaentrace;
                for (int i=0; i<numSamples; i++)
                    rcaentrace.Waveform.push_back(trace[i]);
                rcaen.Trace.push_back(rcaentrace);
                free(trace);
            }
        }

        er->event->Caen = rcaen;

        pthread_mutex_unlock(&(event_buffer->mutex[idx]));
    }
    else if (thisDataId == fRunDataId) {
        ORDataProcessor::EReturnCode code = ORCompoundDataProcessor::ProcessDataRecord(record);
        if (code != kSuccess)
            return code;

        if (record[1] & 0x1) {
            if (record[1] & 0x2) {
                printf("orca: soft run start");
            }
            else {
                printf("orca: hard run start");
                fEventOrder = 0;
                fCurrentGTId = 0;
                fCaenOffset = 0;
                fCaenLastGTId = 0;
            }
        }

        // FIXME move printout to new thread
        /*
        printf("orca: %i/%#x: caen %f | pmt %f | mtc %f | unhandled %f\n",
            (int)GetRunContext()->GetRunNumber(),
            fCurrentGTId,
            (float)pCaenCount/fEventOrder,
            (float)pPMTCount/fEventOrder,
            (float)pMTCCount/fEventOrder,
            (float)pUnhandledCount/fTotalReceived);
        */
    }
    else {
        //std::cout << "unhandled record: id: " << std::hex << (int)thisDataId << std::dec << std::endl;
        stats.records_unhandled++;
    }

    stats.records_received++;

    return kSuccess;
}

