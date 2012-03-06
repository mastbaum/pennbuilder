#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <limits.h>
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

void handler(int signal);

pthread_mutex_t writer_mutex;
TFile* outfile = NULL;
TTree* tree = NULL;
//RAT::DS::PackedRec* rec = NULL;
RAT::DS::PackedEvent* rec = NULL;
int run_active = 0;

void* shipper(void* ptr)
{
    pthread_mutex_init(&writer_mutex, NULL);
    clock_t time_now;
    clock_t time_last_shipped;
    time_last_shipped = clock();
    char filename[100];
    avalanche::server* dispatcher = new avalanche::server(DISPATCHER_ADDRESS);
    outfile = NULL;
    tree = NULL;
    //rec = new RAT::DS::PackedRec();
    rec = new RAT::DS::PackedEvent();
    
    signal(SIGINT, &handler);
    while (1) {
        pthread_mutex_lock(&writer_mutex);
        if (!run_active) {
            pthread_mutex_unlock(&writer_mutex);
            continue;
        }
        pthread_mutex_unlock(&writer_mutex);

        time_now = clock();
        EventRecord* ertemp = (EventRecord*) event_buffer->keys[event_buffer->read];

        // skipped gtid timeout
        if (!ertemp) {
            if ((float)(time_now - time_last_shipped) / CLOCKS_PER_SEC > 0.01) {
                RecordType rtemp;
                buffer_pop(event_buffer, &rtemp, (void**)&ertemp);
		time_last_shipped = clock();
                printf("shipper: skipped missing gtid after timeout\n");
            }
            continue;
        }

        // loop through run headers looking for an RHDR
        int irhdr = run_header_buffer->read;
        while (run_header_buffer->keys[irhdr]) {
            if (run_header_buffer->type[irhdr] == RUN_HEADER) {
                RAT::DS::RHDR* h = (RAT::DS::RHDR*) run_header_buffer->keys[irhdr];
                if (h->FirstEventID <= ertemp->gtid) {
                    pthread_mutex_lock(&writer_mutex);
                    if (run_active && outfile && tree) {
                        outfile->cd();
                        tree->Write();
                        outfile->Close();
			delete outfile;
			outfile = NULL;
                        delete tree;
                        tree = NULL;
                    }
                    int run_id = h->RunID;
                    sprintf(filename, "run_%i.root", run_id);
                    printf("Starting new run: ID %i, GTID %i (%s)\n", run_id, h->FirstEventID, filename);
                    outfile = new TFile(filename, "recreate");
                    tree = new TTree("PackT", "RAT Tree");
                    //tree->Branch("PackRec", rec->ClassName(), &rec, 32000, 99);
                    tree->Branch("PackEv", rec->ClassName(), &rec, 32000, 99);
                    tree->SetAutoSave(10*1024*1024); // 10 MiB
                    run_active = 1;
                    pthread_mutex_unlock(&writer_mutex);
                }
            }
            irhdr++;
            irhdr %= run_header_buffer->size;
        }

        // hold until we get a run header
        bool print_rh_waiting = false;
        pthread_mutex_lock(&writer_mutex);
        if (!run_active) {
            if (!print_rh_waiting) {
                printf("shipper: waiting for run header...\n");
                print_rh_waiting = true;
            }
            pthread_mutex_unlock(&writer_mutex);
            continue;
        }
        pthread_mutex_unlock(&writer_mutex);

        while (run_header_buffer->keys[run_header_buffer->read]) {
            void* header = run_header_buffer->keys[run_header_buffer->read];
            RecordType r = run_header_buffer->type[run_header_buffer->read];
            int first_gtid;
            if (r == RUN_HEADER)
                first_gtid = ((RAT::DS::RHDR*) header)->FirstEventID;
            if (r == AV_STATUS_HEADER)
                first_gtid = 0; //((CAAC*) header)->first_event_id;
            if (r == MANIPULATOR_STATUS_HEADER)
                first_gtid = 0; //((CAST*) header)->first_event_id;

            if (first_gtid <= ertemp->gtid) {
                pthread_mutex_lock(&writer_mutex);
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
                free(header);
                pthread_mutex_unlock(&writer_mutex);
            }
            else
                break;
        }

        // ship event-level headers
        if (ertemp) {
	    clock_t time_event = ertemp->arrival_time;
            if((float)(time_now - time_event) / CLOCKS_PER_SEC < QUEUE_DELAY) {
                continue;
            }
        }

        while (event_header_buffer->keys[event_header_buffer->read]) {
            void* header = event_header_buffer->keys[event_header_buffer->read];
            RecordType r = event_header_buffer->type[event_header_buffer->read];
            int first_gtid;
            if (r == TRIG_BANK_HEADER)
                first_gtid = ((RAT::DS::TRIG*) header)->EventID;
            if (r == EPED_BANK_HEADER)
                first_gtid = ((RAT::DS::EPED*) header)->EventID;

            if (first_gtid < ertemp->gtid) {
                pthread_mutex_lock(&writer_mutex);
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
                pthread_mutex_unlock(&writer_mutex);
            }
            else
                break;
        }

        // finally, ship the event data
        EventRecord* er;
        RAT::DS::PackedEvent* e;
        RecordType r;

        pthread_mutex_lock(&writer_mutex);
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
                //printf("shipper: shipped event with gtid %i\n", er->gtid);
                delete e;
                delete er;
            }
        }
        time_last_shipped = clock();
        pthread_mutex_unlock(&writer_mutex);
    }

    delete outfile;
    delete tree;
    delete rec;
}

