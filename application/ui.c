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
//=== UI module — event queue, input router, render, main loop
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include "dbc_assert.h"
#include "ui.h"
#include "ui_hsm.h"
DBC_MODULE_NAME("ui")

//============================================================================
//=== UI event queue (cross-thread ring buffer, holds UI-allocated events)

#define UI_QLEN_ 16U
#define UI_DRAIN_BUDGET_ 8U
#define UI_FRAME_MS_ (1000U / 60U)

typedef struct {
    UI_Evt *buf[UI_QLEN_];
    uint8_t head;
    uint8_t tail;
    uint8_t used;
    pthread_mutex_t mtx;
} UI_EvtQueue;

static UI_EvtQueue UI_q_ = { .mtx = PTHREAD_MUTEX_INITIALIZER };

//============================================================================
//=== UI module state

static UI_AO l_uiAo;

//============================================================================
//=== UI event allocator (heap, supports variable-length payload)

static void *UI_Alloc_(size_t size) {
    void *p = malloc(size);
    DBC_REQUIRE(100, p != (void *)0);
    return p;
}

static void UI_Free_(void *p) {
    free(p);
}

//============================================================================
//=== Event queue operations

// enqueue (thread-safe, used by both router and SST→UI bridge)
static void UI_enqueue_(UI_Evt *e) {
    pthread_mutex_lock(&UI_q_.mtx);
    DBC_REQUIRE(200, UI_q_.used < UI_QLEN_);

    UI_q_.buf[UI_q_.head] = e;
    if (UI_q_.head == 0U) {
        UI_q_.head = UI_QLEN_ - 1U;
    } else {
        --UI_q_.head;
    }
    ++UI_q_.used;

    pthread_mutex_unlock(&UI_q_.mtx);
}

// dequeue (thread-safe — SST enqueue may run concurrently)
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

// drain → virtual dispatch → free (unified event dispatch point)
static void UI_drain_(void) {
    for (uint8_t i = 0U; i < UI_DRAIN_BUDGET_; ++i) {
        UI_Evt *e = UI_dequeue_();
        if (e == (UI_Evt *)0) {
            break;
        }
        (*l_uiAo.dispatch)(&l_uiAo, e);
        l_uiAo.dirty = true;
        UI_Free_(e);
    }
}

//============================================================================
//=== Cross-thread post (called from SST AOs)

void UI_postSignal(UI_Signal sig) {
    UI_Evt *ue = (UI_Evt *)UI_Alloc_(sizeof(UI_Evt));
    ue->sig = sig;
    UI_enqueue_(ue);
}

//============================================================================
//=== Input router: notcurses raw input → UI event → enqueue

static void UI_routeInput_(uint32_t r, ncinput const *ni) {
    (void)ni;

    switch (r) {
    case (uint32_t)-1:
    case NCKEY_ENTER: {
        UI_Evt *e = (UI_Evt *)UI_Alloc_(sizeof(UI_Evt));
        e->sig = UI_QUIT_SIG;
        UI_enqueue_(e);
        return;
    }
    default:
        break;
    }

    if (r != 0U) {
        UI_KeyEvt *ke = (UI_KeyEvt *)UI_Alloc_(sizeof(UI_KeyEvt));
        ke->super.sig = UI_KEY_SIG;
        ke->key       = r;
        UI_enqueue_((UI_Evt *)ke);
    }
}

//============================================================================
//=== Rendering

static void UI_render_(void) {
    if (!l_uiAo.dirty) {
        return;
    }

    // frame rate limiter
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    long elapsed = (now.tv_sec  - l_uiAo.lastRender.tv_sec)  * 1000L
                 + (now.tv_nsec - l_uiAo.lastRender.tv_nsec) / 1000000L;

    if (elapsed < (long)UI_FRAME_MS_) {
        return; // keep dirty flag, retry next tick
    }

    l_uiAo.dirty       = false;
    l_uiAo.lastRender  = now;

    // TODO: render based on l_uiAo state
}

//============================================================================
//=== Main loop

void UI_run(struct notcurses *nc) {
    UI_AO_ctor(&l_uiAo);
    l_uiAo.nc = nc;
    UI_AO_init(&l_uiAo);

    struct timespec ts = { .tv_sec = 0, .tv_nsec = (long)(UI_FRAME_MS_) * 1000000L };

    while (!l_uiAo.quit) {
        // ================================================================
        // Phase 1 — gather
        // ================================================================

        // 1a. read notcurses input → route → enqueue
        ncinput  ni;
        uint32_t r = notcurses_get(l_uiAo.nc, &ts, &ni);
        UI_routeInput_(r, &ni);

        // ================================================================
        // Phase 2 — process events
        // ================================================================

        // drain queue → unified dispatch point → free
        UI_drain_();

        // ================================================================
        // Phase 3 — render
        // ================================================================

        UI_render_();
    }
}
