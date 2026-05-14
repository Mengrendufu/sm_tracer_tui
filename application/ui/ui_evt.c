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
//=== UI event system — queue, allocator, cross-thread post
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include "dbc_assert.h"
#include "ui_evt.h"
DBC_MODULE_NAME("ui_evt")

#define UI_QLEN_ 128U

struct UI_EvtQueue {
    UI_Evt *buf[UI_QLEN_];
    uint8_t head;
    uint8_t tail;
    uint8_t used;
    pthread_mutex_t mtx;
};

static UI_EvtQueue UI_q_  = { .mtx = PTHREAD_MUTEX_INITIALIZER };
static int         l_evfd = -1; // eventfd for poll wake-up

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

    pthread_mutex_lock(&UI_q_.mtx);
    DBC_REQUIRE(201, UI_q_.used < UI_QLEN_);

    UI_q_.buf[UI_q_.head] = e;
    if (UI_q_.head == 0U) {
        UI_q_.head = UI_QLEN_ - 1U;
    } else {
        --UI_q_.head;
    }
    ++UI_q_.used;

    pthread_mutex_unlock(&UI_q_.mtx);
}

static UI_Evt *UI_dequeue_(void) {
    pthread_mutex_lock(&UI_q_.mtx);

    UI_Evt *e = (UI_Evt *)0;

    if (UI_q_.used > 0U) {
        e = UI_q_.buf[UI_q_.tail];
        if (UI_q_.tail == 0U) {
            UI_q_.tail = UI_QLEN_ - 1U;
        } else {
            --UI_q_.tail;
        }
        --UI_q_.used;
    }

    pthread_mutex_unlock(&UI_q_.mtx);
    return e;
}

//============================================================================
//=== Wake main loop after enqueue

static void UI_wake_(void) {
    uint64_t one = 1ULL;
    ssize_t  wr  = write(l_evfd, &one, sizeof(one));
    (void)wr;
}

//============================================================================
//=== Cross-thread post

void UI_postSignal(UI_Signal sig) {
    DBC_REQUIRE(300, sig > UI_NULL_SIG);
    DBC_REQUIRE(301, l_evfd >= 0);

    UI_Evt *ue = (UI_Evt *)UI_Alloc_(sizeof(UI_Evt));
    ue->sig = sig;

    UI_enqueue_(ue);
    UI_wake_();
}

void UI_postText(UI_Signal sig, char const *text) {
    DBC_REQUIRE(310, sig  > UI_NULL_SIG);
    DBC_REQUIRE(311, text != (char const *)0);
    DBC_REQUIRE(312, l_evfd >= 0);

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

//============================================================================
//=== Init / Dequeue / Free

void UI_evtInit(void) {
    l_evfd = eventfd(0, EFD_NONBLOCK);
    DBC_REQUIRE(500, l_evfd >= 0);
}

int UI_evtFd(void) {
    return l_evfd;
}

UI_Evt *UI_evtDequeue(void) {
    return UI_dequeue_();
}

void UI_evtFree(UI_Evt *e) {
    UI_Free_(e);
}
