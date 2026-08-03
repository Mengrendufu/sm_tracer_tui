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
#include <pthread.h>
#include <stdbool.h>
#include "dbc_assert.h"
#include "sp_thread/sp_thread.h"
#include "sp_thread/hsm/sm_sp_thread.h"
#include "sp_thread_evt_priv.h"
#include "sp_thread_wake_priv.h"
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
    SpThreadWake_init(SpThread_evtWakeFd());

    for (;;) {
        int const ready = SpThreadWake_wait();
        if ((ready == SP_THREAD_WAKE_ERROR) && (errno == EINTR)) {
            continue;
        }
        if (ready == SP_THREAD_WAKE_ERROR) {
            DBC_ERROR(201);
        }

        int const consumeStatus = SpThread_evtConsumeWake();
        DBC_ASSERT(202, consumeStatus == 0);
        (void)consumeStatus;

        SpThreadEvt e;
        while (SpThread_evtDequeue(&e)) {
            SM_SpThread_dispatchEvt(&me->hsm, &e);
        }
    }

    return (void *)0;
}

//============================================================================
//=== Lifecycle

int SpThread_start(void) {
    SpThread * const me = &SpThread_inst_;
    DBC_REQUIRE(100, !me->started);

    int status = SpThread_evtInit();
    if (status != 0) {
        return status;
    }

    status = pthread_create(&me->thread,
                            (pthread_attr_t const *)0,
                            &SpThread_run_,
                            me);
    if (status != 0) {
        SpThread_evtDeinit();
        return status;
    }

    me->started = true;
    return 0;
}
