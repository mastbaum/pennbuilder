#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <signal.h>
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
    while(1) {
        PMTBundle* b = malloc(sizeof(PMTBundle));
        b->word1 = 12345;
        b->word2 = 65535;
        b->word3 = 42;

        n = send(sockfd,b,sizeof(PMTBundle),0);
        if (n < 0) {
            error("ERROR writing to socket");
            break;
        }

        free(b);
    }
    close(sockfd);
    return 0;
}

