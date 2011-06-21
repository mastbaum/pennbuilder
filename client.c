#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <signal.h>
#include <jemalloc/jemalloc.h>
#include "ds.h"

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
    int ipmt, igtid;
    for(igtid=0; igtid<3; igtid++)
        for(ipmt=0; ipmt<5; ipmt++) {
            XL3Packet* xl3p = malloc(sizeof(XL3Packet));
            xl3p->header.type = 0;
            xl3p->cmdHeader.num_bundles = 1;

            PMTBundle b;
            b.word[0] = 12345;
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
    
    close(sockfd);
    return 0;
}

