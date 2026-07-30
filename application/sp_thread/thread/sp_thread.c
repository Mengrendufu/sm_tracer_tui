//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
//============================================================================
//=== SpThread runtime: lifecycle and disconnected blocking loop
#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <stdbool.h>
#include "dbc_assert.h"
#include "sp_thread/sp_thread.h"
#include "sp_thread/hsm/sm_sp_thread.h"
DBC_MODULE_NAME("sp_thread")

//============================================================================
//=== Runtime instance

typedef struct {
    pthread_t thread;
    SM_SpThread hsm;
    bool started;
} SpThread;

static SpThread SpThread_inst_;

//============================================================================
//=== Thread entry

static void *SpThread_run_(void * const arg) {
    SpThread * const me = (SpThread *)arg;
    DBC_REQUIRE(200, me != (SpThread *)0);

    SM_SpThread_ctor(&me->hsm);
    SM_SpThread_init(&me->hsm);

    for (;;) {
        int const status = poll((struct pollfd *)0, 0U, -1);
        if ((status < 0) && (errno == EINTR)) {
            continue;
        }
        DBC_ERROR(201);
    }

    return (void *)0;
}

//============================================================================
//=== Lifecycle

int SpThread_start(void) {
    SpThread * const me = &SpThread_inst_;
    DBC_REQUIRE(100, !me->started);

    int const status = pthread_create(&me->thread,
                                      (pthread_attr_t const *)0,
                                      &SpThread_run_,
                                      me);
    if (status != 0) {
        return status;
    }

    me->started = true;
    return 0;
}
