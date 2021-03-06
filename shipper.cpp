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

TFile* outfile;
TTree* tree;
RAT::DS::PackedRec* rec;

void* shipper(void* ptr)
{
    clock_t time_now;
    clock_t time_last_shipped;
    time_last_shipped = clock();
    char filename[100];
    int run_active = 0;
    avalanche::server* dispatcher = new avalanche::server(DISPATCHER_ADDRESS);
    outfile = NULL;
    tree = NULL;
    rec = new RAT::DS::PackedRec();
    
    signal(SIGINT, &handler);
    while(1) {
        time_now = clock();
        RAT::DS::PackedEvent* etemp = (RAT::DS::PackedEvent*) event_buffer->keys[event_buffer->read];

        // skipped gtid timeout
        if(!etemp) {
            if((float)(time_now - time_last_shipped) / CLOCKS_PER_SEC > 0.5) {
                RecordType rtemp;
                buffer_pop(event_buffer, &rtemp, (void**)&etemp);
		time_last_shipped = clock();
                printf("Skipped gtid after timeout\n");
            }
            continue;
        }

        if(etemp) {
	    clock_t time_event = etemp->builder_arrival_time;
            if((float)(time_now - time_event) / CLOCKS_PER_SEC < 1) {
                continue;
            }
        }

        // loop through run headers looking for an RHDR
        int irhdr = run_header_buffer->read;
        while(run_header_buffer->keys[irhdr]) {
            if(run_header_buffer->type[irhdr] == RUN_HEADER) {
                RAT::DS::RHDR* h = (RAT::DS::RHDR*) run_header_buffer->keys[irhdr];
                if(h->FirstEventID <= etemp->gtid) {
                    if(run_active) {
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
                    tree->Branch("PackRec", rec->ClassName(), &rec, 32000, 99);
                    tree->SetAutoSave(10*1024*1024); // 10 MiB
                    run_active = 1;
                }
            }
            irhdr++;
            irhdr %= run_header_buffer->size;
        }

        // hold until we get a run header
        if(!run_active) {
            printf("Waiting for run header...\n");
            continue;
        }

        while(run_header_buffer->keys[run_header_buffer->read]) {
            void* header = run_header_buffer->keys[run_header_buffer->read];
            RecordType r = run_header_buffer->type[run_header_buffer->read];
            int first_gtid;
            if(r == RUN_HEADER)
                first_gtid = ((RAT::DS::RHDR*) header)->FirstEventID;
            if(r == AV_STATUS_HEADER)
                first_gtid = 0; //((CAAC*) header)->first_event_id;
            if(r == MANIPULATOR_STATUS_HEADER)
                first_gtid = 0; //((CAST*) header)->first_event_id;

            if(first_gtid <= etemp->gtid) {
                buffer_pop(run_header_buffer, &r, &header);
                if(r == RUN_HEADER) {
                    rec->RecordType = (int) r;
                    rec->Rec = (RAT::DS::RHDR*) header;
                    tree->Fill();
                    dispatcher->sendObject(rec);
                }
                else if(r == AV_STATUS_HEADER) {
                    rec->RecordType = (int) r;
                    rec->Rec = (RAT::DS::CAAC*) header;
                    tree->Fill();
                    dispatcher->sendObject(rec);
                }
                else if(r == MANIPULATOR_STATUS_HEADER) {
                    rec->RecordType = (int) r;
                    rec->Rec = (RAT::DS::CAST*) header;
                    tree->Fill();
                    dispatcher->sendObject(rec);
                }
		else {
                    printf("Encountered header of unknown type %i in run header buffer\n", r);
		}
                free(header);
            }
            else
                break;
        }

        // ship event-level headers
        while(event_header_buffer->keys[event_header_buffer->read]) {
            void* header = event_header_buffer->keys[event_header_buffer->read];
            RecordType r = event_header_buffer->type[event_header_buffer->read];
            int first_gtid;
            if(r == TRIG_BANK_HEADER)
                first_gtid = ((RAT::DS::TRIG*) header)->EventID;
            if(r == EPED_BANK_HEADER)
                first_gtid = ((RAT::DS::EPED*) header)->EventID;

            if(first_gtid < etemp->gtid) {
                buffer_pop(event_header_buffer, &r, &header);
                if(r == TRIG_BANK_HEADER) {
                    rec->RecordType = (int) r;
                    rec->Rec = (RAT::DS::TRIG*) header;
                    tree->Fill();
                    dispatcher->sendObject(rec);
                }
                else if(r == EPED_BANK_HEADER) {
                    rec->RecordType = (int) r;
                    rec->Rec = (RAT::DS::EPED*) header;
                    tree->Fill();
                    dispatcher->sendObject(rec);
                }
                else {
                    printf("Encountered header of unknown type %i in event header buffer\n", r);
                }
                free(header);
            }
            else
                break;
        }

        // finally, ship the event data
        RAT::DS::PackedEvent* e;
        RecordType r;
        if (buffer_pop(event_buffer, &r, (void**)&e) == 1) {
            if (!e) {
                printf("popped null pointer\n"); 
            }
            else {
                rec->RecordType = (int) DETECTOR_EVENT;
                rec->Rec = (RAT::DS::GenericRec*) e;
                tree->Fill();
                dispatcher->sendObject(rec);
                delete e;
            }
        }
        time_last_shipped = clock();
    }

    delete outfile;
    delete tree;
    delete rec;
}

