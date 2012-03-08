#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <jemalloc/jemalloc.h>
#include <avalanche.hpp>
#include <TFile.h>
#include <TTree.h>

#include "PackedEvent.hh"
#include "shipper.h"
#include "ds.h"
#include "handler.h"

extern Buffer* event_buffer;
extern Buffer* event_header_buffer;
extern Buffer* run_header_buffer;
extern int run_active;

bool flush_all_buffers = false;
bool flush_complete = false;

TFile* outfile = NULL;
TTree* tree = NULL;
//RAT::DS::PackedRec* rec = NULL;
RAT::DS::PackedEvent* rec = NULL;

void* shipper(void* ptr) {
    clock_t time_last_shipped = clock();
    unsigned int gtid_last_shipped = 0;
    char filename[100];
    bool print_rh_waiting = false;
    avalanche::server* dispatcher = new avalanche::server(DISPATCHER_ADDRESS);
    outfile = NULL;
    tree = NULL;
    int run_id = 0;
    clock_t time_last_print = 0;
    clock_t run_start_time = clock();

    signal(SIGINT, &handler);
    while (1) {
        if (flush_all_buffers && buffer_isempty(event_buffer) && buffer_isempty(event_header_buffer) && buffer_isempty(run_header_buffer)) {
            printf("shipper: flushed all buffers -----------------------------\n");
            flush_complete = true;
        }

        if ((float)(clock() - time_last_print) / CLOCKS_PER_SEC > 0.25) {
            unsigned gtid_head = event_buffer->keys[event_buffer->write-1] ? \
                                 ((EventRecord*)(event_buffer->keys[event_buffer->write-1]))->gtid : 0;

            printf("shipper: run %i: events (%#x) %lu..<%lu>..%lu (%#x) | headers %lu..<%lu>..%lu  | %s\n",
                    outfile ? run_id : 0,
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

        if (!run_active && buffer_isempty(event_buffer)) {
            if (outfile && tree) {
                outfile->cd();
                tree->Write();
                outfile->Close();
                delete outfile;
                outfile = NULL;
                printf("shipper: closed run file\n");
            }
            //printf("no run and empty buffer\n");
            continue;
        }

        EventRecord* ertemp = (EventRecord*) event_buffer->keys[event_buffer->read];
        // skipped gtid timeout
        if (!ertemp) {
            if ((float)(clock() - time_last_shipped) / CLOCKS_PER_SEC > SKIP_GTID_DELAY) {
                RecordType rtemp;
                buffer_pop(event_buffer, &rtemp, (void**)&ertemp);
                time_last_shipped = clock();
                printf("shipper: skipped missing gtid after timeout, run_active %i\n", run_active);
            }
            //printf("active run but no event\n");
            continue;
        }

        if (ertemp->arrival_time >= run_start_time) {
            while (run_header_buffer->keys[run_header_buffer->read]) {
                void* header = run_header_buffer->keys[run_header_buffer->read];
                RecordType r = run_header_buffer->type[run_header_buffer->read];
                int first_gtid;
                if (r == RUN_HEADER) {
                    first_gtid = ((RAT::DS::RHDR*) header)->ValidEventID;
                }
                //printf("rh at tail: flushing %i, first_gtid %#x, run_end_idx %i, current idx %i\n", buffer_flushing, first_gtid, run_end_idx, run_header_buffer->read);
                /*
                if (r == AV_STATUS_HEADER)
                    first_gtid = 0; //((CAAC*) header)->first_event_id;
                if (r == MANIPULATOR_STATUS_HEADER)
                    first_gtid = 0; //((CAST*) header)->first_event_id;
                */

                //EventRecord* er_run_end = (EventRecord*) event_buffer->keys[(long)run_end_idx];
                if (ertemp->gtid >= first_gtid) {
                    buffer_pop(run_header_buffer, &r, &header);
                    if (r == RUN_HEADER) {
                        RAT::DS::RHDR* rhdr = (RAT::DS::RHDR*) header;
                        if (run_active && outfile && tree) {
                            outfile->cd();
                            tree->Write();
                            outfile->Close();
                            delete outfile;
                            outfile = NULL;
                            printf("shipper: closed run %i file for soft run start\n", run_id);
                        }
                        run_start_time = clock();
                        run_id = rhdr->RunID;
                        sprintf(filename, "run_%i.root", run_id);
                        printf("shipper: starting new run: id %i, gtid %#x (%s)\n", run_id, rhdr->ValidEventID, filename);
                        //rec = new RAT::DS::PackedRec();
                        rec = new RAT::DS::PackedEvent();
                        //tree->Branch("PackRec", rec->ClassName(), &rec, 32000, 99);
                        tree = new TTree("PackT", "RAT Tree");
                        outfile = new TFile(filename, "recreate");
                        tree->Branch("PackEv", rec->ClassName(), &rec, 32000, 99);
                        tree->SetAutoSave(10 * 1024 * 1024); // 10 MiB
                        //rec->RecordType = (int) r;
                        //rec->Rec = (RAT::DS::RHDR*) header;
                        //tree->Fill();
                        dispatcher->sendObject(rhdr);
                    }
                    else if (r == AV_STATUS_HEADER) {
                        RAT::DS::CAAC* caac = (RAT::DS::CAAC*) header;
                        //rec->RecordType = (int) r;
                        //rec->Rec = (RAT::DS::CAAC*) header;
                        //tree->Fill();
                        dispatcher->sendObject(caac);
                    }
                    else if (r == MANIPULATOR_STATUS_HEADER) {
                        RAT::DS::CAST* cast = (RAT::DS::CAST*) header;
                        //rec->RecordType = (int) r;
                        //rec->Rec = (RAT::DS::CAST*) header;
                        //tree->Fill();
                        dispatcher->sendObject(cast);
                    }
                    else {
                        printf("shipper: encountered header of unknown type %i in run header buffer\n", r);
                    }
                    free(header);
                }
                else
                    break;
            }
        }

        // hold until we get a run header
        if (!run_active && !outfile) {
            if (!print_rh_waiting) {
                printf("shipper: waiting for run header...\n");
                print_rh_waiting = true;
            }
            //printf("no active run or outfile\n");
            continue;
        }
        print_rh_waiting = false;


        // ship event-level headers
        /*
        while (event_header_buffer->keys[event_header_buffer->read]) {
            void* header = event_header_buffer->keys[event_header_buffer->read];
            RecordType r = event_header_buffer->type[event_header_buffer->read];
            int first_gtid;
            if (r == TRIG_BANK_HEADER)
                first_gtid = ((RAT::DS::TRIG*) header)->EventID;
            if (r == EPED_BANK_HEADER)
                first_gtid = ((RAT::DS::EPED*) header)->EventID;

            if (first_gtid < ertemp->gtid) {
                buffer_pop(event_header_buffer, &r, &header);
                if (r == TRIG_BANK_HEADER) {
                    //rec->RecordType = (int) r;
                    //rec->Rec = (RAT::DS::TRIG*) header;
                    //tree->Fill();
                    //dispatcher->sendObject(rec);
                    dispatcher->sendObject((RAT::DS::TRIG*)header);
                }
                else if (r == EPED_BANK_HEADER) {
                    //rec->RecordType = (int) r;
                    //rec->Rec = (RAT::DS::EPED*) header;
                    //tree->Fill();
                    //dispatcher->sendObject(rec);
                    dispatcher->sendObject((RAT::DS::EPED*)header);
                }
                else {
                    printf("shipper: encountered header of unknown type %i in event header buffer\n", r);
                }
                free(header);
            }
            else
                break;
        }
        */

        if (ertemp && !flush_all_buffers) {
            clock_t time_event = ertemp->arrival_time;
            if((float)(clock() - time_event) / CLOCKS_PER_SEC < QUEUE_DELAY) {
                //printf("gonna hang onto this for a while\n");
                continue;
            }
        }

        // finally, ship the event data
        EventRecord* er;
        RAT::DS::PackedEvent* e;
        RecordType r;

        if (buffer_pop(event_buffer, &r, (void**)&er) == 1) {
            if (!er) {
                printf("shipper: popped null pointer\n"); 
            }
            else {
                //rec->RecordType = (int) DETECTOR_EVENT;
                //rec->Rec = (RAT::DS::GenericRec*) er->event;
                gtid_last_shipped = er->gtid;
                if (er->has_bundles && er->has_mtc) {
                    rec = er->event;
                    if (tree) {
                        tree->Fill();
                    }
                    else {
                        std::cout << "shipper: WARNING: tree unavailable for writing. events lost." << std::endl;
                    }
                    dispatcher->sendObject(rec);
                    //printf("shipper: shipped event with gtid %i\n", er->gtid);
                }
                /*
                   else {
                   if (!er->has_bundles)
                   printf("shipper: gtid %#x has no pmt data\n", er->gtid);
                   if (!er->has_mtc)
                   printf("shipper: gtid %#x has no mtc data\n", er->gtid);
                   if (!er->has_caen)
                   printf("shipper: gtid %#x has no caen data\n", er->gtid);
                   }
                 */
                delete e;
                delete er;
            }
        }
        time_last_shipped = clock();
    }

    delete dispatcher;
    if (outfile)
        delete outfile;
}

