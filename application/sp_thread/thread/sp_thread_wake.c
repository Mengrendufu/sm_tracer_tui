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
//=== Component: SpThreadWake
#include "dbc_assert.h"
#include "platform_port.h"
#include "sp_thread_wake_priv.h"
DBC_MODULE_NAME("sp_thread_wake")

enum {
    SP_THREAD_EVENT_SOURCE_,
    SP_THREAD_SERIAL_SOURCE_,
    SP_THREAD_SOURCE_COUNT_
};

static PlatformWaitSet SpThread_waitSet_ = PLATFORM_WAIT_SET_INITIALIZER;

void SpThreadWake_init(PlatformWaitObject const event) {
    DBC_REQUIRE(100, PlatformWaitObject_isValid(event));
    PlatformWaitSet_init(&SpThread_waitSet_, SP_THREAD_SOURCE_COUNT_);
    PlatformWaitSet_bind(&SpThread_waitSet_, SP_THREAD_EVENT_SOURCE_, event);
}

void SpThreadWake_setSerialObject(PlatformWaitObject const serial) {
    PlatformWaitSet_bind(&SpThread_waitSet_, SP_THREAD_SERIAL_SOURCE_,
                         serial);
}

int SpThreadWake_wait(int const timeoutMs) {
    DBC_REQUIRE(300, PlatformWaitObject_isValid(
        SpThread_waitSet_.objects[SP_THREAD_EVENT_SOURCE_]));
    DBC_REQUIRE(301, timeoutMs >= -1);

    PlatformWaitResult ready;
    PlatformWaitStatus const status = PlatformWaitSet_wait(
        &SpThread_waitSet_, timeoutMs, &ready);
    if (status == PLATFORM_WAIT_INTERRUPTED) {
        return SP_THREAD_WAKE_INTERRUPTED;
    } else if (status == PLATFORM_WAIT_ERROR) {
        return SP_THREAD_WAKE_ERROR;
    } else if (status == PLATFORM_WAIT_TIMEOUT) {
        return SP_THREAD_WAKE_TIMEOUT;
    }

    uint32_t const eventBit =
        (uint32_t)1U << SP_THREAD_EVENT_SOURCE_;
    uint32_t const serialBit =
        (uint32_t)1U << SP_THREAD_SERIAL_SOURCE_;
    if ((ready.closedMask & eventBit) != 0U)
    {
        return SP_THREAD_WAKE_ERROR;
    }

    int result = SP_THREAD_WAKE_TIMEOUT;
    if ((ready.readyMask & eventBit) != 0U) {
        result |= SP_THREAD_WAKE_EVENT;
    }
    if ((ready.readyMask & serialBit) != 0U) {
        result |= SP_THREAD_WAKE_SERIAL;
    }
    if ((ready.closedMask & serialBit) != 0U) {
        result |= SP_THREAD_WAKE_SERIAL_LOST;
    }
    if (result == SP_THREAD_WAKE_TIMEOUT) {
        return SP_THREAD_WAKE_ERROR;
    }
    return result;
}
