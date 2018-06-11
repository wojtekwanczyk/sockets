#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <stdint.h>
#include <memory.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/epoll.h>
#include <pthread.h>
#include <unistd.h>
#include "structures.h"




char *unix_path;
int socket_web;
int socket_local;
uint16_t  port_number;
int epoll;

pthread_t ping;
pthread_t console;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

client clients[CLIENT_MAX];
int client_nr = 0;
int operation_nr = 0;

void handle_sigint(int nr);
void open_sockets();
void close_sockets();
void *ping_task(void *arg);
void *console_task(void *arg);
void remove_socket(int nr);
void remove_client(int nr);
int name_compare(char *name, client *c);
int is_in(void *const a, void *const base, size_t nr, size_t size, __compar_fn_t fun_cmp);
void connect_client(char *name, int socket);
void disconnect_client(char *name);


int main(int argc, char *argv[]) {
    if(argc < 3){
        printf("Must be called: ./program_name port_number unix_path\n");
        exit(1);
    }
    signal(SIGINT, handle_sigint);

    port_number = (uint16_t)strtol(argv[1], NULL, 10);
    if(port_number < 1024){
        printf("Cannot choose port number below 1024 like :%d\n", port_number);
        exit(1);
    }

    unix_path = argv[2];
    if (strlen(unix_path) < 1 || strlen(unix_path) > UNIX_PATH_MAX){
        printf("Wrong length of unix path\n");
    }

    if (atexit(close_sockets) == -1){
        printf("Error while setting atexit function\n");
        exit(1);
    }

    open_sockets();

    // ping thread
    if (pthread_create(&ping, NULL, ping_task, NULL) != 0){
        printf("Error while creating ping task\n");
        exit(1);
    }

    // console thread
    if (pthread_create(&console, NULL, console_task, NULL) != 0){
        printf("Error while creating console task\n");
        exit(1);
    }

    struct epoll_event e;
    while(1){
        if(epoll_wait(epoll, &e, 1, -1)){
            printf("Error while waiting for epoll\n");
            exit(1);
        }

    }

    return 0;
}


void *console_task(void *arg){
    operation msg;
    uint8_t mt = REQUEST;
    int er;
    char buf[256];
    srand((unsigned int)time(NULL));
    while(1){
        printf("Write command:\n");

        if(fgets(buf, sizeof(buf), stdin) == NULL){
            printf("Error while reading fgets\n");
        }

        if(sscanf(buf, "%lf %c %lf", &msg.a, &msg.op, &msg.b) != 3){
            printf("Wrong format. Try again\n");
            continue;
        }
        operation_nr++;
        msg.nr = operation_nr;
        pthread_mutex_lock(&clients_mutex);
        if(client_nr == 0){
            printf("Client list is empty. Wait for clients and try again\n");
            pthread_mutex_unlock(&clients_mutex);
            continue;
        }
        er = 0;
        int x = rand() % client_nr;
        if(write(clients[x].fd, &mt, 1) != 1) er = 1;
        if(write(clients[x].fd, &msg, sizeof(operation)) != sizeof(operation)) er = 1;

        if(er == 1){
            printf("Error while sending request to the client \'%s\'\n", clients[x].name);
        } else {
            printf("Command request nr %d sent successfully to client \'%s\'\n",operation_nr, clients[x].name);
        }
        pthread_mutex_unlock(&clients_mutex);
    }
}


void get_connection(int socket) {
    int client = accept(socket, NULL, NULL);
    if(client == -1){
        printf("Error while accepting client\n");
        exit(1);
    }

    struct epoll_event e;
    e.events = EPOLLIN | EPOLLPRI;
    e.data.fd = client;

    if(epoll_ctl(epoll, EPOLL_CTL_ADD, client, &e)){
        printf("Error while add new client to epoll\n");
        exit(1);
    }
}


void get_message(int socket){
    uint8_t mt;
    uint16_t ms;

    if(read(socket, &mt, 1) != 1){
        printf("Error while reading message type\n");
        exit(1);
    }

    if(read(socket, &ms, 1) != 1){
        printf("Error while reading message size\n");
        exit(1);
    }
    char *name = malloc(ms);

    if(read(socket, name, ms) != ms){
        printf("Error while reading exact message\n");
        exit(1);
    }

    switch(mt){
        case PING:
            pthread_mutex_lock(&clients_mutex);
            int nr = is_in(name, clients, (size_t)client_nr, sizeof(client), (__compar_fn_t)name_compare);
            if(nr >= 0) clients[nr].un_active--;
            pthread_mutex_unlock(&clients_mutex);
            break;
        case RESULT:{
            result r;
            if(read(socket, &r, sizeof(result)) != sizeof(result)){
                printf("Error while reading result\n");
                exit(1);
            }
            printf("Client \'%s\' gave result: %lf", name, r.val);
            break;
        }
        case CONNECT:
            connect_client(name, socket);
            break;
        case DISCONNECT:
            disconnect_client(name);
            break;
    }

}


int is_in(void *const a, void *const base, size_t nr, size_t size, __compar_fn_t fun_cmp) {
    char *p = (char*) base;
    if (nr > 0) {
        for (int i = 0; i < nr; ++i)
            if ((*fun_cmp)(a, (void *) (p + i * size)) == 0) return i;
    }
    return -1;
}

