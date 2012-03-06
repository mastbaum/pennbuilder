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

extern Buffer* event_buffer;
extern Buffer* event_header_buffer;
extern Buffer* run_header_buffer;

extern unsigned int gtid_last_received;

void handler(int signal);

//extern pthread_mutex_t run_active_mutex;
extern int run_active;

TFile* outfile = NULL;
TTree* tree = NULL;
//RAT::DS::PackedRec* rec = NULL;
RAT::DS::PackedEvent* rec = NULL;

void* shipper(void* ptr)
{
    clock_t time_now;
    clock_t time_last_shipped = clock();
    unsigned int gtid_last_shipped = 0;
    char filename[100];
    bool print_rh_waiting = false;
    avalanche::server* dispatcher = new avalanche::server(DISPATCHER_ADDRESS);
    outfile = NULL;
    tree = NULL;

    signal(SIGINT, &handler);
    while (1) {
        //pthread_mutex_lock(&run_active_mutex);
        //std::cout << "lock ra 1\n";
        bool buffer_flushing = !run_active && (gtid_last_shipped < gtid_last_received);
        if (!run_active && !buffer_flushing) {
            if (outfile && tree) {
                outfile->cd();
                tree->Write();
                outfile->Close();
                delete outfile;
                outfile = NULL;
                printf("shipper: closed run file\n");
            }
            //pthread_mutex_unlock(&run_active_mutex);
            //std::cout << "unlock ra 1a\n";
            continue;
        }
        //pthread_mutex_unlock(&run_active_mutex); 
        //std::cout << "unlock ra 1b\n";

        //unsigned gtid_tail = event_buffer->keys[event_buffer->read] ? ((EventRecord*)(event_buffer->keys[event_buffer->read]))->gtid : 0;
        //printf("tail %i / shipped %i / recv %i\n", gtid_tail, gtid_last_shipped, gtid_last_received);

        time_now = clock();

        //printf("yo\n");
        // skipped gtid timeout
        //pthread_mutex_lock(&event_buffer->mutex_read);
        //std::cout << "lock e 1\n";
        EventRecord* ertemp = (EventRecord*) event_buffer->keys[event_buffer->read];
        if (!ertemp) {
            if ((float)(time_now - time_last_shipped) / CLOCKS_PER_SEC > SKIP_GTID_DELAY) {
                RecordType rtemp;
                buffer_pop(event_buffer, &rtemp, (void**)&ertemp);
                time_last_shipped = clock();
                printf("shipper: skipped missing gtid after timeout\n");
            }
            continue;
        }
        //pthread_mutex_unlock(&event_buffer->mutex_read);
        //std::cout << "unlock e 1\n";

        printf("buffer size: %i, flushing: %s\n", (event_buffer->write-event_buffer->read)%event_buffer->size, buffer_flushing ? "yes" : "no");

        // loop through run headers looking for an RHDR
        if (!buffer_flushing) {
            //buffer_status(run_header_buffer);
            //pthread_mutex_lock(&run_header_buffer->mutex_read);
            //std::cout << "lock rh 1\n";
            int irhdr = run_header_buffer->read;
            while (run_header_buffer->keys[irhdr]) {
                if (run_header_buffer->type[irhdr] == RUN_HEADER) {
                    RAT::DS::RHDR* h = (RAT::DS::RHDR*) run_header_buffer->keys[irhdr];
                    if (h->ValidEventID <= ertemp->gtid) {
                        if (run_active && outfile && tree) {
                            outfile->cd();
                            tree->Write();
                            outfile->Close();
                            delete outfile;
                            outfile = NULL;
                            printf("shipper: closed file for soft run start\n");
                        }
                        int run_id = h->RunID;
                        sprintf(filename, "run_%i.root", run_id);
                        printf("shipper: starting new run: id %i, gtid %i (%s)\n", run_id, h->ValidEventID, filename);
                        //rec = new RAT::DS::PackedRec();
                        rec = new RAT::DS::PackedEvent();
                        //tree->Branch("PackRec", rec->ClassName(), &rec, 32000, 99);
                        tree = new TTree("PackT", "RAT Tree");
                        outfile = new TFile(filename, "recreate");
                        tree->Branch("PackEv", rec->ClassName(), &rec, 32000, 99);
                        tree->SetAutoSave(10 * 1024 * 1024); // 10 MiB
                    }
                }
                irhdr++;
                irhdr %= run_header_buffer->size;
            }
            //pthread_mutex_unlock(&run_header_buffer->mutex_read);
            //std::cout << "unlock rh 1\n";

            // hold until we get a run header
            //pthread_mutex_lock(&run_active_mutex);
            //std::cout << "lock ra 2\n";
            if (!run_active) {
                if (!print_rh_waiting) {
                    printf("shipper: waiting for run header...\n");
                    print_rh_waiting = true;
                }
                //pthread_mutex_unlock(&run_active_mutex);
                //std::cout << "unlock ra 2a\n";
                continue;
            }
            //pthread_mutex_unlock(&run_active_mutex);
            //std::cout << "unlock ra 2b\n";
            print_rh_waiting = false;

            //pthread_mutex_lock(&run_header_buffer->mutex_read);
            //std::cout << "lock rh 2\n";
            while (run_header_buffer->keys[run_header_buffer->read]) {
                void* header = run_header_buffer->keys[run_header_buffer->read];
                RecordType r = run_header_buffer->type[run_header_buffer->read];
                int first_gtid;
                if (r == RUN_HEADER)
                    first_gtid = ((RAT::DS::RHDR*) header)->ValidEventID;
                if (r == AV_STATUS_HEADER)
                    first_gtid = 0; //((CAAC*) header)->first_event_id;
                if (r == MANIPULATOR_STATUS_HEADER)
                    first_gtid = 0; //((CAST*) header)->first_event_id;

                if (first_gtid <= ertemp->gtid) {
                    buffer_pop(run_header_buffer, &r, &header);
                    if (r == RUN_HEADER) {
                        //rec->RecordType = (int) r;
                        //rec->Rec = (RAT::DS::RHDR*) header;
                        //tree->Fill();
                        //dispatcher->sendObject(rec);
                    }
                    else if (r == AV_STATUS_HEADER) {
                        //rec->RecordType = (int) r;
                        //rec->Rec = (RAT::DS::CAAC*) header;
                        //tree->Fill();
                        //dispatcher->sendObject(rec);
                    }
                    else if (r == MANIPULATOR_STATUS_HEADER) {
                        //rec->RecordType = (int) r;
                        //rec->Rec = (RAT::DS::CAST*) header;
                        //tree->Fill();
                        //dispatcher->sendObject(rec);
                    }
                    else {
                        printf("shipper: encountered header of unknown type %i in run header buffer\n", r);
                    }
                }
                else
                    break;

                free(header);
            }
            //pthread_mutex_unlock(&run_header_buffer->mutex_read);
            //std::cout << "unlock rh 2\n";
        }

        // ship event-level headers
        //pthread_mutex_lock(&event_buffer->mutex_read);
        //std::cout << "lock e 2\n";
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
                }
                else if (r == EPED_BANK_HEADER) {
                    //rec->RecordType = (int) r;
                    //rec->Rec = (RAT::DS::EPED*) header;
                    //tree->Fill();
                    //dispatcher->sendObject(rec);
                }
                else {
                    printf("shipper: encountered header of unknown type %i in event header buffer\n", r);
                }
                free(header);
            }
            else
                break;
        }
        //pthread_mutex_unlock(&event_buffer->mutex_read);
        //std::cout << "unlock e 2\n";

        if (ertemp) {
            clock_t time_event = ertemp->arrival_time;
            if((float)(time_now - time_event) / CLOCKS_PER_SEC < QUEUE_DELAY) {
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
                rec = er->event;
                tree->Fill();
                dispatcher->sendObject(rec);
                gtid_last_shipped = er->gtid;
                //printf("shipper: shipped event with gtid %i\n", er->gtid);
                delete e;
                delete er;
            }
        }
        time_last_shipped = clock();
    }

    delete outfile;
}

