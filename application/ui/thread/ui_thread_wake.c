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
//=== Component: UIThreadWake — wait/wake coordination boundary
//
// UI Runtime binds borrowed terminal and event wait objects once. Frame Clock
// supplies the render timeout per wait. This module owns only the fixed wait
// set and maps platform readiness to UI wake reasons.

#include "dbc_assert.h"
#include "platform_port.h"
#include "ui_thread_wake_priv.h"

DBC_MODULE_NAME("ui_thread_wake")

enum UI_ThreadWakeSource_ {
    UI_THREAD_WAKE_TERMINAL_,
    UI_THREAD_WAKE_EVENT_,
    UI_THREAD_WAKE_SOURCE_NUM_
};

static PlatformWaitSet UI_waitSet_ = PLATFORM_WAIT_SET_INITIALIZER;

void UI_ThreadWake_init(PlatformWaitObject const terminal,
                        PlatformWaitObject const event)
{
    DBC_REQUIRE(100, PlatformWaitObject_isValid(terminal));
    DBC_REQUIRE(101, PlatformWaitObject_isValid(event));

    PlatformWaitSet_init(&UI_waitSet_, UI_THREAD_WAKE_SOURCE_NUM_);
    PlatformWaitSet_bind(&UI_waitSet_, UI_THREAD_WAKE_TERMINAL_,
                         terminal);
    PlatformWaitSet_bind(&UI_waitSet_, UI_THREAD_WAKE_EVENT_, event);
}

int UI_ThreadWake_wait(int timeout) {
    DBC_REQUIRE(200, PlatformWaitObject_isValid(
        UI_waitSet_.objects[UI_THREAD_WAKE_TERMINAL_]));
    DBC_REQUIRE(201, PlatformWaitObject_isValid(
        UI_waitSet_.objects[UI_THREAD_WAKE_EVENT_]));
    DBC_REQUIRE(202, timeout >= -1);

    PlatformWaitResult ready;
    PlatformWaitStatus const status = PlatformWaitSet_wait(
        &UI_waitSet_, timeout, &ready);
    if (status == PLATFORM_WAIT_TIMEOUT) {
        return UI_THREAD_WAKE_TIMEOUT;
    } else if (status == PLATFORM_WAIT_INTERRUPTED) {
        return UI_THREAD_WAKE_INTERRUPTED;
    } else if ((status != PLATFORM_WAIT_READY)
               || (ready.closedMask != 0U))
    {
        return UI_THREAD_WAKE_ERROR;
    }

    int result = UI_THREAD_WAKE_TIMEOUT;
    if ((ready.readyMask
         & ((uint32_t)1U << UI_THREAD_WAKE_TERMINAL_)) != 0U)
    {
        result |= UI_THREAD_WAKE_TERMINAL;
    }

    if ((ready.readyMask
         & ((uint32_t)1U << UI_THREAD_WAKE_EVENT_)) != 0U)
    {
        result |= UI_THREAD_WAKE_EVENT;
    }

    return result;
}
