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
#include <string.h>
#include <time.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include "dbc_assert.h"
#include "bsp.h"
#include "ui.h"
#include "ui_hsm.h"
DBC_MODULE_NAME("ui")

//============================================================================
//=== UI event queue (cross-thread ring buffer, holds UI-allocated events)

#define UI_QLEN_ 16U
#define UI_FRAME_MS_ (1000U / 60U)

typedef struct {
    UI_Evt *buf[UI_QLEN_];
    uint8_t head;
    uint8_t tail;
    uint8_t used;
    pthread_mutex_t mtx;
} UI_EvtQueue;

static UI_EvtQueue UI_q_  = { .mtx = PTHREAD_MUTEX_INITIALIZER };
static int         l_evfd = -1; // eventfd for poll wake-up

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

//============================================================================
//=== Wake main loop after enqueue

static void UI_wake_(void) {
    uint64_t one = 1ULL;
    ssize_t  wr  = write(l_evfd, &one, sizeof(one));
    (void)wr;
}

//============================================================================
//=== Cross-thread post (called from SST AOs or tick callback)

void UI_postSignal(UI_Signal sig) {
    DBC_REQUIRE(300, sig > UI_NULL_SIG);
    DBC_REQUIRE(301, l_evfd >= 0);

    UI_Evt *ue = (UI_Evt *)UI_Alloc_(sizeof(UI_Evt));
    ue->sig = sig;

    UI_enqueue_(ue);
    UI_wake_();
}

void UI_postText(UI_Signal sig, char const *text, size_t len) {
    DBC_REQUIRE(310, sig  > UI_NULL_SIG);
    DBC_REQUIRE(311, text != (char const *)0);
    DBC_REQUIRE(312, len  > 0U);
    DBC_REQUIRE(313, l_evfd >= 0);

    size_t size = sizeof(UI_Evt) + len + 1U;
    UI_Evt *ue = (UI_Evt *)UI_Alloc_(size);
    ue->sig      = sig;
    ue->pld.msg.len  = len;
    ue->pld.msg.text = (char *)(ue + 1);
    memcpy(ue->pld.msg.text, text, len);
    ue->pld.msg.text[len] = '\0';

    UI_enqueue_(ue);
    UI_wake_();
}

//============================================================================
//=== BSP tick callback — posts UI_TIMER_SIG every Nth tick

#define UI_TICK_DIV_ 3U

static void UI_onTick_(void) {
    static uint8_t l_div;
    if (++l_div >= UI_TICK_DIV_) {
        l_div = 0U;
        UI_postSignal(UI_TIMER_SIG);
    }
}

//============================================================================
//=== Input router: notcurses raw input → UI event → enqueue

static UI_Evt *UI_quitEvt_(void) {
    UI_Evt *e = (UI_Evt *)UI_Alloc_(sizeof(UI_Evt));
    e->sig = UI_QUIT_SIG;
    return e;
}

static void UI_routeInput_(uint32_t r, ncinput const *ni) {
    (void)ni; // reserved for future use (modifiers, evtype)

    // quit keys
    switch (r) {
    case (uint32_t)-1:   // EOF / disconnect
    case NCKEY_ENTER:
    case NCKEY_ESC:
    case 'q':
    case 'Q':
        UI_enqueue_(UI_quitEvt_());
        UI_wake_();
        return;
    default:
        break;
    }

    // route everything else as key event
    if (r != 0U) {
        UI_Evt *e = (UI_Evt *)UI_Alloc_(sizeof(UI_Evt));
        e->sig      = UI_KEY_SIG;
        e->pld.key      = r;
        UI_enqueue_(e);
        UI_wake_();
    }
}

//============================================================================
//=== Rendering

static void UI_renderKeybar_(void) {
    if (l_uiAo.keybarPlane == (struct ncplane *)0) {
        return;
    }
    ncplane_erase(l_uiAo.keybarPlane);
    if (UI_CmdHsm_isActive(&l_uiAo.cmdHsm)) {
        char buf[128];
        snprintf(buf, sizeof(buf), " :%s", UI_CmdHsm_buf(&l_uiAo.cmdHsm));
        ncplane_puttext(l_uiAo.keybarPlane, 0, NCALIGN_LEFT,
                        buf, (size_t)0);
    } else {
        ncplane_puttext(l_uiAo.keybarPlane, 0, NCALIGN_CENTER,
                        " ESC:quit | :cmd | /menu | /connect ",
                        (size_t)0);
    }
}

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

    UI_renderKeybar_();
    notcurses_render(l_uiAo.nc);
}

//============================================================================
//=== Prepare — init planes, eventfd, tick (no poll yet)

void UI_prepare(struct notcurses *nc) {
    DBC_REQUIRE(500, nc != (struct notcurses *)0);

    // eventfd must exist before HSM init (cmd HSM posts CMD_IDLE on init)
    l_evfd = eventfd(0, EFD_NONBLOCK);
    DBC_REQUIRE(501, l_evfd >= 0);

    UI_AO_ctor(&l_uiAo);
    l_uiAo.nc = nc;
    UI_AO_init(&l_uiAo);

    // tick producer starts → events go to queue + evfd
    BSP_registerTickHandler(&UI_onTick_);
}

//============================================================================
//=== Main loop

void UI_loop(void) {
    int ncFd = notcurses_inputready_fd(l_uiAo.nc);
    DBC_REQUIRE(502, ncFd >= 0);

    struct pollfd fds[2];
    fds[0].fd      = ncFd;
    fds[0].events  = POLLIN;
    fds[1].fd      = l_evfd;
    fds[1].events  = POLLIN;

    while (!l_uiAo.quit) {
        // ================================================================
        // Phase 1 — block until event
        // ================================================================
        poll(fds, 2, -1);

        // ================================================================
        // Phase 2 — route: gather and enqueue
        // ================================================================

        // 2a. notcurses input → route → enqueue (one at a time)
        if (fds[0].revents & POLLIN) {
            ncinput  ni;
            uint32_t r = notcurses_get_nblock(l_uiAo.nc, &ni);
            if (r != 0U) {
                UI_routeInput_(r, &ni);
            }
        }

        // 2b. drain eventfd (SST tick or cross-thread post)
        if (fds[1].revents & POLLIN) {
            uint64_t dummy;
            ssize_t  rd = read(l_evfd, &dummy, sizeof(dummy));
            (void)rd;
        }

        // ================================================================
        // Phase 3 — drain all pending events
        // ================================================================

        {
            UI_Evt *e;
            while ((e = UI_dequeue_()) != (UI_Evt *)0) {
                (*l_uiAo.dispatch)(&l_uiAo, e);
                l_uiAo.dirty = true;
                UI_Free_(e);
            }
        }

        // ================================================================
        // Phase 4 — render
        // ================================================================

        UI_render_();
    }

    close(l_evfd);
}