void connect_client(char *name, int socket){
    uint8_t mt;
    pthread_mutex_lock(&clients_mutex);
    if(client_nr == CLIENT_MAX){
        mt = FULL;
        if(write(socket, &mt, 1) != 1){
            printf("Error while sending FULL message to client\n");
            exit(1);
        }
        remove_socket(socket);
    } else {
        int already_in = is_in(name, clients, (size_t)client_nr, sizeof(client), (__compar_fn_t)name_compare);
        if(already_in != -1){
            mt = BADNAME;
            if(write(socket, &mt, 1) != 1){
                printf("Error while writing BADNAME message to client\n");
                exit(1);
            }
        } else {
            clients[client_nr].fd = socket;
            clients[client_nr].name = malloc(strlen(name) + 1);
            clients[client_nr].un_active = 0;
            strcpy(clients[client_nr].name, name);
            client_nr++;
            mt = SUCCESS;
            if(write(socket, &mt, 1) != 1){
                printf("Error while writing SUCCESS message to client\n");
                exit(1);
            }
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void disconnect_client(char *name){
    pthread_mutex_lock(&clients_mutex);
    int i = is_in(name, clients, (size_t)client_nr, sizeof(client), (__compar_fn_t)name_compare);
    if(i >= 0){
        remove_client(i);
        printf("Client /'%s/' has been disconnected\n", name);
    }
    pthread_mutex_unlock(&clients_mutex);
}

int name_compare(char *name, client *c){
    return strcmp(name, c->name);
}



void *ping_task(void *arg){
    uint8_t mtype = PING;
    while(1){
        pthread_mutex_lock(&clients_mutex);

        for(int i = 0; i < client_nr; i++){
            if(write(clients[i].fd, &mtype, 1) != 1){
                printf("Unable to ping %s. Client will be disconnected\n");
                remove_client(i);
                i--;
            }
        }
        pthread_mutex_unlock(&clients_mutex);
        sleep(5);
    }
}

void remove_client(int nr){
    remove_socket(clients[nr].fd);
    free(clients[nr].name);
    client_nr--;
    for(int i=nr; i<client_nr; i++){
        clients[i] = clients[i+1];
    }
}

void remove_socket(int nr){
    if (epoll_ctl(epoll, EPOLL_CTL_DEL, nr, NULL) == -1){
        printf("Error while removing client's socket from epoll\n");
        exit(1);
    }

    if (shutdown(nr, SHUT_RDWR) == -1) {
        printf("Error with shutdown of client's socket\n");
        exit(1);
    }

    if (close(nr) == -1){
        printf("Error while closing client's socket\n");
        exit(1);
    }
}



void open_sockets(){
    // WEB SOCKET
    struct sockaddr_in address_web;
    address_web.sin_family = AF_INET;
    address_web.sin_addr.s_addr = INADDR_ANY;
    address_web.sin_port = htons(port_number);

    socket_web = socket(AF_INET, SOCK_STREAM, 0);
    if(socket_web == -1){
        printf("Error while creating web socket\n");
        exit(1);
    }

    if(bind(socket_web, (const struct sockaddr*)&address_web, sizeof(address_web))){
        printf("Error while binding web socket\n");
        exit(1);
    }

    if(listen(socket_web, CLIENT_MAX)){
        printf("Error while trying to listen on web socket\n");
        exit(1);
    }

    // LOCAL SOCKET
    struct sockaddr_un address_local;
    address_local.sun_family = AF_UNIX;
    snprintf(address_local.sun_path, UNIX_PATH_MAX, "%s", unix_path);

    socket_local = socket(AF_UNIX, SOCK_STREAM, 0);
    if(socket_local == -1){
        printf("Error while creating local socket\n");
        exit(1);
    }

    if(bind(socket_local, (const struct sockaddr*)&address_local, sizeof(address_local))){
        printf("Error while binding local socket\n");
        exit(1);
    }

    if(listen(socket_local, CLIENT_MAX)){
        printf("Error while trying to listen on local socket\n");
        exit(1);
    }

    // EPOLL
    struct epoll_event event;
    event.events = EPOLLIN | EPOLLPRI;

    if ((epoll = epoll_create1(0)) == -1){
        printf("Error while creating epoll\n");
        exit(1);
    }

    event.data.fd = -socket_web;
    if(epoll_ctl(epoll, EPOLL_CTL_ADD, socket_web, &event) == -1){
        printf("Error while adding web socket to epoll\n");
        exit(1);
    }

    event.data.fd = -socket_local;
    if(epoll_ctl(epoll, EPOLL_CTL_ADD, socket_local, &event) == -1){
        printf("Error while adding local socket to epoll\n");
        exit(1);
    }

}

void close_sockets(){
    pthread_cancel(ping);
    pthread_cancel(console);
    if (close(socket_web) == -1)
        printf("Error while closing web socket\n");
    if (close(socket_local) == -1)
        printf("Error while closing local socket\n");
    if (unlink(unix_path) == -1)
        printf("Error with unlinking unix path\n");
    if (close(epoll) == -1)
       printf("Error while closing epoll\n");
}


void handle_sigint(int nr){
    printf("\nReceived SIGINT - ending program\n");
    exit(0);
}