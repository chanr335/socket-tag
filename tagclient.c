/*
** tagclient.c 
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <arpa/inet.h>

#define PORT "3490" // the port client will be connecting to
#define ROWS 20
#define COLS 30

#define MAXDATASIZE 50 // max number of bytes we can get at once

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa){
    if (sa ->sa_family == AF_INET){
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void printBoard(int p1r, int p1c, int p2r, int p2c){
    int matrix[ROWS][COLS];

    printf("printboard: %d, %d, %d, %d\n", p1r, p1c, p2r, p2c);
    
    for(int r = 0; r < ROWS; r++){
        for(int c = 0; c < COLS; c++){
            if(r == 0 || r == ROWS -1 || c == 0 || c == COLS - 1){
                printf("#");
            } else if(r == p1r && c == p1c){
                printf("1");
            } else if(r == p2r && c == p2c){
                printf("2");
            } else{
                printf(" ");
            }
        }
        printf("\n");
    }
}

int main(int argc, char *argv[]){
    int sockfd, numbytes;
    u_int32_t p1buf[MAXDATASIZE];
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];

    if(argc != 2){
        fprintf(stderr, "usage: client hostname\n");
        exit(1);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(argv[1], PORT, &hints, &servinfo)) != 0){
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next){
        if((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1){
            perror("client: socket");
            continue;
        }

        inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
        printf("client: attempting conenction to %s\n", s);

        if(connect(sockfd, p->ai_addr, p->ai_addrlen) == -1){
            perror("client: connect");
            close(sockfd);
            continue;
        }

        break;
    }

    if(p == NULL){
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
    printf("client: connected to %s\n", s);

    freeaddrinfo(servinfo);

    while(1){
        if ((numbytes = recv(sockfd, p1buf, MAXDATASIZE-1, 0)) == -1){
            perror("recv");
            exit(1);
        }

        // p1buf[numbytes] = '\0';
        printf("p1buf: %d, %d\n", ntohl(p1buf[0]), ntohl(p1buf[1]));
        printBoard(ntohl(p1buf[0]), ntohl(p1buf[1]), 10, 10);
        // close(sockfd);
    }
    return 0;
}
