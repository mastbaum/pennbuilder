#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <signal.h>
#include <jemalloc/jemalloc.h>
#include "ds.h"
#include "listener.h"

#define PER_FEC 2000

int sockfd;

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
	/*
    int ipmt, igtid;
    for(igtid=0; igtid<3; igtid++)
        for(ipmt=0; ipmt<5; ipmt++) {
            XL3Packet* xl3p = malloc(sizeof(XL3Packet));
            xl3p->header.type = 0;
            xl3p->cmdHeader.num_bundles = 1;

            PMTBundle b;
            b.word[0] = 0;
            b.word[1] = 65535;
            b.word[2] = ipmt;

            PMTBundle* a = (PMTBundle*) &(xl3p->payload);
            *a = b;

            n = send(sockfd, xl3p, sizeof(XL3Packet), 0);
            if (n < 0) {
                error("ERROR writing to socket");
                break;
            }
        }
 */

	// lets start the magic
	FILE *infile;
	srand(time(NULL));
	PMTBundle *fecs[304];
	PMTBundle *xl3; 
	XL3Packet *xl3switch;

	int readptr[304],writeptr[304];
	int xl3ptr;
	int i,j,k,l;
	char ibuf[10000];
	char *words;
	uint32_t crate,card,channel;
	int fec_id;
	uint32_t gtid = 0x0;

	infile = fopen("data.txt","r");

	// intialize our FEC buffers
	for (i=0;i<304;i++){
		readptr[i]=0;
		writeptr[i]=0;
		fecs[i] = (PMTBundle *) malloc(PER_FEC*sizeof(PMTBundle));
	}
	for (i=0;i<PER_FEC;i++){
		for (j=0;j<304;j++){
			fecs[j][i].word[0] = 0x0;
			fecs[j][i].word[1] = 0x0;
			fecs[j][i].word[2] = 0x0;
		}
	}

	xl3  = (PMTBundle *) malloc(304*PER_FEC*sizeof(PMTBundle));
	xl3ptr = 0;
	int done = 0;
	while(done == 0){

		// lets read in and buffer up a random number of events
		int num_events = rand()%10+1;
		for (i=0;i<num_events;i++){
			if (fgets(ibuf,10000,infile)){
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
					fec_id = crate*16+card;
					fecs[fec_id][writeptr[fec_id]].word[0] = temp_bundle.word[0];
					fecs[fec_id][writeptr[fec_id]].word[1] = temp_bundle.word[1];
					fecs[fec_id][writeptr[fec_id]].word[2] = temp_bundle.word[2];
					writeptr[fec_id]++;
					if (writeptr[fec_id] == PER_FEC){
						writeptr[fec_id] = 0;
					}else if(writeptr[fec_id] == readptr[fec_id]){
						printf("error card %d, %u %u\n",fec_id,writeptr[fec_id],readptr[fec_id]);
					}
				} // end loop over hits
			}else{
				done = 1; // we will be done after this time through
				break; // we are done with the file
			}
		} // end loop over events being buffered

		// now lets do some readout
		uint16_t slot_mask;
		uint32_t crate_mask;
		crate_mask = 0x0;
		while(crate_mask != 0x7FFFF){
			for (i=0;i<19;i++){ // loop over crates
				j = 0;
				while(j<MEGASIZE){ // each crate sends one packet 
					for (k=0;k<16;k++){ // loop over slots
						l = j;
						while(l<MEGASIZE){ // at most 120 from each slot
							card = i*16+k;
							if (readptr[card] != writeptr[card]){
								xl3[xl3ptr].word[0] = fecs[card][readptr[card]].word[0];
								xl3[xl3ptr].word[1] = fecs[card][readptr[card]].word[1];
								xl3[xl3ptr].word[2] = fecs[card][readptr[card]].word[2];
								readptr[card]++;
								xl3ptr++;
								j++;
								l++;
							}else{
								slot_mask |= (0x1<<k); 
								l = 999;
							}
						} // 120 bundle loop for one slot
					} // end loop over slots
					if (slot_mask == 0xFFFF){
						crate_mask |= (0x1<<i);
						break;
					}
				} // loop over 120 bundles per crate
			} // loop over crates
		} // loop until all empty
	} // keep doin more events until done
	
	// ok we are done with the fec buffers
	for (i=0;i<304;i++){
		free(fecs[i]);
	}
	// now we have room for our next malloc
	xl3switch = (XL3Packet *) malloc(50000*sizeof(XL3Packet));
	int ibndl,ipckt,imega;
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
		}
		ipckt++;
		ibndl+=num_bundles;
	}

	free(xl3);

	//now we send packets forever
	while(1){
		for(i=0;i<ipckt;i++){
			num_bundles = xl3switch[i].cmdHeader.num_bundles;
            n = send(sockfd, &xl3switch[i], MAX_BUFFER_LEN, 0);
		}
	}
	free(xl3switch);

   
    close(sockfd);
    return 0;
}

