/*
* taghost.c -- player 1 and host of the local network tag game
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

#include <signal.h>

#include <ncurses.h>

#include <time.h>

#define PORT "3490"
#define MAXDATASIZE 50 // max number of bytes we can get at once
#define ROWS 20
#define COLS 50

/*
* keeps errno consistent while reaping children w/o any blocking to avoid zombies
*/
void sigchld_handler(int s){
    (void)s; // quiet unused variable warning

    // waitpid() might overwrite errno, so we save and restore it:
    int saved_errno = errno;

    while(waitpid(-1, NULL, WNOHANG) > 0);

    errno = saved_errno;
}

void *get_in_addr(struct sockaddr *sa)
{
    if(sa->sa_family == AF_INET){
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int getListener(void){
    struct addrinfo hints, *res, *p;
    int rv;
    int sockfd;
    int yes=1;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if((rv = getaddrinfo(NULL, PORT, &hints, &res)) != 0){
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return -1;
    }

    for(p = res; p != NULL; p = p->ai_next){
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1){
            perror("setsockopt");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1){
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    if (p == NULL){
        // if nothing bindable in servinfo you end up here
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    freeaddrinfo(res);

    if (listen(sockfd, 10) == -1){
        perror("listen");
        exit(1);
    }

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

int main(int argc, char* argv[]) {
    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    int newfd, numbytes;
    char s[INET6_ADDRSTRLEN]; // connections address
    fd_set readfds;
    struct timeval tv;

    u_int32_t p1buf[2];
    u_int32_t p2buf[MAXDATASIZE];
    u_int32_t p1r = 1;
    u_int32_t p1c = 1;
    int direction;
    int score;

    char hostname[20];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        printf("Hostname: %s\n", hostname);
    }

    int listener = getListener();
    if (listener == -1) {
        printf("listener descriptor failed: %d", listener);
    }

    struct sigaction sa;
    sa.sa_handler = sigchld_handler;   // reap all daad processes when receive SIGCHLD (a child terminated)
    sigemptyset(&sa.sa_mask);          // "don't block anything extra during this handler, let those other signals thru"
    sa.sa_flags = SA_RESTART;          // restart a system call if interupted by this signal
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    printf("server: waiting for connections...\n");

    while (1) {
        sin_size = sizeof their_addr;
        newfd = accept(listener, (struct sockaddr *)&their_addr, &sin_size);
        if (newfd == -1) {
            perror("accept");
            continue;
        }

        inet_ntop(their_addr.ss_family,
                  get_in_addr((struct sockaddr *)&their_addr),
                  s, sizeof s);
        printf("server: got connection from %s\n", s);

        if (fork() == 0) { // child process
            time_t start_time = time(NULL);
            initscr();              // start ncurses
            cbreak();               // disable line buffering
            noecho();               // dont echo input
            keypad(stdscr, TRUE);   // enable arrow keys
            nodelay(stdscr, TRUE); 

            p1buf[0] = htonl(p1r);
            p1buf[1] = htonl(p1c);

            if (send(newfd, p1buf, sizeof p1buf, 0) == -1) {
                perror("send");
            }

            if ((numbytes = recv(newfd, p2buf, MAXDATASIZE-1, 0)) == -1) {
                perror("recv");
                exit(1);
            }

            printBoard(p1r, p1c, ntohl(p2buf[0]), ntohl(p2buf[1]), 0);

            while (1) {
                direction = getch();
                if (direction == KEY_UP && p1r-1 > 0) {
                    p1r -= 1;
                } else if (direction == KEY_DOWN && p1r+1 < ROWS-1) {
                    p1r += 1;
                } else if (direction == KEY_LEFT && p1c-1 > 0) {
                    p1c -= 1;
                } else if (direction == KEY_RIGHT && p1c+1 < COLS-1) {
                    p1c += 1;
                }

                p1buf[0] = htonl(p1r);
                p1buf[1] = htonl(p1c);

                if (send(newfd, p1buf, sizeof p1buf, 0) == -1) {
                    perror("send");
                    break;
                }

                FD_ZERO(&readfds);
                FD_SET(newfd, &readfds);
                tv.tv_sec = 0;
                tv.tv_usec = 10000;

                int rv = select(newfd+1, &readfds, NULL, NULL, &tv);
                if (rv == -1) {
                    perror("select");
                    break;
                } else if (rv > 0 && FD_ISSET(newfd, &readfds)) {
                    if ((numbytes = recv(newfd, p2buf, sizeof(p2buf), 0)) <= 0) {
                        if (numbytes == 0) {
                            mvprintw(ROWS/2, COLS/2-5, "Client disconnected");
                        } else {
                            perror("recv");
                        }
                        break;
                    }
                }

                time_t now = time(NULL);
                int elapsed = (int) difftime(now, start_time);

                if(p1r == ntohl(p2buf[0]) && p1c == ntohl(p2buf[1])){
                    clear();
                    score = elapsed;
                    while((direction = getch()) != 'q'){
                        mvprintw(ROWS/2, COLS/5, "\nPlayer 1 caught Player 2\nScore: %d seconds\nPress 'q' to quit", score);
                    }
                    endwin(); // end ncurses
                    return 0;
                } else{
                    printBoard(p1r, p1c, ntohl(p2buf[0]), ntohl(p2buf[1]), elapsed);
                }
            }
        }
        endwin();
    }
    return 0;
}
