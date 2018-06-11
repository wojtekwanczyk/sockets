//
// Created by wojtek on 11.06.18.
//

#ifndef ZAD1_STRUCTURES2_H
#define ZAD1_STRUCTURES2_H

#include <stdint.h>
#include <unistd.h>

typedef enum message_type {
    PING,
    REQUEST,
    RESULT,
    SUCCESS,
    CONNECT,
    DISCONNECT,
    FULL,
    BADNAME
} message_type;

typedef enum connect_type{
    WEB,
    LOCAL
} connect_type;

typedef struct message_t{
    message_type mt;
    char name[32];
    connect_type ct;
    int nr;
    double val;
}message_t;



typedef struct client {
    struct  sockaddr* sockaddr;
    socklen_t socklen;
    connect_type ct;
    char *name;
    uint8_t not_active;
} client;


typedef struct operation {
    int nr;
    char op;
    double a;
    double b;
} operation;

typedef struct result {
    int nr;
    double val;
} result;

#define UNIX_PATH_MAX 108
#define CLIENT_MAX 20

#endif //ZAD1_STRUCTURES2_H
