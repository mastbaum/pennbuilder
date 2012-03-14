#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <jemalloc/jemalloc.h>
#include <avalanche.hpp>
#include <TFile.h>
#include <TTree.h>

#include <shipper.h>
#include <ds.h>
#include <handler.h>
#include <PackedEvent.hh>

extern Buffer<EventRecord*>* event_buffer;
extern std::deque<RAT::DS::PackedRec*> event_header_buffer;
extern std::deque<RAT::DS::PackedRec*> run_header_buffer;
extern BuilderStats stats;

TFile* outfile = NULL;
TTree* tree = NULL;
RAT::DS::PackedEvent* rec = NULL;
//RAT::DS::PackedRec* rec = NULL;

void* shipper(void* ptr) {
    clock_t time_last_shipped = clock();
    uint32_t current_gtid = 0;
    uint32_t gtid_last_shipped = 0;
    bool print_rh_waiting = false;

    avalanche::server* dispatcher = new avalanche::server(DISPATCHER_ADDRESS);
    outfile = NULL;
    tree = NULL;

    signal(SIGINT, &handler);
    while (1) {
        // FIXME status print to separate thread, print then sleep
        /*
        clock_t time_last_print = 0;
        if ((float)(clock() - time_last_print) / CLOCKS_PER_SEC > 0.25) {
            unsigned gtid_head = event_buffer->keys[event_buffer->write-1] ? \
                                 ((EventRecord*)(event_buffer->keys[event_buffer->write-1]))->gtid : 0;

            printf("shipper: run %i: events (%#x) %lu..<%lu>..%lu (%#x) | headers %lu..<%lu>..%lu  | %s\n",
                    outfile ? stats.run_id : 0,
                    gtid_head,
                    event_buffer->read,
                    (event_buffer->write-event_buffer->read) % event_buffer->size,
                    event_buffer->write,
                    gtid_last_shipped,
                    run_header_buffer->read,
                    (run_header_buffer->write-run_header_buffer->read) % run_header_buffer->size,
                    run_header_buffer->write,
                    run_active ? "running" : "run stoppped");

            time_last_print = clock();
            
        }
        */

        //// do nothing if the event buffer is empty
        if (event_buffer->read == event_buffer->write) {
            continue;
        }

        EventRecord* er = event_buffer->elem[event_buffer->read];

        // keep track of gtid even during skips, to know when to start a run, etc.
        if (er)
            current_gtid = er->gtid;
        else
            current_gtid++;

        //// ship run headers (assume that they arrive in order)
        while (run_header_buffer.front()) {
            RAT::DS::PackedRec* rtemp = run_header_buffer.front();
            if (rtemp->RecordType == RAT::DS::kRecRHDR) {
                RAT::DS::RHDR* rhdr = dynamic_cast<RAT::DS::RHDR*>(rtemp->Rec);
                if (current_gtid == rhdr->ValidEventID) {
                    if (outfile) {
                        outfile->cd();
                        tree->Write();
                        outfile->Close();
                        delete outfile; // deletes tree and rec
                        outfile = NULL;
                        printf("shipper: closed run %i file for soft run start\n", stats.run_id);
                    }

                    stats.run_id = rhdr->RunID;
                    char filename[100];
                    sprintf(filename, "run_%i.root", stats.run_id);
                    outfile = new TFile(filename, "recreate");
                    stats.filename = filename;
                    printf("shipper: starting new run: id %i, first gtid %#x (%s)\n", stats.run_id, rhdr->ValidEventID, filename);

                    rec = new RAT::DS::PackedEvent();
                    //rec = new RAT::DS::PackedRec();
                    tree = new TTree("PackT", "RAT Tree");
                    tree->SetAutoSave(10 * 1024 * 1024); // 10 MiB
                    tree->Branch("PackEv", rec->ClassName(), &rec, 32000, 99);
                    //tree->Branch("PackRec", rec->ClassName(), &rec, 32000, 99);
                    //rec->RecordType = rtemp->RecordType;
                    //rec->Rec = rhdr;
                    //tree->Fill();
                    dispatcher->sendObject(rhdr);

                    delete rhdr;
                    run_header_buffer.pop_back();
                }
                else
                    break;
            }
            /*
            else if (rtemp->RecordType == RAT::DS::kRecCAAC) {
                RAT::DS::CAAC* caac = dynamic_cast<RAT::DS::CAAC*>(rtemp->Rec);
                if (current_gtid >= 0) { // caac->ValidEventID) 
                    //rec->RecordType = rtemp->RecordType;
                    //rec->Rec = caac;
                    //tree->Fill();
                    dispatcher->sendObject(caac);

                    delete caac;
                    run_header_buffer.pop_back();
                }
                else
                    break;
            }
            else if (rtemp->RecordType == RAT::DS::kRecCAST) {
                RAT::DS::CAST* cast = dynamic_cast<RAT::DS::CAST*>(rtemp->Rec);
                if (current_gtid >= 0) { // cast->ValidEventID) 
                    //rec->RecordType = rtemp->RecordType;
                    //rec->Rec = cast;
                    //tree->Fill();
                    dispatcher->sendObject(cast);

                    delete cast;
                    run_header_buffer.pop_back();
                }
                else
                    break;
            }
            */
            else {
                printf("shipper: encountered header of unknown type %i in run header buffer\n", rtemp->RecordType);
                delete rtemp->Rec;
                run_header_buffer.pop_back();
            }
        }

        //// hold until we get a run header
        if (!outfile) {
            if (!print_rh_waiting) {
                printf("shipper: waiting for run header...\n");
                print_rh_waiting = true;
            }
            continue;
        }
        print_rh_waiting = false;

        //// ship event-level headers
        while (event_header_buffer.front()) {
            RAT::DS::PackedRec* rtemp = event_header_buffer.front();

            if (rtemp->RecordType == RAT::DS::kRecEPED) {
                RAT::DS::EPED* eped = dynamic_cast<RAT::DS::EPED*>(rtemp->Rec);
                if (current_gtid >= eped->EventID) {
                    //rec->RecordType = rtemp->RecordType;
                    //rec->Rec = eped;
                    //tree->Fill();
                    dispatcher->sendObject(eped);
                    delete eped;
                    event_header_buffer.pop_back();
                }
                else
                    break;
            }
            else if (rtemp->RecordType == RAT::DS::kRecTRIG) {
                RAT::DS::TRIG* trig = dynamic_cast<RAT::DS::TRIG*>(rtemp->Rec);
                if (current_gtid >= trig->EventID) {
                    //rec->RecordType = rtemp->RecordType;
                    //rec->Rec = eped;
                    //tree->Fill();
                    dispatcher->sendObject(trig);
                    delete trig;
                    event_header_buffer.pop_back();
                }
                else
                    break;
            }
            else {
                printf("shipper: encountered header of unknown type %i in event header buffer\n", rtemp->RecordType);
                delete rtemp->Rec;
                event_header_buffer.pop_back();
            }
        }

        //// skipped gtid timeout
        if (!er) {
            if ((float)(clock() - time_last_shipped) / CLOCKS_PER_SEC > SKIP_GTID_DELAY) {
                current_gtid++;
                event_buffer->read++;
                time_last_shipped = clock();
                printf("shipper: skipped missing gtid after timeout, idx: %lu\n", event_buffer->read);
            }
            continue;
        }

        //// event timeout -- wait QUEUE_DELAY seconds for data before shipping
        if((float)(clock() - er->arrival_time) / CLOCKS_PER_SEC < QUEUE_DELAY) {
            continue;
        }

        //// finally, ship the event data
        gtid_last_shipped = er->gtid;
        time_last_shipped = clock();

        //rec->RecordType = kRecPMT;
        //rec->Rec = (RAT::DS::GenericRec*) er->event;

        if (er->has_pmt && er->has_mtc) {
            rec = er->event;
            if (tree)
                tree->Fill();
            else
                std::cout << "shipper: WARNING: tree unavailable for writing. events lost." << std::endl;
            dispatcher->sendObject(rec);
            //printf("shipper: shipped event with gtid %i\n", er->gtid);
        }

        // shipper statistics
        stats.events_written++;
        if (er->has_pmt)
            stats.events_with_pmt++;
        if (er->has_mtc)
            stats.events_with_mtc++;
        if (er->has_caen)
            stats.events_with_caen++;

        delete er;
        event_buffer->elem[event_buffer->read] = NULL;
        event_buffer->read = ++event_buffer->read % event_buffer->size;
    }

    delete dispatcher;
    delete outfile;

}

