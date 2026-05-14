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
//=== UI module — main loop, input router, render
#include <stdbool.h>
#include <poll.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include "dbc_assert.h"
#include "bsp.h"
#include "ui.h"
#include "sm_ui.h"
DBC_MODULE_NAME("ui")

#define UI_FRAME_MS_ (1000U / 60U)

static SM_UI SM_UI_inst;

//============================================================================
//=== BSP tick callback — posts UI_TIMER_SIG every Nth tick

#define UI_TICK_DIV_  BSP_TICKS_PER_SEC / 10U

static void UI_onTick_(void) {
    static uint8_t l_div = 0;
    if (++l_div >= UI_TICK_DIV_) {
        l_div = 0U;
        UI_postSignal(UI_TIMER_SIG);
    }
}

//============================================================================
//=== Input router: minimal — only quit keys

static void UI_routeInput_(uint32_t r, ncinput const *ni) {
    UI_Signal sig = UI_NULL_SIG;
    if (r == 27) {
        sig = UI_KEY_ESC_SIG;
    } else if (r == 0x1F) {
        sig = UI_KEY_CTRL_SLASH_SIG;
    } else if (r == NCKEY_UP) {
        sig = UI_KEY_UP_SIG;
    } else if (r == NCKEY_DOWN) {
        sig = UI_KEY_DOWN_SIG;
    } else if (r == NCKEY_ENTER) {
        sig = UI_KEY_ENTER_SIG;
    } else if (r == 'j') {
        sig = UI_KEY_J_SIG;
    } else if (r == 'k') {
        sig = UI_KEY_K_SIG;
    } else if (r == NCKEY_PGUP) {
        sig = UI_KEY_PGUP_SIG;
    } else if (r == NCKEY_PGDOWN) {
        sig = UI_KEY_PGDN_SIG;
    } else if (r == NCKEY_RESIZE) {
        sig = UI_RESIZE_SIG;
    } else if (r == NCKEY_EOF) {
        // input stream closed — notcurses can't recover; ignore silently
    } else {
        char buf[64];
        (void)snprintf(buf, sizeof(buf),
                       "key: r=0x%08X mod=%u\n", r, ni->modifiers);
        UI_postText(UI_KEY_DEBUG_SIG, buf);
    }

    if (sig != UI_NULL_SIG) {
        UI_postSignal(sig);
    }
}

//============================================================================
//=== Rendering

static void UI_render_(void) {
    if (!SM_UI_inst.dirty) {
        return;
    }

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    uint64_t elapsed = ((uint64_t)now.tv_sec
                        - (uint64_t)SM_UI_inst.lastRender.tv_sec)
                       * 1000ULL
                     + ((uint64_t)now.tv_nsec
                        - (uint64_t)SM_UI_inst.lastRender.tv_nsec)
                       / 1000000ULL;

    if (elapsed < UI_FRAME_MS_) {
        return;
    }

    SM_UI_inst.dirty       = false;
    SM_UI_inst.lastRender  = now;

    notcurses_render(SM_UI_inst.nc);
}

//============================================================================
//=== Prepare — init event system, HSM, tick registration

void UI_prepare(struct notcurses *nc) {
    DBC_REQUIRE(500, nc != (struct notcurses *)0);

    UI_evtInit();

    SM_UI_ctor(&SM_UI_inst);
    SM_UI_inst.nc = nc;
    SM_UI_start(&SM_UI_inst);

    BSP_registerTickHandler(&UI_onTick_);
}

//============================================================================
//=== Main loop

void UI_loop(void) {
    int ncFd = notcurses_inputready_fd(SM_UI_inst.nc);
    DBC_REQUIRE(502, ncFd >= 0);

    int evFd = UI_evtFd();
    DBC_REQUIRE(503, evFd >= 0);

    struct pollfd fds[2];
    fds[0].fd      = ncFd;
    fds[0].events  = POLLIN;
    fds[1].fd      = evFd;
    fds[1].events  = POLLIN;

    while (!SM_UI_inst.quit) {
        poll(fds, 2, -1);

        if (fds[0].revents & POLLIN) {
            ncinput  ni;
            uint32_t r = notcurses_get_nblock(SM_UI_inst.nc, &ni);
            if (r != 0U) {
                UI_routeInput_(r, &ni);
            }
        }

        if (fds[1].revents & POLLIN) {
            uint64_t dummy;
            ssize_t  rd = read(evFd, &dummy, sizeof(dummy));
            (void)rd;
        }

        {
            UI_Evt *e;
            while ((e = UI_evtDequeue()) != (UI_Evt *)0) {
                (*SM_UI_inst.dispatch)(&SM_UI_inst, e);
                SM_UI_inst.dirty = true;
                UI_evtFree(e);
            }
        }

        UI_render_();
    }

    close(evFd);
}
