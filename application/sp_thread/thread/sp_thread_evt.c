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
#include <stdint.h>
#include "dbc_assert.h"
#include "platform_port.h"
#include "sp_thread/sp_thread.h"
#include "sp_thread_evt_priv.h"
DBC_MODULE_NAME("sp_thread_evt")

#define SP_THREAD_EVT_QLEN_ 16U

struct SpThreadEvtQueue {
    SpThreadEvt buf[SP_THREAD_EVT_QLEN_];
    uint8_t head;
    uint8_t tail;
    uint8_t used;
    PlatformMutex mutex;
};

struct SpThreadEventInbox {
    struct SpThreadEvtQueue queue;
    PlatformWake wake;
};

static struct SpThreadEventInbox SpThread_eventInbox_ = {
    .queue = {.mutex = PLATFORM_MUTEX_INITIALIZER},
    .wake = PLATFORM_WAKE_INITIALIZER
};

static void SpThread_evtPost_(SpThreadEvt const * const e) {
    DBC_REQUIRE(100, e != (SpThreadEvt const *)0);
    DBC_REQUIRE(101, e->sig > SPTHRD_NULL_SIG);
    DBC_REQUIRE(102, PlatformWaitObject_isValid(
        PlatformWake_waitObject(&SpThread_eventInbox_.wake)));

    struct SpThreadEvtQueue * const queue =
        &SpThread_eventInbox_.queue;
    int status = PlatformMutex_lock(&queue->mutex);
    DBC_ASSERT(104, status == 0);
    DBC_REQUIRE(103, queue->used < SP_THREAD_EVT_QLEN_);

    queue->buf[queue->head] = *e;
    if (queue->head == 0U) {
        queue->head = SP_THREAD_EVT_QLEN_ - 1U;
    } else {
        --queue->head;
    }
    ++queue->used;

    status = PlatformMutex_unlock(&queue->mutex);
    DBC_ASSERT(105, status == 0);

    status = PlatformWake_signal(&SpThread_eventInbox_.wake);
    (void)status;
}

void SpThread_postOpenPort(SerialConfig const * const config) {
    DBC_REQUIRE(110, config != (SerialConfig const *)0);

    SpThreadEvt const openEvt = {
        .sig = SPTHRD_OPEN_PORT_SIG,
        .config = *config,
    };
    SpThread_evtPost_(&openEvt);
}

void SpThread_postApplyConfig(SerialConfig const * const config) {
    DBC_REQUIRE(120, config != (SerialConfig const *)0);

    SpThreadEvt const configEvt = {
        .sig = SPTHRD_APPLY_CONFIG_SIG,
        .config = *config,
    };
    SpThread_evtPost_(&configEvt);
}

void SpThread_postClosePort(void) {
    static SpThreadEvt const closeEvt = {
        .sig = SPTHRD_CLOSE_PORT_SIG,
    };
    SpThread_evtPost_(&closeEvt);
}

void SpThread_postRefreshPorts(void) {
    static SpThreadEvt const refreshEvt = {
        .sig = SPTHRD_REFRESH_PORTS_SIG
    };
    SpThread_evtPost_(&refreshEvt);
}

int SpThread_evtInit(void) {
    DBC_REQUIRE(200, !PlatformWaitObject_isValid(
        PlatformWake_waitObject(&SpThread_eventInbox_.wake)));

    return PlatformWake_init(&SpThread_eventInbox_.wake);
}

void SpThread_evtDeinit(void) {
    DBC_REQUIRE(210, PlatformWaitObject_isValid(
        PlatformWake_waitObject(&SpThread_eventInbox_.wake)));

    PlatformWake_deinit(&SpThread_eventInbox_.wake);
}

PlatformWaitObject SpThread_evtWakeObject(void) {
    return PlatformWake_waitObject(&SpThread_eventInbox_.wake);
}

int SpThread_evtConsumeWake(void) {
    DBC_REQUIRE(220, PlatformWaitObject_isValid(
        PlatformWake_waitObject(&SpThread_eventInbox_.wake)));

    return PlatformWake_consume(&SpThread_eventInbox_.wake);
}

bool SpThread_evtDequeue(SpThreadEvt * const e) {
    DBC_REQUIRE(230, e != (SpThreadEvt *)0);

    struct SpThreadEvtQueue * const queue =
        &SpThread_eventInbox_.queue;
    int status = PlatformMutex_lock(&queue->mutex);
    DBC_ASSERT(231, status == 0);

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

    status = PlatformMutex_unlock(&queue->mutex);
    DBC_ASSERT(232, status == 0);
    (void)status;
    return hasEvent;
}
