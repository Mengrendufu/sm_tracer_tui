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
//=== Component: SpThreadEventInbox
#include <pthread.h>
#include <stdint.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include "dbc_assert.h"
#include "sp_thread/sp_thread.h"
#include "sp_thread_evt_priv.h"
DBC_MODULE_NAME("sp_thread_evt")

#define SP_THREAD_EVT_QLEN_ 16U

struct SpThreadEvtQueue {
    SpThreadEvt buf[SP_THREAD_EVT_QLEN_];
    uint8_t head;
    uint8_t tail;
    uint8_t used;
    pthread_mutex_t mutex;
};

struct SpThreadEventInbox {
    struct SpThreadEvtQueue queue;
    int wakeFd;
};

static struct SpThreadEventInbox SpThread_eventInbox_ = {
    .queue = {.mutex = PTHREAD_MUTEX_INITIALIZER},
    .wakeFd = -1
};

static void SpThread_evtPost_(SpThreadEvt const * const e) {
    DBC_REQUIRE(100, e != (SpThreadEvt const *)0);
    DBC_REQUIRE(101, e->sig > SPTHRD_NULL_SIG);
    DBC_REQUIRE(102, SpThread_eventInbox_.wakeFd >= 0);

    struct SpThreadEvtQueue * const queue =
        &SpThread_eventInbox_.queue;
    pthread_mutex_lock(&queue->mutex);
    DBC_REQUIRE(103, queue->used < SP_THREAD_EVT_QLEN_);

    queue->buf[queue->head] = *e;
    if (queue->head == 0U) {
        queue->head = SP_THREAD_EVT_QLEN_ - 1U;
    } else {
        --queue->head;
    }
    ++queue->used;

    pthread_mutex_unlock(&queue->mutex);

    uint64_t const one = 1ULL;
    ssize_t const written = write(SpThread_eventInbox_.wakeFd,
                                  &one, sizeof(one));
    (void)written;
}

void SpThread_postRefreshPorts(void) {
    static SpThreadEvt const refreshEvt = {
        .sig = SPTHRD_REFRESH_PORTS_SIG
    };
    SpThread_evtPost_(&refreshEvt);
}

int SpThread_evtInit(void) {
    DBC_REQUIRE(200, SpThread_eventInbox_.wakeFd < 0);

    SpThread_eventInbox_.wakeFd = eventfd(
        0U, EFD_NONBLOCK | EFD_CLOEXEC);
    return SpThread_eventInbox_.wakeFd >= 0 ? 0 : 1;
}

void SpThread_evtDeinit(void) {
    DBC_REQUIRE(210, SpThread_eventInbox_.wakeFd >= 0);

    (void)close(SpThread_eventInbox_.wakeFd);
    SpThread_eventInbox_.wakeFd = -1;
}

int SpThread_evtWakeFd(void) {
    return SpThread_eventInbox_.wakeFd;
}

int SpThread_evtConsumeWake(void) {
    DBC_REQUIRE(220, SpThread_eventInbox_.wakeFd >= 0);

    uint64_t count;
    ssize_t const readSize = read(SpThread_eventInbox_.wakeFd,
                                  &count, sizeof(count));
    return readSize == (ssize_t)sizeof(count) ? 0 : 1;
}

bool SpThread_evtDequeue(SpThreadEvt * const e) {
    DBC_REQUIRE(230, e != (SpThreadEvt *)0);

    struct SpThreadEvtQueue * const queue =
        &SpThread_eventInbox_.queue;
    pthread_mutex_lock(&queue->mutex);

    bool const hasEvent = queue->used > 0U;
    if (hasEvent) {
        *e = queue->buf[queue->tail];
        if (queue->tail == 0U) {
            queue->tail = SP_THREAD_EVT_QLEN_ - 1U;
        } else {
            --queue->tail;
        }
        --queue->used;
    }

    pthread_mutex_unlock(&queue->mutex);
    return hasEvent;
}
