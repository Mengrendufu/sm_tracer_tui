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
#include <errno.h>
#include <poll.h>
#include "dbc_assert.h"
#include "sp_thread_wake_priv.h"
DBC_MODULE_NAME("sp_thread_wake")

static struct pollfd SpThread_pollFd_ = {
    .fd = -1,
    .events = POLLIN
};

void SpThreadWake_init(int const eventFd) {
    DBC_REQUIRE(100, eventFd >= 0);
    SpThread_pollFd_.fd = eventFd;
}

int SpThreadWake_wait(void) {
    DBC_REQUIRE(200, SpThread_pollFd_.fd >= 0);

    int const ready = poll(&SpThread_pollFd_, 1U, -1);
    if (ready < 0) {
        return SP_THREAD_WAKE_ERROR;
    }
    if ((ready == 0) || ((SpThread_pollFd_.revents & POLLIN) == 0)) {
        errno = EIO;
        return SP_THREAD_WAKE_ERROR;
    }
    return SP_THREAD_WAKE_EVENT;
}
