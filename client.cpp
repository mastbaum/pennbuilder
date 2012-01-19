#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h> 
#include <signal.h>
#include <jemalloc/jemalloc.h>
#include "ds.h"
#include "listener.h"

#define PER_CHANNEL 50
//#define PER_FEC (32*PER_CHANNEL)
#define PER_FEC (32*50)
#define PER_CRATE (304*PER_FEC)
#define INPUT_FILE "data.txt"

int sockfd;
int last_gtid1[10000];
int last_gtid2[10000];
int last_gtid3[10000];
int last_gtid4[10000];

void error(const char *msg)
{
    perror(msg);
    exit(0);
}

inline void handler(int signal)
{
    if(signal == SIGINT) {
        printf("\nCaught CTRL-C (SIGINT), Exiting...\n");
        close(sockfd);
        exit(0);
    }
    else
        printf("\nCaught signal %i\n", signal);
}

int main(int argc, char *argv[])
{
    int portno, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    if (argc < 3) {
        fprintf(stderr,"usage %s hostname port\n", argv[0]);
        exit(0);
    }
    portno = atoi(argv[2]);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");
    server = gethostbyname(argv[1]);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    memcpy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno);
    if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
        error("ERROR connecting");

    signal(SIGINT, &handler);

    // lets start the magic
    memset((char *) last_gtid1, 0, 10000*sizeof(int));
    memset((char *) last_gtid2, 0, 10000*sizeof(int));
    memset((char *) last_gtid3, 0, 10000*sizeof(int));
    memset((char *) last_gtid4, 0, 10000*sizeof(int));
    FILE *infile;
    srand(time(NULL));
    PMTBundle *channels[19*16*32];
    PMTBundle *fecs[304];
    PMTBundle *xl3; 
    XL3Packet *xl3switch;

    int creadptr[9728],cwriteptr[9728];
    int freadptr[304],fwriteptr[304];
    int sequencer[304];
    int xl3ptr;
    int i,j,k,l;
    char ibuf[10000];
    char *words;
    uint32_t crate,card,channel;
    int fec_id,chan_id;
    uint32_t gtid = 0x0;
    int icrate,ibndl,ifec;
    int current_slot[19];

    infile = fopen(INPUT_FILE,"r");

    // intialize our FEC buffers
    for (i=0;i<9728;i++){
        creadptr[i] = 0;
        cwriteptr[i] = 0;
        channels[i] = (PMTBundle *) malloc(PER_CHANNEL*sizeof(PMTBundle));
    }
    for (i=0;i<10;i++){
        for (j=0;j<9728;j++){
            channels[j][i].word[0] = 0x0;
            channels[j][i].word[1] = 0x0;
            channels[j][i].word[2] = 0x0;
        }
    }
    for (i=0;i<304;i++){
        freadptr[i]=0;
        fwriteptr[i]=0;
        sequencer[i]=0;
        fecs[i] = (PMTBundle *) malloc(PER_FEC*sizeof(PMTBundle));
    }
    for (i=0;i<PER_FEC;i++){
        for (j=0;j<304;j++){
            fecs[j][i].word[0] = 0x0;
            fecs[j][i].word[1] = 0x0;
            fecs[j][i].word[2] = 0x0;
        }
    }
    for (i=0;i<19;i++){
        current_slot[i] = 0;
    }

    xl3  = (PMTBundle *) malloc(PER_CRATE*sizeof(PMTBundle));
    xl3ptr = 0;
    int done = 0;
    int num_events;
    int tot_num_events = 0;
    while(done != 3){

        if (done == 0){
            // lets read in and buffer up a random number of events
            // this first loop represents events going from the pmts into the channel analog buffers
            num_events = rand()%5+1;
            for (i=0;i<num_events;i++){
                if (fgets(ibuf,10000,infile)){
                    tot_num_events++;
                    words = strtok(ibuf, " ");
                    PMTBundle temp_bundle;
                    int num_hits = atoi(words);
                    for (j=0;j<num_hits;j++){
                        words = strtok(NULL, " ");
                        temp_bundle.word[0] = strtoul(words,(char**)NULL,16);
                        words = strtok(NULL, " ");
                        temp_bundle.word[1] = strtoul(words,(char**)NULL,16);
                        words = strtok(NULL, " ");
                        temp_bundle.word[2] = strtoul(words,(char**)NULL,16);

                        crate = (temp_bundle.word[0] & 0x03E00000) >> 21;
                        card = (temp_bundle.word[0] & 0x3C000000) >> 26;
                        channel = (temp_bundle.word[0] & 0x001F0000) >> 16;
                        int gtid = pmtbundle_gtid(&temp_bundle);

                        chan_id = crate*16*32+card*32+channel;
                        if (gtid > last_gtid1[chan_id] || gtid == 0){
                            last_gtid1[chan_id] = gtid;
                        }else{
                            printf("wtf for pmt %d, %d < %d\n",chan_id,gtid,last_gtid1[chan_id]); 
                            continue;
                        }

                        if (chan_id == 9190)
                            continue;
                        channels[chan_id][cwriteptr[chan_id]].word[0] = temp_bundle.word[0];
                        channels[chan_id][cwriteptr[chan_id]].word[1] = temp_bundle.word[1];
                        channels[chan_id][cwriteptr[chan_id]].word[2] = temp_bundle.word[2];
                        cwriteptr[chan_id]++;
                        if (cwriteptr[chan_id] == PER_CHANNEL){
                            cwriteptr[chan_id] = 0;
                        }else if(cwriteptr[chan_id] == creadptr[chan_id]){
                            printf("error writing channel %d, %u %u\n",chan_id,cwriteptr[chan_id],creadptr[chan_id]);
                        }
                    } // end loop over hits
                }else{ // if out of stuff in the file
                    done = 1; // we are done with file
                    break;
                }
            } // end loop over events being buffered into channels
        } // end if done == 0


        if (done < 2){
            // now we have the sequencer loop through a random number of times reading out the channel
            // buffers into the fec fifo buffers
            int ifec;
            for (ifec=0;ifec<304;ifec++){        
                // each fec does its own sequencer readout loop
                card = ifec%16;
                crate = (ifec-card)/16;
                num_events = 5*32+rand()%(5*32)+1; // it goes around up to five full loops
                for (i=0;i<num_events;i++){
                    // each sequencer reads out the channels in order
                    chan_id = ifec*32+sequencer[ifec];
                    if (creadptr[chan_id] != cwriteptr[chan_id]){
                        fecs[ifec][fwriteptr[ifec]].word[0] = channels[chan_id][creadptr[chan_id]].word[0];
                        fecs[ifec][fwriteptr[ifec]].word[1] = channels[chan_id][creadptr[chan_id]].word[1];
                        fecs[ifec][fwriteptr[ifec]].word[2] = channels[chan_id][creadptr[chan_id]].word[2];
                        fwriteptr[ifec]++;
                        int gtid = pmtbundle_gtid(&fecs[ifec][fwriteptr[ifec]]);
                        if (gtid > last_gtid2[chan_id] || gtid == 0)
                            last_gtid2[chan_id] = gtid;
                        else
                            printf("wtf #2 for pmt %d, %d < %d\n",chan_id,gtid,last_gtid2[chan_id]); 

                        if (fwriteptr[ifec] == PER_FEC)
                            fwriteptr[ifec] = 0;
                        else if (fwriteptr[ifec] == freadptr[ifec])
                            printf("error writing card %d, %u %u\n",ifec,fwriteptr[ifec],freadptr[ifec]);
                        creadptr[chan_id]++;
                        if (creadptr[chan_id] == PER_CHANNEL)
                            creadptr[chan_id] = 0;
                    }
                    sequencer[ifec] = (sequencer[ifec]+1)%32;
                } // end loop over number of sequencer reads
            } // end loop over fecs

            if (done == 1){
                for (i=0;i<9728;i++){
                    if (creadptr[i] == cwriteptr[i]){
                        done = 2; // we are done with file and channels
                    }else{
                        done = 1;
                        break;
                    }
                }
            }
        } // end done < 2


        // now we do some readout from the fecs to the xl3 
        uint16_t data_avail;
        uint16_t slot_mask[19];
        int new_current_slot[19];
        for (icrate=0;icrate<19;icrate++){ // loop over crates
            slot_mask[icrate] = 0x0;
            ibndl=0;
            data_avail = 0x0;
            // read out each xl3 fully
            for (j=0;j<16;j++){
                if (freadptr[icrate*16+j] != fwriteptr[icrate*16+j]){
                    data_avail |= 0x1<<j; // fill data available
                }
            }
            while (data_avail != 0x0){
                for (j=0;j<16;j++){
                    if ((0x1<<j) & data_avail){
                        card = icrate*16+j;
                        for (i=0;i<120;i++){
                            if (freadptr[card] == fwriteptr[card]){
                                data_avail &= ~(0x1<<j);
                                break;
                            }else{
                                xl3[xl3ptr].word[0] = fecs[card][freadptr[card]].word[0];
                                xl3[xl3ptr].word[1] = fecs[card][freadptr[card]].word[1];
                                xl3[xl3ptr].word[2] = fecs[card][freadptr[card]].word[2];

                                int gtid = pmtbundle_gtid(&xl3[xl3ptr]);
                                int ccc = pmtbundle_pmtid(&xl3[xl3ptr]); 
                                if (gtid > last_gtid3[ccc] || gtid == 0)
                                    last_gtid3[ccc] = gtid;
                                else
                                    printf("wtf #3 for pmt %d, %d < %d\n",ccc,gtid,last_gtid3[ccc]); 
                                freadptr[card]++;
                                if (freadptr[card] == PER_FEC)
                                    freadptr[card] = 0;
                                xl3ptr++;
                                ibndl++;
                            }
                        } // end loop of 120 bundles per fec
                    } // end if data_avail mask
                } // end loop over slots
                // update data available again
                data_avail = 0x0;
                for (j=0;j<16;j++){
                    if (freadptr[icrate*16+j] != fwriteptr[icrate*16+j]){
                        data_avail |= 0x1<<j; // fill data available
                    }
                }
            } // end while. stop when fecs are empty
        } // loop over crates

        if (done == 2){
                    done = 3;
                    break;
        }
    } // keep doin more events until done
    printf("done\n");

    // ok we are done with the fec buffers
    for (i=0;i<304;i++){
        free(fecs[i]);
    }
    for (i=0;i<9728;i++){
        free(channels[i]);
    }
    // now we have room for our next malloc
    xl3switch = (XL3Packet *) malloc(50000*sizeof(XL3Packet));
    int ipckt,imega;
    ibndl = 0;
    ipckt = 0;
    int num_bundles;
    while (ibndl<xl3ptr){
        if ((xl3ptr-ibndl) >= MEGASIZE)
            num_bundles = MEGASIZE;
        else
            num_bundles = xl3ptr-ibndl;
        xl3switch[ipckt].header.type = 0; 
        xl3switch[ipckt].cmdHeader.num_bundles = num_bundles;
        xl3switch[ipckt].cmdHeader.packet_type = 0xF;
        xl3switch[ipckt].cmdHeader.packet_num = 0xAA;
        PMTBundle *temp_bundle;
        temp_bundle = (PMTBundle *) xl3switch[ipckt].payload;
        for (imega=0;imega<num_bundles;imega++){
            temp_bundle->word[0] = xl3[ibndl].word[0];
            temp_bundle->word[1] = xl3[ibndl].word[1];
            temp_bundle->word[2] = xl3[ibndl].word[2];
            temp_bundle++;
            ibndl++;
        }
        ipckt++;
    }

    free(xl3);

    //now we send packets forever
    j = 0;
    FILE *outfile;
    outfile = fopen("client.txt","w");
    printf("ipckt = %d\n",ipckt);
    while(1){
        for(i=0;i<ipckt;i++){
            uint32_t extra_gtid = (j*tot_num_events)%0xFFFFFF;
            xl3switch[i].cmdHeader.packet_num = extra_gtid&0xFFFF;
            xl3switch[i].cmdHeader.packet_type = (extra_gtid&0xFF0000)>>16;
            if (extra_gtid != 0)
                printf("extra = %u, %u\n",extra_gtid,xl3switch[i].cmdHeader.packet_num+(xl3switch[i].cmdHeader.packet_type<<16));
            //            printf("packet %d: %d, %d, %d\n",i,xl3switch[i].header.type,xl3switch[i].cmdHeader.num_bundles,xl3switch[i].cmdHeader.packet_num);
            num_bundles = xl3switch[i].cmdHeader.num_bundles;
            PMTBundle *temp_bundle;
            temp_bundle = (PMTBundle *) xl3switch[i].payload;
            for (k=0;k<num_bundles;k++){
                int ccc = pmtbundle_pmtid(temp_bundle);
                int gtid = pmtbundle_gtid(temp_bundle) + extra_gtid;
                fprintf(outfile,"%i %i\n",ccc,gtid);
                if (gtid > last_gtid4[ccc] || gtid == 0)
                    last_gtid4[ccc] = gtid;
                else if (gtid < last_gtid4[ccc])
                    printf("wtf #4 pmt %d was %d < %d\n",ccc,gtid,last_gtid4[ccc]);
                //                printf("%08x %08x %08x, %d\n",temp_bundle->word[0],temp_bundle->word[1],temp_bundle->word[2],pmtbundle_pmtid(temp_bundle));
                temp_bundle++;
            }
            int iwhee;
            for(iwhee=0;iwhee<100;iwhee++)
              printf("wheeee\n");
            n = send(sockfd, &xl3switch[i], MAX_BUFFER_LEN, 0);
        }
        j++;
    }
    free(xl3switch);
    fclose(outfile);


    close(sockfd);
    return 0;
}

