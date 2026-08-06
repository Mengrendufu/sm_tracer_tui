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
#include <stdio.h>
#include <string.h>
#include <notcurses/notcurses.h>
#include "dbc_assert.h"
#include "bsp.h"
#include "platform_port.h"
#include "ui.h"
#include "ui_evt_priv.h"
#include "ui_input_router_priv.h"
#include "ui_terminal_input_priv.h"
#include "ui_thread_wake_priv.h"
#include "hsm/sm_ui.h"
DBC_MODULE_NAME("ui")

//============================================================================
//=== Component: NotcursesRuntime
//
// Owned by UIThreadRuntime; supplies terminal input and frame rendering.

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
#define UI_INPUT_BATCH_SIZE_ 64

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
//=== Input adapter: preserve payload and classify the UI signal

static void UI_routeInput_(uint32_t r, ncinput const *ni) {
    DBC_REQUIRE(500, ni != (ncinput const *)0);

    UI_Input input = {
        .id = r,
        .modifiers = ni->modifiers,
        .type = (uint32_t)ni->evtype,
    };
    memcpy(input.utf8, ni->utf8, sizeof(input.utf8));
    UI_evtEnqueueInput(UI_InputRouter_route(&input), &input);
}

static void UI_dispatchPendingEvents_(void) {
    UI_Evt *e;

    while ((e = UI_evtDequeue()) != (UI_Evt *)0) {
        SM_UI_dispatchEvt(e);
        UI_evtFree(e);
    }
}

enum UI_TerminalInputResult {
    UI_TERMINAL_INPUT_OK,
    UI_TERMINAL_INPUT_ERROR,
    UI_TERMINAL_INPUT_CLOSED
};

static enum UI_TerminalInputResult UI_drainTerminalInput_(void) {
    ncinput input[UI_INPUT_BATCH_SIZE_];

    for (;;) {
        int const count = UI_TerminalInput_read(
            input, UI_INPUT_BATCH_SIZE_);
        if (count < 0) {
            return UI_TERMINAL_INPUT_ERROR;
        }
        if (count == 0) {
            return UI_TERMINAL_INPUT_OK;
        }

        for (int i = 0; i < count; ++i) {
            if (input[i].id == NCKEY_EOF) {
                UI_dispatchPendingEvents_();
                return UI_TERMINAL_INPUT_CLOSED;
            }
            UI_routeInput_(input[i].id, &input[i]);
        }
        UI_dispatchPendingEvents_();
    }
}

//============================================================================
//=== Component: FrameClock
//
// Acquires time once per loop and supplies render eligibility and deadline.

struct FrameClock {
    uint64_t nowMs;
    uint64_t lastRenderMs;
    bool     canRender;
    int      pollTimeout;
};

static int FrameClock_tick_(struct FrameClock *frameClock,
                            bool const framePending)
{
    DBC_REQUIRE(520, frameClock != (struct FrameClock *)0);

    if (Platform_monotonicMs(&frameClock->nowMs) != 0) {
        return 1;
    }

    if (!framePending) {
        frameClock->canRender  = false;
        frameClock->pollTimeout = -1;
        return 0;
    }

    uint64_t const elapsed = frameClock->nowMs >= frameClock->lastRenderMs
        ? frameClock->nowMs - frameClock->lastRenderMs : 0U;
    frameClock->canRender = (elapsed >= UI_FRAME_MS_);
    frameClock->pollTimeout =
        frameClock->canRender ? 0 : (int)(UI_FRAME_MS_ - elapsed);
    return 0;
}

//============================================================================
//=== Interface: ISMUIHostOps
//
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
//=== Component: UIThreadRuntime
//
// Owns lifecycle wiring and the main event/input/render loop.

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

    struct ncplane * const std = notcurses_stdplane(UI_nc_);
    nccell base = NCCELL_TRIVIAL_INITIALIZER;
    nccell_set_bg_rgb8(&base, 24, 27, 31);
    nccell_load_char(std, &base, ' ');
    ncplane_set_base_cell(std, &base);
    nccell_release(std, &base);
    ncplane_erase(std);

    if (UI_evtInit() != 0) {
        notcurses_stop(UI_nc_);
        UI_nc_ = (struct notcurses *)0;
        return 1;
    }
    if (UI_TerminalInput_init(UI_nc_) != 0) {
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
    int result = 0;

    // Runtime wiring for UI Thread Wake required inputs. notcurses Runtime
    // provides terminal readiness; UI Event Inbox provides event readiness.
    PlatformWaitObject const terminal =
        UI_TerminalInput_waitObject();
    PlatformWaitObject const event = UI_evtWakeObject();
    DBC_REQUIRE(503, PlatformWaitObject_isValid(terminal));
    DBC_REQUIRE(504, PlatformWaitObject_isValid(event));

    struct FrameClock frameClock = {0};

    UI_ThreadWake_init(terminal, event);

    while (!UI_hostState_.quitRequested) {
        if (FrameClock_tick_(&frameClock,
                             UI_hostState_.framePending) != 0)
        {
            errorMsg = "monotonic clock failed";
            result = 1;
            break;
        }

        //--------------------------------------------------------------------
        // triple blocking sources:
        // 1. thread event queue
        // 2. notcurses input
        // 3. rendering timeout
        // Frame Clock supplies the dynamic render deadline to the fixed wait
        // set bound above.
        int waitReady = UI_ThreadWake_wait(frameClock.pollTimeout);
        if (waitReady == UI_THREAD_WAKE_INTERRUPTED) {
            continue;
        } else if (waitReady == UI_THREAD_WAKE_ERROR) {
            errorMsg = "UI thread wait failed";
            result = 1;
            break;
        }

        if ((waitReady & UI_THREAD_WAKE_EVENT) != 0) {
            if (UI_evtConsumeWake() != 0) {
                errorMsg = "UI event wake consume failed";
                result = 1;
                break;
            }
        }

        UI_dispatchPendingEvents_();

        if ((waitReady & UI_THREAD_WAKE_TERMINAL) != 0) {
            enum UI_TerminalInputResult const inputResult =
                UI_drainTerminalInput_();
            if (inputResult == UI_TERMINAL_INPUT_ERROR) {
                errorMsg = "notcurses input failed";
                result = 1;
                break;
            } else if (inputResult == UI_TERMINAL_INPUT_CLOSED) {
                errorMsg = "terminal input closed";
                result = 1;
                break;
            }
        }

        // Catch cross-thread events posted while terminal input was drained.
        UI_dispatchPendingEvents_();

        if (frameClock.canRender) {
            UI_hostState_.framePending = false;
            SM_UI_flush();
            if (notcurses_render(UI_nc_) != 0) {
                errorMsg = "notcurses render failed";
                result = 1;
                break;
            }
            frameClock.lastRenderMs = frameClock.nowMs;
        }
    }

    // SST producers remain active until process exit, so wake lifetime
    // follows the process. Stop the Windows terminal bridge before destroying
    // the borrowed notcurses runtime it reads from.
    SM_UI_teardown();
    if (UI_TerminalInput_deinit() != 0) {
        if (errorMsg == (char const *)0) {
            errorMsg = "terminal input stop failed";
        }
        result = 1;
    }
    if (notcurses_stop(UI_nc_) != 0) {
        if (errorMsg == (char const *)0) {
            errorMsg = "notcurses stop failed";
        }
        result = 1;
    }
    UI_nc_ = (struct notcurses *)0;

    if (errorMsg != (char const *)0) {
        fprintf(stderr, "%s\n", errorMsg);
    }

    return result;
}
