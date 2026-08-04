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

enum {
    SP_THREAD_EVENT_FD_,
    SP_THREAD_SERIAL_FD_,
    SP_THREAD_FD_COUNT_
};

static struct pollfd SpThread_pollFds_[SP_THREAD_FD_COUNT_] = {
    [SP_THREAD_EVENT_FD_] = {
        .fd = -1,
        .events = POLLIN,
    },
    [SP_THREAD_SERIAL_FD_] = {
        .fd = -1,
        .events = POLLIN,
    },
};

void SpThreadWake_init(int const eventFd) {
    DBC_REQUIRE(100, eventFd >= 0);
    SpThread_pollFds_[SP_THREAD_EVENT_FD_].fd = eventFd;
    SpThread_pollFds_[SP_THREAD_SERIAL_FD_].fd = -1;
}

void SpThreadWake_setSerialFd(int const serialFd) {
    DBC_REQUIRE(200, serialFd >= -1);
    SpThread_pollFds_[SP_THREAD_SERIAL_FD_].fd = serialFd;
}

int SpThreadWake_wait(int const timeoutMs) {
    DBC_REQUIRE(300,
                SpThread_pollFds_[SP_THREAD_EVENT_FD_].fd >= 0);
    DBC_REQUIRE(301, timeoutMs >= -1);

    int const ready = poll(SpThread_pollFds_,
                           SP_THREAD_FD_COUNT_, timeoutMs);
    if (ready < 0) {
        return SP_THREAD_WAKE_ERROR;
    }
    if (ready == 0) {
        return SP_THREAD_WAKE_TIMEOUT;
    }

    short const eventRevents =
        SpThread_pollFds_[SP_THREAD_EVENT_FD_].revents;
    short const serialRevents =
        SpThread_pollFds_[SP_THREAD_SERIAL_FD_].revents;
    if (((eventRevents & (POLLERR | POLLHUP | POLLNVAL)) != 0)
        || ((serialRevents & POLLNVAL) != 0))
    {
        errno = EIO;
        return SP_THREAD_WAKE_ERROR;
    }

    int result = SP_THREAD_WAKE_TIMEOUT;
    if ((eventRevents & POLLIN) != 0) {
        result |= SP_THREAD_WAKE_EVENT;
    }
    if ((serialRevents & (POLLIN | POLLERR | POLLHUP)) != 0) {
        result |= SP_THREAD_WAKE_SERIAL;
    }
    if (result == SP_THREAD_WAKE_TIMEOUT) {
        errno = EIO;
        return SP_THREAD_WAKE_ERROR;
    }
    return result;
}
