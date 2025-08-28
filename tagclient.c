/*
** tagclient.c 
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <sys/types.h>

#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <ncurses.h>

#include <time.h>

#define PORT "3490" // the port client will be connecting to
#define ROWS 20
#define COLS 50

#define MAXDATASIZE 50 // max number of bytes we can get at once

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa){
    if (sa ->sa_family == AF_INET){
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int findListener(char *hostname){
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(hostname, PORT, &hints, &servinfo)) != 0){
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

    return sockfd;
}

void printBoard(int p1r, int p1c, int p2r, int p2c, int time){
    clear();
    mvprintw(ROWS, 0, "time elapsed: %d", time);
    for(int r = 0; r < ROWS; r++){
        for(int c = 0; c < COLS; c++){
            if(r == 0 || r == ROWS -1 || c == 0 || c == COLS - 1){
                mvprintw(r, c, "#");
            } else if(r == p1r && c == p1c){
                mvprintw(r, c, "1");
            } else if(r == p2r && c == p2c){
                mvprintw(r, c, "2");
            } else{
                mvprintw(r, c, " ");
            }
        }
    }
    refresh();
}
int main(int argc, char *argv[]){
    fd_set readfds;
    struct timeval tv;
    int numbytes;
    u_int32_t p1buf[MAXDATASIZE];
    u_int32_t p2buf[2];
    int direction;
    int p2r = ROWS-2;
    int p2c = COLS-2;
    int score;

    if(argc != 2){
        fprintf(stderr, "usage: client hostname\n");
        exit(1);
    }

    int sockfd = findListener(argv[1]);
    if(sockfd == -1){
        printf("finding listener descriptor failed: %d", sockfd);
    }

    time_t start_time = time(NULL);
    initscr();            // start ncurses
    cbreak();             // disable line buffering
    noecho();             // don't echo input
    keypad(stdscr, TRUE); // enable arrow keys
    nodelay(stdscr, TRUE);  // make getch() non-blocking

    if ((numbytes = recv(sockfd, p1buf, MAXDATASIZE-1, 0)) == -1){
        perror("recv");
        exit(1);
    }

    printf("p1buf: %d, %d\n", ntohl(p1buf[0]), ntohl(p1buf[1]));
    printBoard(ntohl(p1buf[0]), ntohl(p1buf[1]), p2r, p2c, 0);

    p2buf[0] = htonl(p2r);
    p2buf[1] = htonl(p2c);
    if(send(sockfd, p2buf, sizeof p2buf, 0) == -1){
        perror("send");
    }

    while(1){
        direction = getch();
        if(direction == KEY_UP && p2r-1 > 0){
            p2r -= 1;
        }else if(direction == KEY_DOWN && p2r+1 < ROWS-1){
            p2r += 1;
        }else if(direction == KEY_LEFT && p2c-1 > 0){
            p2c -= 1;
        }else if(direction == KEY_RIGHT && p2c+1 < COLS-1){
            p2c += 1;
        }

        p2buf[0] = htonl(p2r);
        p2buf[1] = htonl(p2c);
        if(send(sockfd, p2buf, sizeof p2buf, 0) == -1){
            perror("send");
            break;
        }

        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);

        tv.tv_sec = 0;
        tv.tv_usec = 10000;

        int rv = select(sockfd+1, &readfds, NULL, NULL, &tv);
        if (rv == -1) {
            perror("select");
            break;
        } else if (rv > 0 && FD_ISSET(sockfd, &readfds)) {
            if ((numbytes = recv(sockfd, p1buf, sizeof(p1buf), 0)) <= 0) {
                if (numbytes == 0) {
                    mvprintw(ROWS/2, COLS/2-5, "Server disconnected");
                } else {
                    perror("recv");
                }
                break;
            }
        }

        time_t now = time(NULL);
        int elapsed = (int) difftime(now, start_time);

        if(p2r == ntohl(p1buf[0]) && p2c == ntohl(p1buf[1])){
            clear();
            score = elapsed;
            while((direction = getch()) != 'q'){
                mvprintw(ROWS/2, COLS/5, "\nPlayer 1 caught Player 2\nScore: %d seconds\nPress 'q' to quit", score);
            }
            endwin(); // end ncurses
            return 0;
        } else{
            printBoard(ntohl(p1buf[0]), ntohl(p1buf[1]), p2r, p2c, elapsed);
        }
    }
    endwin();
    return 0;
}
