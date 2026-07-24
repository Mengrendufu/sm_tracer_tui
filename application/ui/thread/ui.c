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
#include <stdint.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <notcurses/notcurses.h>
#include "dbc_assert.h"
#include "bsp.h"
#include "ui.h"
#include "ui_evt_priv.h"
#include "ui_thread_wake_priv.h"
#include "hsm/sm_ui.h"
DBC_MODULE_NAME("ui")

//============================================================================
//=== notcurses — owned by UI module, hidden from main

static struct notcurses_options UI_ncOpts_ = {
    .flags = NCOPTION_SUPPRESS_BANNERS
};
static struct notcurses *UI_nc_;

struct UI_HostState {
    bool quitRequested;
    bool framePending;
};

static struct UI_HostState UI_hostState_;

#define UI_FRAME_MS_   (1000U / 60U)

//============================================================================
//=== BSP tick callback — posts UI_TIMER_SIG every Nth tick

#define UI_TICK_DIV_  BSP_TICKS_PER_SEC / 10U

static void UI_onTick_(void) {
    static uint8_t l_div = 0;
    if (++l_div >= UI_TICK_DIV_) {
        l_div = 0U;
        UI_evtPostSignal(UI_TIMER_SIG);
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
    } else if (r == 0x0E || (r == 'N' && ncinput_ctrl_p(ni))) {
        sig = UI_KEY_CTRL_N_SIG;
    } else if (r == 0x10 || (r == 'P' && ncinput_ctrl_p(ni))) {
        sig = UI_KEY_CTRL_P_SIG;
    } else if (r == NCKEY_PGUP) {
        sig = UI_KEY_PGUP_SIG;
    } else if (r == NCKEY_PGDOWN) {
        sig = UI_KEY_PGDN_SIG;
    } else if (r == NCKEY_RESIZE) {
        sig = UI_RESIZE_SIG;
    } else {
        // DEBUG -------------------------------------------------------------
        char buf[64];
        (void)snprintf(buf, sizeof(buf),
                       "key: r=0x%08X mod=%u\n", r, ni->modifiers);
        UI_evtPostText(UI_KEY_DEBUG_SIG, buf);
    }

    if (sig != UI_NULL_SIG) {
        UI_evtPostSignal(sig);
    }
}

//============================================================================
//=== Frame clock — single time acquisition, shared by poll and render

#define UI_NS_PER_MS_  1000000L
#define UI_NS_PER_SEC_ 1000000000L

struct FrameClock_ {
    struct timespec now;
    struct timespec lastRender;
    bool            canRender;
    int             pollTimeout;
};

static uint64_t UI_elapsedMs_(struct timespec const *now,
                              struct timespec const *then)
{
    DBC_REQUIRE(504, now != (struct timespec const *)0);
    DBC_REQUIRE(505, then != (struct timespec const *)0);

    if (now->tv_sec < then->tv_sec) {
        return 0U;
    }

    uint64_t sec = (uint64_t)now->tv_sec - (uint64_t)then->tv_sec;
    long nsec = now->tv_nsec - then->tv_nsec;
    if (nsec < 0) {
        if (sec == 0U) {
            return 0U;
        }
        --sec;
        nsec += UI_NS_PER_SEC_;
    }

    return sec * 1000ULL + (uint64_t)nsec / (uint64_t)UI_NS_PER_MS_;
}

static int FrameClock_tick_(struct FrameClock_ *fc,
                            bool const framePending)
{
    DBC_REQUIRE(520, fc != (struct FrameClock_ *)0);

    if (clock_gettime(CLOCK_MONOTONIC, &fc->now) != 0) {
        return 1;
    }

    if (!framePending) {
        fc->canRender  = false;
        fc->pollTimeout = -1;
        return 0;
    }

    uint64_t const elapsed = UI_elapsedMs_(&fc->now, &fc->lastRender);
    fc->canRender  = (elapsed >= UI_FRAME_MS_);
    fc->pollTimeout = fc->canRender ? 0
                                    : (int)(UI_FRAME_MS_ - elapsed);
    return 0;
}

//============================================================================
// Host capabilities injected into SM_UI. The opaque context keeps runtime
// control latches owned and hidden by this module while HSM requests changes.
// One host context travels with the callback table; each handler restores its
// concrete type and mutates only the member belonging to that capability.
static void UI_requestQuit_(void * const ctx) {
    DBC_REQUIRE(521, ctx != (void *)0);
    ((struct UI_HostState *)ctx)->quitRequested = true;
}

