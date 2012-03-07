#include <iostream>
#include <stdlib.h>

#include <TFile.h>
#include <TTree.h>
#include <PackedEvent.hh>
#include <ORRunContext.hh>

#include "orca.h"
#include "ds.h"

const int kGTIdWindow = 10000000; 

extern Buffer* event_buffer;
extern Buffer* run_header_buffer;

int run_active;

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

    fTotalReceived = 0;
    pUnhandledCount = 0;
    fCurrentGTId = 0;
    fCaenOffset = 0;
    fCaenLastGTId = 0;
    pMTCCount = 0;
    pCaenCount = 0;
    pPMTCount = 0;
    fEventOrder = 0;
}

ORBuilderProcessor::~ORBuilderProcessor() {
    delete fMTCProcessor;
    delete fPMTProcessor;
    delete fCaenProcessor;
    delete fRunProcessor;
}

ORDataProcessor::EReturnCode ORBuilderProcessor::StartRun() {
    std::cout << "ORBuilderProcessor::StartRun: starting run " << (int)GetRunContext()->GetRunNumber() << std::endl;

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
    buffer_push(run_header_buffer, RUN_HEADER, rhdr);

    run_active = 1;
    fTotalReceived++;
    return kSuccess;
}

ORDataProcessor::EReturnCode ORBuilderProcessor::EndRun() {
    std::cout << "ORBuilderProcessor::EndRun: ending run " << (int)GetRunContext()->GetRunNumber() << std::endl;
    std::cout << "ORBuilderProcessor::EndRun: received " << fTotalReceived << " records so far" << std::endl;

    run_active = 0;
    fTotalReceived++;
    return kSuccess;
}

ORDataProcessor::EReturnCode ORBuilderProcessor::ProcessDataRecord(UInt_t* record) {
    unsigned int thisDataId = fMTCDecoder.DataIdOf(record); // any long decoder would do the job

    EventRecord* er;
    RecordType r;

    if (thisDataId == fMTCDataId) {
        ORDataProcessor::EReturnCode code = ORCompoundDataProcessor::ProcessDataRecord(record);
        if (code != kSuccess)
            return code;

        uint32_t gtid = record[4] & 0xffffff;    
        uint64_t keyid = buffer_keyid(event_buffer, gtid);

        fCurrentGTId = gtid;

        //std::cout << "got mtc data, gtid: " << gtid << std::endl;
        if (pPMTCount == 0 && fEventOrder == 0)
            event_buffer->offset = gtid;

        pthread_mutex_lock(&(event_buffer->mutex_buffer[keyid]));

        buffer_at(event_buffer, gtid, &r, (void**)&er);

        if (!er) {
            er = new EventRecord();
            er->event = NULL;
            er->arrival_time = 0;
            er->gtid = 0;
            buffer_insert(event_buffer, gtid, DETECTOR_EVENT, (void*)er);
        }

        er->has_mtc = true;

        if (!er->event) {
            er->event = new RAT::DS::PackedEvent();
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
        er->arrival_time = clock();

        if (er && er->gtid != gtid) {
            printf("Buffer overflow! Ignoring GTID %i\n", gtid);
        }

        pthread_mutex_unlock(&(event_buffer->mutex_buffer[keyid]));

        pMTCCount++;
    }
    else if (thisDataId == fPMTDataId) {
        fPMTDecoder.Swap(record);
	//std::cout << "got megabundle" << std::endl;

        unsigned int bundle_length = (fPMTDecoder.LengthOf(record) - 2) / 3;
        //printf("bundle_length %i\n", bundle_length);
        record += 2;
        for (; bundle_length != 0; bundle_length--) {
            uint32_t gtid = fPMTDecoder.GTId(record);
            //printf("gtid %i, bundles remaining %i\n", gtid, bundle_length);
            uint64_t keyid = buffer_keyid(event_buffer, gtid);

            fCurrentGTId = gtid;
	    if (pPMTCount == 0 && fEventOrder == 0)
		    event_buffer->offset = gtid;

	    pthread_mutex_lock(&(event_buffer->mutex_buffer[keyid]));

	    buffer_at(event_buffer, gtid, &r, (void**)&er);

            if (!er) {
                er = new EventRecord();
                buffer_insert(event_buffer, gtid, DETECTOR_EVENT, (void*)er);
            }

            er->has_bundles = true;

            if (!er->event) {
                er->event = new RAT::DS::PackedEvent();
            }

            RAT::DS::PMTBundle rpmtb;
            rpmtb.Word[0] = fPMTDecoder.Wrd0(record);
            rpmtb.Word[1] = fPMTDecoder.Wrd1(record);
            rpmtb.Word[2] = fPMTDecoder.Wrd2(record);
            er->event->PMTBundles.push_back(rpmtb);
            er->event->NHits++;

            record += 3;

	    pthread_mutex_unlock(&(event_buffer->mutex_buffer[keyid]));
        }

        pPMTCount++;
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

        if (pPMTCount == 0 && fEventOrder == 0)
            event_buffer->offset = gtid;

        fCurrentGTId = gtid;

        uint64_t keyid = buffer_keyid(event_buffer, gtid);
	//std::cout << "got caen data, gtid: " << gtid << std::endl;

        pthread_mutex_lock(&(event_buffer->mutex_buffer[keyid]));

        buffer_at(event_buffer, gtid, &r, (void**)&er);

        if (!er) {
            er = new EventRecord();
            buffer_insert(event_buffer, gtid, DETECTOR_EVENT, (void*)er);
        }
            
        er->has_caen = true;

        if (!er->event) {
            er->event = new RAT::DS::PackedEvent();
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

        pthread_mutex_unlock(&(event_buffer->mutex_buffer[keyid]));

        pCaenCount++;
    }
    else if (thisDataId == fRunDataId) {
        ORDataProcessor::EReturnCode code = ORCompoundDataProcessor::ProcessDataRecord(record);
        if (code != kSuccess)
            return code;

        if (!(record[1] & 0x8) && !(record[1] & 0x2)) {
            fCurrentGTId = 0;
            fCaenOffset = 0;
            fCaenLastGTId = 0;
            pMTCCount = 0;
            pCaenCount = 0;
            pPMTCount = 0;
            fEventOrder = 0;
        }

        printf("orca: %i/%#x: caen %f | pmt %f | mtc %f | unhandled %f\n",
            (int)GetRunContext()->GetRunNumber(),
            fCurrentGTId,
            (float)pCaenCount/fEventOrder,
            (float)pPMTCount/fEventOrder,
            (float)pMTCCount/fEventOrder,
            (float)pUnhandledCount/fTotalReceived);
    }
    else {
        //std::cout << "unhandled record: id: " << std::hex << (int)thisDataId << std::dec << std::endl;
        pUnhandledCount++;
    }

    fTotalReceived++;
    return kSuccess;
}

