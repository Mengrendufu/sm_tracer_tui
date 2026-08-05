//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#ifndef CONNECTION_STATUS_BAR_H_
#define CONNECTION_STATUS_BAR_H_

#include <stdbool.h>

struct ncplane;

enum {
    CONNECTION_STATUS_ROWS_      = 4U,
    CONNECTION_STATUS_SHORT_LEN_ = 16U,
    CONNECTION_STATUS_PORT_LEN_  = 128U,
    CONNECTION_STATUS_PROTO_LEN_ = 256U
};

struct ConnectionStatusBar {
    struct ncplane *plane;
    bool connected;
    char connection[CONNECTION_STATUS_SHORT_LEN_];
    char port[CONNECTION_STATUS_PORT_LEN_];
    char baud[CONNECTION_STATUS_SHORT_LEN_];
    char dataBits[CONNECTION_STATUS_SHORT_LEN_];
    char stopBits[CONNECTION_STATUS_SHORT_LEN_];
    char parity[CONNECTION_STATUS_SHORT_LEN_];
    char flow[CONNECTION_STATUS_SHORT_LEN_];
    char protocol[CONNECTION_STATUS_PROTO_LEN_];
};

typedef int (*ConnectionStatusBar_ResizeCb)(struct ncplane *plane);

void ConnectionStatusBar_init(struct ConnectionStatusBar *bar);
void ConnectionStatusBar_create(
         struct ConnectionStatusBar *bar,
         struct ncplane *parent,
         void *owner,
         unsigned cols,
         ConnectionStatusBar_ResizeCb resizeCb);
void ConnectionStatusBar_resize(struct ConnectionStatusBar *bar,
                                unsigned cols);
void ConnectionStatusBar_setConnection(struct ConnectionStatusBar *bar,
                                       bool connected);
void ConnectionStatusBar_setSerial(struct ConnectionStatusBar *bar,
                                   char const *port,
                                   char const *baud,
                                   char const *dataBits,
                                   char const *stopBits,
                                   char const *parity,
                                   char const *flow);
void ConnectionStatusBar_setProtocol(struct ConnectionStatusBar *bar,
                                     char const *protocol);

#endif // CONNECTION_STATUS_BAR_H_
