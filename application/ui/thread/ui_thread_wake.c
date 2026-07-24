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
// UI Runtime binds borrowed terminal and event descriptors once. Frame Clock
// supplies the render timeout per wait. This module owns only the fixed wait
// set and maps Linux poll readiness to UI wake reasons.

#include <poll.h>

#include "dbc_assert.h"
#include "ui_thread_wake_priv.h"

DBC_MODULE_NAME("ui_thread_wake")

enum UI_ThreadWakeSource_ {
    UI_THREAD_WAKE_TERMINAL_FD_,
    UI_THREAD_WAKE_EVENT_FD_,
    UI_THREAD_WAKE_FD_NUM_
};

static struct pollfd l_pollFds_[UI_THREAD_WAKE_FD_NUM_] = {
    [UI_THREAD_WAKE_TERMINAL_FD_] = {.fd = -1, .events = POLLIN},
    [UI_THREAD_WAKE_EVENT_FD_]    = {.fd = -1, .events = POLLIN},
};

void UI_ThreadWake_init(int terminalFd, int eventFd) {
    DBC_REQUIRE(100, terminalFd >= 0);
    DBC_REQUIRE(101, eventFd >= 0);

    l_pollFds_[UI_THREAD_WAKE_TERMINAL_FD_].fd = terminalFd;
    l_pollFds_[UI_THREAD_WAKE_EVENT_FD_].fd = eventFd;
}

int UI_ThreadWake_wait(int timeout) {
    DBC_REQUIRE(200,
                l_pollFds_[UI_THREAD_WAKE_TERMINAL_FD_].fd >= 0);
    DBC_REQUIRE(201,
                l_pollFds_[UI_THREAD_WAKE_EVENT_FD_].fd >= 0);
    DBC_REQUIRE(202, timeout >= -1);

    int const ready = poll(l_pollFds_, UI_THREAD_WAKE_FD_NUM_, timeout);
    if (ready < 0) {
        return -1;
    }

    if (ready == 0) {
        return UI_THREAD_WAKE_TIMEOUT;
    }

    int result = UI_THREAD_WAKE_TIMEOUT;
    if (l_pollFds_[UI_THREAD_WAKE_TERMINAL_FD_].revents & POLLIN) {
        result |= UI_THREAD_WAKE_TERMINAL;
    }

    if (l_pollFds_[UI_THREAD_WAKE_EVENT_FD_].revents & POLLIN) {
        result |= UI_THREAD_WAKE_EVENT;
    }

    return result;
}
