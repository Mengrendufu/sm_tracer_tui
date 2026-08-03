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
//=== Component: UIEventInbox
//
// Owns queued UI events and the eventfd used to wake UIThreadRuntime.
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include "dbc_assert.h"
#include "ui_evt.h"
#include "ui_evt_priv.h"
DBC_MODULE_NAME("ui_evt")

#define UI_QLEN_ 512U

struct UI_EvtQueue {
    UI_Evt *buf[UI_QLEN_];
    uint16_t head;
    uint16_t tail;
    uint16_t used;
    pthread_mutex_t mtx;
};

struct UIEventInbox {
    struct UI_EvtQueue queue;
    int wakeFd;
};

static struct UIEventInbox UI_eventInbox_ = {
    .queue = {.mtx = PTHREAD_MUTEX_INITIALIZER},
    .wakeFd = -1
};

//============================================================================
//=== Allocator

static void *UI_Alloc_(size_t size) {
    void *p = malloc(size);
    DBC_REQUIRE(100, p != (void *)0);
    return p;
}

static void UI_Free_(void *p) {
    free(p);
}

//============================================================================
//=== Queue operations

static void UI_enqueue_(UI_Evt *e) {
    DBC_REQUIRE(200, e != (UI_Evt *)0);

    struct UI_EvtQueue * const queue = &UI_eventInbox_.queue;
    pthread_mutex_lock(&queue->mtx);
    DBC_REQUIRE(201, queue->used < UI_QLEN_);

    queue->buf[queue->head] = e;
    if (queue->head == 0U) {
        queue->head = UI_QLEN_ - 1U;
    } else {
        --queue->head;
    }
    ++queue->used;

    pthread_mutex_unlock(&queue->mtx);
}

static UI_Evt *UI_dequeue_(void) {
    struct UI_EvtQueue * const queue = &UI_eventInbox_.queue;
    pthread_mutex_lock(&queue->mtx);

    UI_Evt *e = (UI_Evt *)0;

    if (queue->used > 0U) {
        e = queue->buf[queue->tail];
        if (queue->tail == 0U) {
            queue->tail = UI_QLEN_ - 1U;
        } else {
            --queue->tail;
        }
        --queue->used;
    }

    pthread_mutex_unlock(&queue->mtx);
    return e;
}

//============================================================================
//=== Wake main loop after enqueue

static void UI_wake_(void) {
    uint64_t one = 1ULL;
    ssize_t  wr  = write(UI_eventInbox_.wakeFd, &one, sizeof(one));
    (void)wr;
}

//============================================================================
//=== Event producers

void UI_evtPostSignal(UI_Signal sig) {
    DBC_REQUIRE(300, sig > UI_NULL_SIG);
    DBC_REQUIRE(301, UI_eventInbox_.wakeFd >= 0);

    UI_Evt *ue = (UI_Evt *)UI_Alloc_(sizeof(UI_Evt));
    ue->sig = sig;

    UI_enqueue_(ue);
    UI_wake_();
}

void UI_evtPostText(UI_Signal sig, char const *text) {
    DBC_REQUIRE(310, sig  > UI_NULL_SIG);
    DBC_REQUIRE(311, text != (char const *)0);
    DBC_REQUIRE(312, UI_eventInbox_.wakeFd >= 0);

    size_t len = strlen(text);
    size_t size = sizeof(UI_AppEvt) + len + 1U;
    UI_AppEvt *ae = (UI_AppEvt *)UI_Alloc_(size);
    ae->super.sig    = sig;
    ae->pld.msg.len  = len;
    ae->pld.msg.text = (char *)(ae + 1);
    memcpy(ae->pld.msg.text, text, len);
    ae->pld.msg.text[len] = '\0';

    UI_enqueue_((UI_Evt *)ae);
    UI_wake_();
}

void UI_evtEnqueueInput(UI_Signal const sig,
                        UI_Input const * const input)
{
    DBC_REQUIRE(320, (UI_INPUT_SIG <= sig)
                     && (sig <= UI_RESIZE_SIG));
    DBC_REQUIRE(321, input != (UI_Input const *)0);
    DBC_REQUIRE(322, UI_eventInbox_.wakeFd >= 0);

    UI_InputEvt *ie = (UI_InputEvt *)UI_Alloc_(sizeof(UI_InputEvt));
    ie->super.sig = sig;
    ie->input = *input;

    // Terminal input is already executing on the UI thread and will be
    // drained in this loop iteration, so no eventfd wake is required.
    UI_enqueue_((UI_Evt *)ie);
}

void UI_postText(char const *text) {
    UI_evtPostText(UI_TEXT_SIG, text);
}

void UI_postPortList(char const * const portNames,
                     size_t const portNamesSize)
{
    DBC_REQUIRE(330, portNames != (char const *)0);
    DBC_REQUIRE(331, portNamesSize >= 2U);
    DBC_REQUIRE(332, portNames[portNamesSize - 1U] == '\0');
    DBC_REQUIRE(333, portNames[portNamesSize - 2U] == '\0');
    DBC_REQUIRE(334, UI_eventInbox_.wakeFd >= 0);
    DBC_REQUIRE(335,
                portNamesSize <= (SIZE_MAX - sizeof(UI_PortListEvt)));

    size_t const size = sizeof(UI_PortListEvt) + portNamesSize;
    UI_PortListEvt * const pe =
        (UI_PortListEvt *)UI_Alloc_(size);
    pe->super.sig = UI_REFRESHED_PORTS_SIG;
    pe->portNamesSize = portNamesSize;
    pe->portNames = (char *)(pe + 1);
    memcpy(pe->portNames, portNames, portNamesSize);

    UI_enqueue_((UI_Evt *)pe);
    UI_wake_();
}

//============================================================================
//=== Init / Wake fd / Dequeue / Free

int UI_evtInit(void) {
    DBC_REQUIRE(500, UI_eventInbox_.wakeFd < 0);

    UI_eventInbox_.wakeFd = eventfd(0, EFD_NONBLOCK);
    return UI_eventInbox_.wakeFd >= 0 ? 0 : 1;
}

int UI_evtWakeFd(void) {
    return UI_eventInbox_.wakeFd;
}

int UI_evtConsumeWake(void) {
    DBC_REQUIRE(501, UI_eventInbox_.wakeFd >= 0);

    uint64_t count;
    ssize_t const rd = read(UI_eventInbox_.wakeFd, &count, sizeof(count));
    return rd == (ssize_t)sizeof(count) ? 0 : 1;
}

UI_Evt *UI_evtDequeue(void) {
    return UI_dequeue_();
}

void UI_evtFree(UI_Evt *e) {
    UI_Free_(e);
}
