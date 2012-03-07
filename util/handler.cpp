#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <signal.h>
#include <TFile.h>
#include <TTree.h>

#include "ds.h"
#include "listener.h"

extern Buffer* event_buffer;
extern Buffer* event_header_buffer;
extern Buffer* run_header_buffer;

extern TFile* outfile;
extern TTree* tree;
extern RAT::DS::PackedRec* rec;

extern int sockfd;
extern int thread_sockfd[NUM_THREADS];

extern int run_active;

void close_sockets() {
    int i;
    for (i=0; i<NUM_THREADS; i++)
        close(thread_sockfd[i]);
    close(sockfd);
}

void handler(int signal) {
    if (signal == SIGINT) {
        printf("\nCaught CTRL-C (SIGINT), Exiting...\n");
        run_active = 0;

        if (!buffer_isempty(event_buffer)) {
            printf("Warning: exiting with non-empty event buffer\n");
            buffer_status(event_buffer);
        }
        if (!buffer_isempty(event_header_buffer)) {
            printf("Warning: exiting with non-empty event header buffer\n");
            buffer_status(event_header_buffer);
        }
        if (!buffer_isempty(run_header_buffer)) {
            printf("Warning: exiting with non-empty run header buffer\n");
            buffer_status(run_header_buffer);
        }

        printf("Closing sockets...\n");
        close_sockets();

        if (outfile && tree) {
            printf("Closing run file...\n");
            outfile->cd();
            tree->Write();
            outfile->Close();
            delete outfile;
            outfile = NULL;
            delete tree;
            tree = NULL;
        }

        exit(0);
    }
    else
        printf("\nCaught signal %i\n", signal);
}

