//
// Created by wwanczyk on 09.06.18.
//

#ifndef ZAD1_STRUCTURES_H
#define ZAD1_STRUCTURES_H

#include <stdint.h>

typedef enum message_t {
    PING,
    PONG,
    REQUEST,
    RESULT,
    SUCCESS,
    CONNECT,
    DISCONNECT,
    FULL,
    BADNAME
} message_t;

typedef struct client {
    int fd;
    char *name;
    uint8_t un_active;
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

#endif //ZAD1_STRUCTURES_H