static void UI_requestFrame_(void * const ctx) {
    DBC_REQUIRE(522, ctx != (void *)0);
    ((struct UI_HostState *)ctx)->framePending = true;
}

//============================================================================
int UI_init(void) {
    SM_UI_HostOps const hostOps = {
        .requestQuit = &UI_requestQuit_,
        .requestFrame = &UI_requestFrame_,
        .ctx = &UI_hostState_
    };

    UI_nc_ = notcurses_core_init(&UI_ncOpts_, NULL);
    if (UI_nc_ == (struct notcurses *)0) {
        return 1;
    }

    if (UI_evtInit() != 0) {
        notcurses_stop(UI_nc_);
        UI_nc_ = (struct notcurses *)0;
        return 1;
    }

    UI_hostState_.quitRequested = false;
    UI_hostState_.framePending = false;
    SM_UI_setup(UI_nc_, &hostOps);
    BSP_registerTickHandler(&UI_onTick_);
    return 0;
}

//============================================================================
//=== Main loop

int UI_run(void) {
    char const *errorMsg = (char const *)0;
    int errorNo = 0;
    int result = 0;

    // Runtime wiring for UI Thread Wake required inputs. notcurses Runtime
    // provides terminal readiness; UI Event Inbox provides event readiness.
    int terminalFd = notcurses_inputready_fd(UI_nc_);
    int eventFd = UI_evtWakeFd();
    DBC_REQUIRE(503, eventFd >= 0);

    struct FrameClock_ fc = {0};

    if (terminalFd < 0) {
        errorMsg = "notcurses input fd unavailable";
        result = 1;
        goto cleanup;
    }

    UI_ThreadWake_init(terminalFd, eventFd);

    while (!UI_hostState_.quitRequested) {
        if (FrameClock_tick_(&fc, UI_hostState_.framePending) != 0) {
            errorMsg = "clock_gettime failed";
            errorNo = errno;
            result = 1;
            break;
        }

        //--------------------------------------------------------------------
        // tripple block source:
        // 1. thread event queue
        // 2. notcurses input
        // 3. rendering timeout
        // Frame Clock supplies the dynamic render deadline to the fixed wait
        // set bound above.
        int waitReady = UI_ThreadWake_wait(fc.pollTimeout);
        if (waitReady < 0) {
            if (errno == EINTR) {
                continue;
            }
            errorMsg = "poll failed";
            errorNo = errno;
            result = 1;
            break;
        }

        if ((waitReady & UI_THREAD_WAKE_TERMINAL) != 0) {
            ncinput ni;
            uint32_t const r = notcurses_get_nblock(UI_nc_, &ni);
            if (r == (uint32_t)-1) {
                errorMsg = "notcurses input failed";
                result = 1;
                break;
            } else if (r == NCKEY_EOF) {
                errorMsg = "terminal input closed";
                result = 1;
                break;
            } else if (r != 0U) {
                UI_routeInput_(r, &ni);
            }
        }

        if ((waitReady & UI_THREAD_WAKE_EVENT) != 0) {
            if (UI_evtConsumeWake() != 0) {
                errorMsg = "eventfd read failed";
                errorNo = errno;
                result = 1;
                break;
            }
        }

        {
            UI_Evt *e;
            while ((e = UI_evtDequeue()) != (UI_Evt *)0) {
                SM_UI_dispatchEvt(e);
                UI_evtFree(e);
            }
        }

        if (fc.canRender) {
            UI_hostState_.framePending = false;
            SM_UI_flush();
            if (notcurses_render(UI_nc_) != 0) {
                errorMsg = "notcurses render failed";
                result = 1;
                break;
            }
            fc.lastRender = fc.now;
        }
    }

cleanup:
    // SST producers remain active until process exit, so eventfd lifetime
    // follows the process.
    // notcurses must still restore the terminal here.
    if (notcurses_stop(UI_nc_) != 0) {
        if (errorMsg == (char const *)0) {
            errorMsg = "notcurses stop failed";
        }
        result = 1;
    }
    UI_nc_ = (struct notcurses *)0;

    if (errorMsg != (char const *)0) {
        if (errorNo != 0) {
            fprintf(stderr, "%s: %s\n", errorMsg, strerror(errorNo));
        } else {
            fprintf(stderr, "%s\n", errorMsg);
        }
    }

    return result;
}
