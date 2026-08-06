//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#include <stdint.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include "sp_thread/thread/sp_thread_wake_priv.h"

int main(void) {
    int failed = 0;
    int const eventFd = eventfd(0U, EFD_NONBLOCK | EFD_CLOEXEC);
    int serialPipe[2];
    failed += (eventFd >= 0) && (pipe(serialPipe) == 0) ? 0 : 1;
    if (failed != 0) {
        return 1;
    }

    SpThreadWake_init(PlatformWaitObject_fromDescriptor(eventFd));
    SpThreadWake_setSerialObject(
        PlatformWaitObject_fromDescriptor(serialPipe[0]));

    uint64_t const wake = 1U;
    failed += write(eventFd, &wake, sizeof(wake)) == sizeof(wake)
              ? 0 : 1;
    int ready = SpThreadWake_wait(20);
    failed += (ready & SP_THREAD_WAKE_EVENT) != 0 ? 0 : 1;
    uint64_t consumed;
    failed += read(eventFd, &consumed, sizeof(consumed))
              == sizeof(consumed) ? 0 : 1;

    char const byte = 'x';
    failed += write(serialPipe[1], &byte, sizeof(byte)) == sizeof(byte)
              ? 0 : 1;
    ready = SpThreadWake_wait(20);
    failed += (ready & SP_THREAD_WAKE_SERIAL) != 0 ? 0 : 1;
    char received;
    failed += read(serialPipe[0], &received, sizeof(received))
              == sizeof(received) ? 0 : 1;

    ready = SpThreadWake_wait(1);
    failed += ready == SP_THREAD_WAKE_TIMEOUT ? 0 : 1;

    (void)close(serialPipe[1]);
    ready = SpThreadWake_wait(20);
    failed += (ready & SP_THREAD_WAKE_SERIAL_LOST) != 0 ? 0 : 1;

    SpThreadWake_setSerialObject(PLATFORM_WAIT_OBJECT_INVALID);
    ready = SpThreadWake_wait(1);
    failed += ready == SP_THREAD_WAKE_TIMEOUT ? 0 : 1;

    (void)close(serialPipe[0]);
    (void)close(eventFd);
    return failed == 0 ? 0 : 1;
}
