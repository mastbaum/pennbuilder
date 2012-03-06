#include <iostream>

#include <ORRunContext.hh>
#include "orca.h"
#include "ds.h"

void handler(int signal);

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

    fMTCDataId = fMTCProcessor->GetDataId();
    fPMTDataId = fPMTProcessor->GetDataId();
    fCaenDataId = fCaenProcessor->GetDataId();
    fRunDataId = fRunProcessor->GetDataId();
}

ORBuilderProcessor::~ORBuilderProcessor() {
    delete fMTCProcessor;
    delete fPMTProcessor;
    delete fCaenProcessor;
    delete fRunProcessor;
}

ORDataProcessor::EReturnCode ORBuilderProcessor::StartRun() {
    std::cout << "run start: " << (int)GetRunContext()->GetRunNumber() << std::endl;

    fCaenOffset = 0;
    fCaenLastGTId = 0;
    pMTCCount = 0;
    pCaenCount = 0;
    pPMTCount = 0;
    fEventOrder = 0;

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
    rhdr->ValidEventID = 0;
    rhdr->RunID = GetRunContext()->GetRunNumber();
    buffer_push(run_header_buffer, RUN_HEADER, rhdr);

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
            printf("Buffer overflow! Ignoring GTID %i\n", gtid);
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
            rpmtb.Word[0] = fPMTDecoder.Wrd0(record);
            rpmtb.Word[1] = fPMTDecoder.Wrd1(record);
            rpmtb.Word[2] = fPMTDecoder.Wrd2(record);
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
        gtid += gtid >> 16;
        if (gtid & 0x0000ffff)
            gtid++;
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
                rcaen.Trace.push_back(rcaentrace);
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
                std::cout << "soft end" << std::endl;
            if (record[1] & 0x4)
                std::cout << "remote control" << std::endl;
        }
    }
    else {
        std::cout << "unhandled record: id: " << std::hex << (int)thisDataId << std::dec << std::endl;
    }
    return kSuccess;
}

#include <stdlib.h>
#include <fstream>
#include <sstream>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h> 
#include <set> 

#include "ORDataProcManager.hh"
#include "ORLogger.hh"
#include "ORSocketReader.hh"
#include "OROrcaRequestProcessor.hh"
#include "ORServer.hh"
#include "ORHandlerThread.hh"

void* orca_listener(void* arg) {
    OrcaURL* ohi = (OrcaURL*) arg;
    std::string orcahost = ohi->host;
    int orcaport = ohi->port;

    std::string label = "OR";
    ORVReader* reader = NULL;

    bool keepAliveSocket = false;
    bool runAsDaemon = false;
    unsigned long timeToSleep = 10; //default sleep time for sockets.
    unsigned int reconnectAttempts = 0; // default reconnect tries for sockets.
    unsigned int portToListenOn = 0;
    unsigned int maxConnections = 5; // default connections accepted by server

    ORHandlerThread* handlerThread = new ORHandlerThread();
    handlerThread->StartThread();

    /***************************************************************************/
    /*   Running orcaroot as a daemon server. */
    /***************************************************************************/
    if (runAsDaemon) {
        /* Here we start listening on a socket for connections. */
        /* We are doing this very simply with a simple fork. Eventually want
           to check number of spawned processes, etc.  */
        std::cout << "Running orcaroot as daemon on port: " << portToListenOn << std::endl;
        pid_t childpid = 0;
        std::set<pid_t> childPIDRecord;

        ORServer* server = new ORServer(portToListenOn);
        /* Starting server, binding to a port. */
        if (!server->IsValid()) {
            std::cout << "Error listening on port " << portToListenOn 
                      << std::endl << "Error code: " << server->GetErrorCode()
                      << std::endl;
            return NULL;
        }

        signal(SIGINT, &handler);
        while (1) {
            /* This while loop is broken by a kill signal which is well handled
             * by the server.  The kill signal will automatically propagate to the
             * children so we really don't have to worry about waiting for them to
             * die.  */
            while (childPIDRecord.size() >= maxConnections) { 
                /* We've reached our maximum number of child processes. */
                /* Wait for a process to end. */
                childpid = wait3(0, WUNTRACED, 0);
                if(childPIDRecord.erase(childpid) != 1) {
                    /* Something really weird happened. */
                    std::cout << "Ended child process " << childpid 
                              << " not recognized!" << std::endl;
                }
            }
            while((childpid = wait3(0,WNOHANG,0)) > 0) {
                /* Cleaning up any children that may have ended.                   * 
                 * This will just go straight through if no children have stopped. */
                if(childPIDRecord.erase(childpid) != 1) {
                    /* Something really weird happened. */
                    std::cout << "Ended child process " << childpid 
                              << " not recognized!" << std::endl;
                }
            } 
            std::cout << childPIDRecord.size()  << " connections running..." << std::endl;
            std::cout << "Waiting for connection..." << std::endl;
            TSocket* sock = server->Accept(); 
            if (sock == (TSocket*) 0 || sock == (TSocket*) -1 ) {
                // There was an error, or the socket got closed .
                if (!server->IsValid()) return 0;
                continue;
            }
            if(!sock->IsValid()) {
                /* Invalid socket, cycle to wait. */
                delete sock;
                continue;
            }
            if ((childpid = fork()) == 0) {
                /* We are in the child process.  Set up reader and fire away. */
                delete server;
                delete handlerThread;
                handlerThread = new ORHandlerThread;
                handlerThread->StartThread();
                reader = new ORSocketReader(sock, true);
                /* Get out of the while loop */
                break;
            } 
            /* Parent process: wait for next connection. Close our descriptor. */
            std::cout << "Connection accepted, child process begun with pid: " 
                      << childpid << std::endl;
            childPIDRecord.insert(childpid);
            delete sock; 
        }

        /***************************************************************************/
        /*  End daemon server code.  */
        /***************************************************************************/
    } else {
        /* Normal running, either connecting to a server or reading in a file. */
        reader = new ORSocketReader("snoplusdaq1.snolab.ca", 44666); //orcahost.c_str(), orcaport);
    }
    if (!reader->OKToRead()) {
        ORLog(kError) << "Reader couldn't read" << std::endl;
        return NULL;
    }

    std::cout << "Setting up data processing manager..." << std::endl;
    ORDataProcManager dataProcManager(reader);

    /* Declare processors here. */
    OROrcaRequestProcessor orcaReq;
    ORBuilderProcessor builderProcessor(label);

    if (runAsDaemon) {
        /* Add them here if you wish to run them in daemon mode ( not likely ).*/
        dataProcManager.SetRunAsDaemon();
        dataProcManager.AddProcessor(&orcaReq);
    } else {
        /* Add the processors here to run them in normal mode. */
        std::cout << "adding builderproc" << std::endl;
        dataProcManager.AddProcessor(&builderProcessor);
    }

    std::cout << "Start processing..." << std::endl;
    dataProcManager.ProcessDataStream();
    std::cout << "Finished processing..." << std::endl;

    delete reader;
    delete handlerThread;

    return NULL;
}

