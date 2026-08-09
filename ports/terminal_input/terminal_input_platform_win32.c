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
//=== Component: TerminalInputPlatform (Win32)
//
// MinGW notcurses has no public waitable input object. A bridge thread waits
// in notcurses and projects parsed ncinput values through a platform Wake.
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <windows.h>
#include "dbc_assert.h"
#include "terminal_input_geometry_priv.h"
#include "terminal_input_platform.h"
DBC_MODULE_NAME("terminal_input_platform")

#define TERMINAL_INPUT_QLEN_ 512U
#define TERMINAL_INPUT_POLL_MS_ 50U

struct TerminalInputQueue {
    ncinput buffer[TERMINAL_INPUT_QLEN_];
    uint16_t head;
    uint16_t tail;
    uint16_t used;
    PlatformMutex mutex;
};

struct TerminalInputPlatform {
    struct notcurses *nc;
    struct TerminalInputQueue queue;
    PlatformSemaphore slots;
    PlatformWake wake;
    PlatformThread thread;
    TerminalInputGeometry geometry;
    bool failed;
    bool stopping;
};

static struct TerminalInputPlatform TerminalInputPlatform_instance_ = {
    .queue = {.mutex = PLATFORM_MUTEX_INITIALIZER},
    .slots = PLATFORM_SEMAPHORE_INITIALIZER,
    .wake = PLATFORM_WAKE_INITIALIZER,
    .thread = PLATFORM_THREAD_INITIALIZER,
};

static void TerminalInputPlatform_fail_(void) {
    int status = PlatformMutex_lock(
        &TerminalInputPlatform_instance_.queue.mutex);
    DBC_ASSERT(100, status == 0);
    TerminalInputPlatform_instance_.failed = true;
    status = PlatformMutex_unlock(
        &TerminalInputPlatform_instance_.queue.mutex);
    DBC_ASSERT(101, status == 0);
    status = PlatformWake_signal(&TerminalInputPlatform_instance_.wake);
    (void)status;
}

static bool TerminalInputPlatform_isStopping_(
    struct TerminalInputPlatform * const me)
{
    int status = PlatformMutex_lock(&me->queue.mutex);
    DBC_ASSERT(105, status == 0);
    bool const stopping = me->stopping;
    status = PlatformMutex_unlock(&me->queue.mutex);
    DBC_ASSERT(106, status == 0);
    return stopping;
}

static bool TerminalInputPlatform_queryGeometry_(
    TerminalInputGeometry * const geometry)
{
    HANDLE const output = GetStdHandle(STD_OUTPUT_HANDLE);
    if ((output == NULL) || (output == INVALID_HANDLE_VALUE)) {
        return false;
    }

    CONSOLE_SCREEN_BUFFER_INFO info;
    if (GetConsoleScreenBufferInfo(output, &info) == FALSE) {
        return false;
    }

    geometry->rows = (unsigned)(info.srWindow.Bottom
                                - info.srWindow.Top + 1);
    geometry->cols = (unsigned)info.dwSize.X;
    return (geometry->rows > 0U) && (geometry->cols > 0U);
}

static bool TerminalInputPlatform_enqueue_(
    struct TerminalInputPlatform * const me,
    ncinput const * const input)
{
    if (PlatformSemaphore_wait(&me->slots) != 0) {
        TerminalInputPlatform_fail_();
        return false;
    }
    if (TerminalInputPlatform_isStopping_(me)) {
        (void)PlatformSemaphore_post(&me->slots);
        return false;
    }

    int status = PlatformMutex_lock(&me->queue.mutex);
    DBC_ASSERT(110, status == 0);
    DBC_ASSERT(111, me->queue.used < TERMINAL_INPUT_QLEN_);
    me->queue.buffer[me->queue.head] = *input;
    me->queue.head = (uint16_t)(
        (me->queue.head + 1U) % TERMINAL_INPUT_QLEN_);
    ++me->queue.used;
    status = PlatformMutex_unlock(&me->queue.mutex);
    DBC_ASSERT(112, status == 0);

    status = PlatformWake_signal(&me->wake);
    (void)status;
    return true;
}

