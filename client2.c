#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <signal.h>
#include "structures2.h"
#include <arpa/inet.h>



char *name;
int S;

void close_sockets();
void handle_sigint(int nr);
void open_sockets(char*, char*);
void connect_to_server();
void send_m(uint8_t mt, int nr, double val);
void request();

connect_type ct;

int main(int argc, char **argv) {
    if (argc < 4){
        printf("Must have 3 arguments. Name, web/local, address:port/path");
        exit(1);
    }
    if (atexit(close_sockets) == -1) {
        printf("Error while setting atexit function\n");
    }

    name = argv[1];
    signal(SIGINT, handle_sigint);



    open_sockets(argv[2], argv[3]);
    connect_to_server();

    uint8_t mt;
    while(1){
        if(read(S, &mt, 1) != 1){
            printf("Error while reading message type\n");
            exit(1);
        }

        if(mt == PING){
            printf("Received PING message. Sending back\n");
            send_m(PING, 0, 0);
        } else if(mt == REQUEST) {
            printf("Received request. Calculating\n");
            request();
        } else {
            printf("Received unknown message\n");
        }
    }

}

void send_m(uint8_t mt, int nr, double val){
    message_t m;
    m.mt = (message_type)mt;
    snprintf(m.name, 32, "%s", name);
    m.ct = ct;
    m.nr = nr;
    m.val = val;
    if(write(S, &m, sizeof(message_t)) != sizeof(message_t)){
        printf("Error while sending message type\n");
        exit(1);
    }
    printf("Message send!\n");
}

void request(){
    operation o;
    char buf[256];

    if(read(S, &o, sizeof(operation)) != sizeof(operation)){
        printf("Error while reading request message\n");
        exit(1);
    }

    int nr = o.nr;
    double val = 0;

    sprintf(buf, "echo 'scale=6 %lf %c %lf' | bc", o.a, o.op, o.b);
    FILE *calc = popen(buf, "r");
    size_t n = fread(buf, 1, 256, calc);
    pclose(calc);

    buf[n-1] = '\0';
    sscanf(buf, "%lf", &val);

    send_m(RESULT, nr, val);
}



void connect_to_server(){
    send_m(CONNECT, 0, 0);

    uint8_t mt;
    if(read(S, &mt, 1) != 1){
        printf("Error while reading message type\n");
        exit(1);
    }

    switch(mt){
        case BADNAME:
            printf("Chosen name is already connected\n");
            exit(1);
        case FULL:
            printf("Server is full\n");
            exit(1);
        case SUCCESS:
            printf("Connected successfully!\n");
            break;
        default:
            printf("Somethin went wrong\n");
    }
}

void open_sockets(char *arg2, char *arg3){
    if(strcmp(arg2, "web") == 0){
        strtok(arg3, ":");
        char *p = strtok(NULL, ":");
        //printf("%s\n", arg3);
        if(p == NULL){
            printf("Error - no port given\n");
            exit(1);
        }

        uint32_t ip = inet_addr(arg3);
        if(ip == -1){
            printf("Error - wrong ip address\n");
            exit(1);
        }

        uint16_t port = (uint16_t) strtol(p, NULL, 10);
        if(port < 1024){
            printf("Error - port must be larger than 1024");
            exit(1);
        }

        S = socket(AF_INET, SOCK_DGRAM, 0);
        if(S < 0){
            printf("Error while creating web socket\n");
            exit(1);
        }

        struct sockaddr_in address_web;
        address_web.sin_family = AF_INET;
        address_web.sin_addr.s_addr = htonl(INADDR_ANY);
        address_web.sin_port = 0;

        if(connect(S, (const struct sockaddr *)&address_web, sizeof(address_web)) == -1){
            printf("Error while connecting to web socket\n");
            exit(1);
        }


        address_web.sin_family = AF_INET;
        address_web.sin_addr.s_addr = ip;
        address_web.sin_port = htons(port);

        if(connect(S, (const struct sockaddr *)&address_web, sizeof(address_web)) == -1){
            printf("Error while connecting to web socket\n");
            exit(1);
        }

        ct = WEB;

    } else if(strcmp(arg2,"local") == 0){
        char *path = arg3;
        if(strlen(path) < 1 || strlen(path) > UNIX_PATH_MAX){
            printf("Wrong unix path\n");
            exit(1);
        }

        struct sockaddr_un address_local;
        address_local.sun_family = AF_UNIX;
        snprintf(address_local.sun_path, UNIX_PATH_MAX, "%s", path);

        S = socket(AF_UNIX, SOCK_DGRAM, 0);
        if(S < 0){
            printf("Error while creating local socket\n");
            exit(1);
        }

        if(bind(S, (const struct sockaddr *)&address_local, sizeof(address_local))){
            printf("Error while binding to local socket\n");
            perror("CO JEST: ");
            exit(1);
        }

        if(connect(S, (const struct sockaddr *)&address_local, sizeof(address_local)) == -1){
            printf("Error while connecting to local socket\n");
            perror("CO JEST: ");
            exit(1);
        }

        ct = LOCAL;
    } else {
        printf("Second argument must be local or web\n");
        exit(1);
    }
}

void close_sockets(){
    send_m(DISCONNECT, 0, 0);
    if(close(S) == -1)
        printf("Error while closing socket\n");
}

void handle_sigint(int nr){
    printf("\nReceived SIGINT - ending program\n");
    exit(0);
}