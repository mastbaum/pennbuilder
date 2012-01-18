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
//#include <jemalloc/jemalloc.h>
#include "shipper.h"
#include "ds.h"

extern Buffer* event_buffer;
extern Buffer* event_header_buffer;
extern Buffer* run_header_buffer;

void handler(int signal);

FILE* outfile;

void* shipper(void* ptr)
{
    clock_t time_now;
    clock_t time_last_shipped;
    time_last_shipped = clock();
    outfile = NULL;
    char filename[100];
    int run_active = 0;

    signal(SIGINT, &handler);
    while(1) {
        time_now = clock();
        Event* etemp = (Event*) event_buffer->keys[event_buffer->read];

        // skipped gtid timeout
        if(!etemp) {
            if((float)(time_now - time_last_shipped) / CLOCKS_PER_SEC > 0.5) {
                RecordType rtemp;
                buffer_pop(event_buffer, &rtemp, (void*)&etemp);
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
                RHDR* h = (RHDR*) run_header_buffer->keys[irhdr];
                if(h->first_event_id <= etemp->gtid) {
                    if(run_active)
                        fclose(outfile);
                    int run_id = h->run_id;
                    sprintf(filename, "run_%i.cdab", run_id);
                    printf("Starting new run: ID %i, GTID %i (%s)\n", run_id, h->first_event_id, filename);
                    outfile = fopen(filename, "wb+");
                    run_active = 1;
                }
            }
            irhdr++;
            irhdr %= run_header_buffer->size;
        }

        // hold until we get a run header
        if(!run_active) {
            printf("Waiting on run header...\n");
            continue;
        }

        while(run_header_buffer->keys[run_header_buffer->read]) {
            void* header = run_header_buffer->keys[run_header_buffer->read];
            RecordType r = run_header_buffer->type[run_header_buffer->read];
            int first_gtid;
            if(r == RUN_HEADER)
                first_gtid = ((RHDR*) header)->first_event_id;
            if(r == AV_STATUS_HEADER)
                first_gtid = 0; //((CAAC*) header)->first_event_id;
            if(r == MANIPULATOR_STATUS_HEADER)
                first_gtid = 0; //((CAST*) header)->first_event_id;

            if(first_gtid <= etemp->gtid) {
                buffer_pop(run_header_buffer, &r, &header);
                if(r == RUN_HEADER) {
                    CDABHeader cdh;
                    cdh.record_type = (int) r;
                    cdh.size = sizeof(RHDR);
                    fwrite(&cdh, sizeof(cdh), 1, outfile);
                    RHDR* rhdr = (RHDR*) header;
                    fwrite(rhdr, sizeof(RHDR), 1, outfile);
                }
                else if(r == AV_STATUS_HEADER) {
                    CDABHeader cdh;
                    cdh.record_type = (int) r;
                    cdh.size = sizeof(CAAC);
                    fwrite(&cdh, sizeof(cdh), 1, outfile);
                    CAAC* caac = (CAAC*) header;
                    fwrite(caac, sizeof(CAAC), 1, outfile);
                }
                else if(r == MANIPULATOR_STATUS_HEADER) {
                    CDABHeader cdh;
                    cdh.record_type = (int) r;
                    cdh.size = sizeof(CAST);
                    fwrite(&cdh, sizeof(cdh), 1, outfile);
                    CAST* cast = (CAST*) header;
                    fwrite(cast, sizeof(CAST), 1, outfile);
                }
                else {
                    printf("Encountered header of unknown type %i in run header buffer\n", r);
                    // do something
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
                first_gtid = ((TRIG*) header)->event_id;
            if(r == EPED_BANK_HEADER)
                first_gtid = ((EPED*) header)->event_id;

            if(first_gtid < etemp->gtid) {
                buffer_pop(event_header_buffer, &r, &header);
                if(r == TRIG_BANK_HEADER) {
                    CDABHeader cdh;
                    cdh.record_type = (int) r;
                    cdh.size = sizeof(TRIG);
                    fwrite(&cdh, sizeof(cdh), 1, outfile);
                    TRIG* trig = (TRIG*) header;
                    fwrite(trig, sizeof(TRIG), 1, outfile);
                }
                else if(r == EPED_BANK_HEADER) {
                    CDABHeader cdh;
                    cdh.record_type = (int) r;
                    cdh.size = sizeof(EPED);
                    fwrite(&cdh, sizeof(cdh), 1, outfile);
                    EPED* eped = (EPED*) header;
                    fwrite(eped, sizeof(EPED), 1, outfile);
                }
                else {
                    printf("Encountered header of unknown type %i in event header buffer\n", r);
                    // do something
                }
                free(header);
            }
            else
                break;
        }

        // finally, ship the event data
        Event* e;
        RecordType r;
        if (buffer_pop(event_buffer, &r, (void*)&e) == 1) {
            if (!e) {
                printf("popped null pointer\n"); 
            }
            else {
                printf("popping e: %p, gtid %i\n", e, e->gtid);
                CDABHeader cdh;
                cdh.record_type = DETECTOR_EVENT;
                cdh.size = sizeof(Event);
                fwrite(&cdh, sizeof(CDABHeader), 1, outfile);
                fwrite(e, sizeof(Event), 1, outfile);

                free(e);
            }
        }
        time_last_shipped = clock();
    }
}

