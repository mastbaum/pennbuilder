#include <queue>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <signal.h>
#include <TFile.h>
#include <TTree.h>

#include <ds.h>
#include <listener.h>
#include <PackedEvent.hh>

extern Buffer<EventRecord*>* event_buffer;
extern std::deque<RAT::DS::GenericRec*> event_header_buffer;
extern std::deque<RAT::DS::GenericRec*> run_header_buffer;

extern TFile* outfile;
extern TTree* tree;

/*
extern int sockfd;
extern int thread_sockfd[NUM_THREADS];

void close_sockets() {
    int i;
    for (i=0; i<NUM_THREADS; i++)
        close(thread_sockfd[i]);
    close(sockfd);
}
*/

void handler(int signal) {
    if (signal == SIGINT) {
        printf("\nCaught CTRL-C (SIGINT), Exiting...\n");

        if (event_buffer->read != event_buffer->write) {
            printf("warning: exiting with non-empty event buffer\n");
            printf("r=%lu, w=%lu, queued: %lu\n", event_buffer->read, event_buffer->write, (event_buffer->write - event_buffer->read) % event_buffer->size);
        }
        if (!event_header_buffer.empty()) {
            printf("warning: exiting with non-empty event header buffer\n");
            printf("queued: %lu\n", event_header_buffer.size());
        }
        if (!run_header_buffer.empty()) {
            printf("warning: exiting with non-empty run header buffer\n");
            printf("queued: %lu\n", run_header_buffer.size());
        }

        printf("Closing sockets...\n");
        //close_sockets();

        if (outfile) {
            printf("Closing run file...\n");
            outfile->cd();
            tree->Write();
            outfile->Close();
            delete outfile;
            outfile = NULL;
        }

        exit(0);
    }
    else
        printf("\nCaught signal %i\n", signal);
}