static void TerminalInputPlatform_run_(void * const ctx) {
    struct TerminalInputPlatform * const me =
        (struct TerminalInputPlatform *)ctx;

    for (;;) {
        // MinGW notcurses uses a default CLOCK_REALTIME condition variable.
        struct timespec deadline;
        if (clock_gettime(CLOCK_REALTIME, &deadline) != 0) {
            TerminalInputPlatform_fail_();
            return;
        }
        deadline.tv_nsec +=
            (long)TERMINAL_INPUT_POLL_MS_ * 1000000L;
        if (deadline.tv_nsec >= 1000000000L) {
            ++deadline.tv_sec;
            deadline.tv_nsec -= 1000000000L;
        }

        ncinput input;
        uint32_t const id = notcurses_get(me->nc, &deadline, &input);
        if (TerminalInputPlatform_isStopping_(me)) {
            return;
        }

        TerminalInputGeometry geometry;
        if (!TerminalInputPlatform_queryGeometry_(&geometry)) {
            TerminalInputPlatform_fail_();
            return;
        }
        if (TerminalInputGeometry_update(&me->geometry, geometry)) {
            ncinput const resize = {.id = NCKEY_RESIZE};
            if (!TerminalInputPlatform_enqueue_(me, &resize)) {
                return;
            }
        }

        if (id == (uint32_t)-1) {
            TerminalInputPlatform_fail_();
            return;
        }
        if (id == 0U) {
            continue;
        }

        if (!TerminalInputPlatform_enqueue_(me, &input)) {
            return;
        }
        if (id == NCKEY_EOF) {
            return;
        }
    }
}

int TerminalInputPlatform_init(struct notcurses * const nc) {
    struct TerminalInputPlatform * const me =
        &TerminalInputPlatform_instance_;
    DBC_REQUIRE(200, nc != (struct notcurses *)0);
    DBC_REQUIRE(201, me->nc == (struct notcurses *)0);

    if (PlatformSemaphore_init(&me->slots,
                               TERMINAL_INPUT_QLEN_) != 0)
    {
        return 1;
    }
    if (PlatformWake_init(&me->wake) != 0) {
        PlatformSemaphore_deinit(&me->slots);
        return 1;
    }
    if (!TerminalInputPlatform_queryGeometry_(&me->geometry)) {
        PlatformWake_deinit(&me->wake);
        PlatformSemaphore_deinit(&me->slots);
        return 1;
    }

    me->nc = nc;
    me->failed = false;
    me->stopping = false;
    if (PlatformThread_start(&me->thread,
                             &TerminalInputPlatform_run_,
                             me) != 0)
    {
        me->nc = (struct notcurses *)0;
        PlatformWake_deinit(&me->wake);
        PlatformSemaphore_deinit(&me->slots);
        return 1;
    }
    return 0;
}

int TerminalInputPlatform_deinit(void) {
    struct TerminalInputPlatform * const me =
        &TerminalInputPlatform_instance_;
    DBC_REQUIRE(210, me->nc != (struct notcurses *)0);

    int status = PlatformMutex_lock(&me->queue.mutex);
    DBC_ASSERT(211, status == 0);
    me->stopping = true;
    status = PlatformMutex_unlock(&me->queue.mutex);
    DBC_ASSERT(212, status == 0);

    // Release a producer that might be waiting for queue capacity.
    (void)PlatformSemaphore_post(&me->slots);
    if (PlatformThread_join(&me->thread) != 0) {
        return 1;
    }

    PlatformWake_deinit(&me->wake);
    PlatformSemaphore_deinit(&me->slots);
    me->nc = (struct notcurses *)0;
    me->queue.head = 0U;
    me->queue.tail = 0U;
    me->queue.used = 0U;
    return 0;
}

PlatformWaitObject TerminalInputPlatform_waitObject(void) {
    return PlatformWake_waitObject(
        &TerminalInputPlatform_instance_.wake);
}

int TerminalInputPlatform_read(ncinput * const input,
                               size_t const capacity)
{
    struct TerminalInputPlatform * const me =
        &TerminalInputPlatform_instance_;
    DBC_REQUIRE(300, input != (ncinput *)0);
    DBC_REQUIRE(301, (capacity > 0U) && (capacity <= (size_t)INT_MAX));

    int status = PlatformMutex_lock(&me->queue.mutex);
    DBC_ASSERT(302, status == 0);

    size_t count = 0U;
    while ((count < capacity) && (me->queue.used > 0U)) {
        input[count] = me->queue.buffer[me->queue.tail];
        me->queue.tail = (uint16_t)(
            (me->queue.tail + 1U) % TERMINAL_INPUT_QLEN_);
        --me->queue.used;
        ++count;
    }
    bool const failed = me->failed && (count == 0U);

    status = PlatformMutex_unlock(&me->queue.mutex);
    DBC_ASSERT(303, status == 0);

    for (size_t i = 0U; i < count; ++i) {
        status = PlatformSemaphore_post(&me->slots);
        DBC_ASSERT(304, status == 0);
    }
    (void)status;
    return failed ? -1 : (int)count;
}
