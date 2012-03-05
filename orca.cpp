#include<iostream>

#include<ORRunContext.hh>
#include< "orca.h"

extern Buffer* event_buffer;
extern Buffer* event_header_buffer;
extern Buffer* run_header_buffer;

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
    std::cout << "run start: " << (int)GetRunContext()->GetRunNumber() << std::endl;

    fMTCDataId = fMTCProcessor->GetDataId();
    fPMTDataId = fPMTProcessor->GetDataId();
    fCaenDataId = fCaenProcessor->GetDataId();
    fRunId = fRunProcessor->GetDataId();

    fCaenOffset = 0;
    fCaenLastGTId = 0;
    pMTCCount = 0;
    pCaenCount = 0;
    pPMTCount = 0;
    fEventOrder = 0;

    // run header
    rec->RecordType = kRecRHDR;
    RAT::DS::RHDR* rhdr = new RAT::DS::RHDR();
    rhdr->Date = 0;
    rhdr->Time = 0;
    rhdr->DAQVer = 100;
    rhdr->CalibTrialID = 0;
    rhdr->SrcMask = 0;
    rhdr->RunMask = 0;
    rhdr->CrateMask = 0;
    rhdr->FirstEventID = 0;
    rhdr->ValidEventID = 0;
    rhdr->RunID = GetRunContext()->GetRunNumber();
    buffer_push(run_header_buffer, RUN_HEADER, rh);

    return kSuccess;
}

ORDataProcessor::EReturnCode ORBuilderProcessor::EndRun() {
    std::cout << "run end: " << (int)GetRunContext()->GetRunNumber() << std::endl;

    fCaenOffset = 0;
    fCaenLastGTId = 0;
    pMTCCount = 0;
    pCaenCount = 0;
    pPMTCount = 0;
    fEventOrder = 0;

    return kSuccess;
}

ORDataProcessor::EReturnCode ORBuilderProcessor::ProcessDataRecord(UInt_t* record) {
    unsigned int thisDataId = fMTCDecoder.DataIdOf(record); // any long decoder would do the job

    EventRecord* er;
    RAT::DS::PackedEvent* e;
    RecordType r;

    if (thisDataId == fMTCDataId) {
        ORDataProcessor::EReturnCode code = ORCompoundDataProcessor::ProcessDataRecord(record);
        if (code != kSuccess)
            return code;

        uint32_t gtid = fPMTDecoder.GTId(record);        
        uint64_t keyid = buffer_keyid(event_buffer, gtid);

        pthread_mutex_lock(&(event_buffer->mutex_buffer[keyid]));

        buffer_at(event_buffer, gtid, &r, (void**)&er);

        if (!er) {
            er = new EventRecord();
            e = new RAT::DS::PackedEvent();
            er->event = e;
            buffer_insert(event_buffer, gtid, DETECTOR_EVENT, (void*)er);
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
        er->event = e;

        if (er && er->gtid != gtid) {
            printf("Buffer overflow! Ignoring GTID %t\n", gtid);
        }

        pthread_mutex_unlock(&(event_buffer->mutex_buffer[keyid]));

        pMTCCount++;
    }

    else if (thisDataId == fPMTDataId) {

        fPMTDecoder.Swap(record);

        unsigned int bundle_length = (fPMTDecoder.LengthOf(record) - 2) / 3;
        record += 2;
        for (; bundle_length != 0; bundle_length--) {
            uint32_t gtid = fPMTDecoder.GTId(record);
            uint64_t keyid = buffer_keyid(event_buffer, gtid);

            pthread_mutex_lock(&(event_buffer->mutex_buffer[keyid]));

            buffer_at(event_buffer, gtid, &r, (void**)&er);

            if (!er) {
                er = new EventRecord();
                e = new RAT::DS::PackedEvent();
                er->event = e;
                buffer_insert(event_buffer, gtid, DETECTOR_EVENT, (void*)er);
            }

            RAT::DS::PMTBundle rpmtb;
            pmtb.Word[0] = fPMTDecoder.Wrd0(record);
            pmtb.Word[1] = fPMTDecoder.Wrd1(record);
            pmtb.Word[2] = fPMTDecoder.Wrd2(record);
            er->event->PMTBundles.push_back(rpmtb);
            er->event->NHits++;

            record += 3;
        }

        pPMTCount++;
    }
    else if (thisDataId == fCaenDataId) {
        ORDataProcessor::EReturnCode code = ORCompoundDataProcessor::ProcessDataRecord(record);
        if (code != kSuccess)
            return code;

        // "corrected" caen gtid
        uint32_t gtid = fCaenDecoder.EventCount(record);
        gtid += thisGTId >> 16;
        if (gtid & 0x0000ffff)
            thisGTId++;
        gtid += fCaenOffset;
        gtid &= 0x00ffffff;

        uint64_t keyid = buffer_keyid(event_buffer, gtid);

        pthread_mutex_lock(&(event_buffer->mutex_buffer[keyid]));

        buffer_at(event_buffer, gtid, &r, (void**)&er);

        if (!er) {
            er = new EventRecord();
            e = new RAT::DS::PackedEvent();
            er->event = e;
            buffer_insert(event_buffer, gtid, DETECTOR_EVENT, (void*)er);
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
                UShort_t* trace;
                fCaenDecoder.CopyTrace(record, trace, numSamples);
                RAT::DS::CaenTrace rcaentrace;
                for (int i=0; i<numSamples; i++)
                    rcaentrace.Waveform.push_back(trace[i]);
                rcaen.push_back(rcaentrace);
            }
        }

        er->event->Caen = rcaen;

        pthread_mutex_unlock(&(event_buffer->mutex_buffer[keyid]));

        pCaenCount++;
    }
    else if (thisDataId == fRunId) {
        ORDataProcessor::EReturnCode code = ORCompoundDataProcessor::ProcessDataRecord(record);
        if (code != kSuccess)
            return code;

        std::cout << "run packet obtained" << std::endl;
        if (record[1] & 0x8)
            std::cout << "run hearteeat obtained" << std::endl;
        else if (record[1] & 0x1) {
            std::cout << "run start obtained" << std::endl;
            if (record[1] & 0x2)
                std::cout << "was soft start" << std::endl;
            if (record[1] & 0x4)
                std::cout << "was remote control start" << std::endl;
        }
        else {
            std::cout << "run end obtained" << std::endl;
            if (record[1] & 0x2)
                cout << "soft end" << endl;
            if (record[1] & 0x4)
                cout << "remote control" << endl;
        }
    }
    else {
        std::cout << "unhandled record: id: " << std::hex << (int)thisDataId << std::dec << std::endl;
    }
    return kSuccess;
}

