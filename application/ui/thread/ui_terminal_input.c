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
//=== Component: UITerminalInput
//
// POSIX lends notcurses' readiness descriptor directly. Windows notcurses
// has no public waitable input object, so a bridge thread waits in notcurses
// and projects parsed ncinput values through an Event.
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include "dbc_assert.h"
#include "platform_port.h"
#include "ui_terminal_input_priv.h"
DBC_MODULE_NAME("ui_terminal_input")

#if defined(_WIN32)

#define UI_TERMINAL_INPUT_QLEN_ 512U
#define UI_TERMINAL_INPUT_POLL_MS_ 50U

struct UI_TerminalInputQueue {
    ncinput buffer[UI_TERMINAL_INPUT_QLEN_];
    uint16_t head;
    uint16_t tail;
    uint16_t used;
    PlatformMutex mutex;
};

struct UI_TerminalInput {
    struct notcurses *nc;
    struct UI_TerminalInputQueue queue;
    PlatformSemaphore slots;
    PlatformWake wake;
    PlatformThread thread;
    bool failed;
    bool stopping;
};

static struct UI_TerminalInput UI_terminalInput_ = {
    .queue = {.mutex = PLATFORM_MUTEX_INITIALIZER},
    .slots = PLATFORM_SEMAPHORE_INITIALIZER,
    .wake = PLATFORM_WAKE_INITIALIZER,
    .thread = PLATFORM_THREAD_INITIALIZER,
};

static void UI_TerminalInput_fail_(void) {
    int status = PlatformMutex_lock(&UI_terminalInput_.queue.mutex);
    DBC_ASSERT(100, status == 0);
    UI_terminalInput_.failed = true;
    status = PlatformMutex_unlock(&UI_terminalInput_.queue.mutex);
    DBC_ASSERT(101, status == 0);
    status = PlatformWake_signal(&UI_terminalInput_.wake);
    (void)status;
}

static bool UI_TerminalInput_isStopping_(
    struct UI_TerminalInput * const me)
{
    int status = PlatformMutex_lock(&me->queue.mutex);
    DBC_ASSERT(105, status == 0);
    bool const stopping = me->stopping;
    status = PlatformMutex_unlock(&me->queue.mutex);
    DBC_ASSERT(106, status == 0);
    return stopping;
}

static void UI_TerminalInput_run_(void * const ctx) {
    struct UI_TerminalInput * const me =
        (struct UI_TerminalInput *)ctx;

    for (;;) {
        // MinGW notcurses uses a default CLOCK_REALTIME condition variable.
        struct timespec deadline;
        if (clock_gettime(CLOCK_REALTIME, &deadline) != 0) {
            UI_TerminalInput_fail_();
            return;
        }
        deadline.tv_nsec +=
            (long)UI_TERMINAL_INPUT_POLL_MS_ * 1000000L;
        if (deadline.tv_nsec >= 1000000000L) {
            ++deadline.tv_sec;
            deadline.tv_nsec -= 1000000000L;
        }

        ncinput input;
        uint32_t const id = notcurses_get(me->nc, &deadline, &input);
        if (UI_TerminalInput_isStopping_(me)) {
            return;
        }
        if (id == (uint32_t)-1) {
            UI_TerminalInput_fail_();
            return;
        }
        if (id == 0U) {
            continue;
        }

        if (PlatformSemaphore_wait(&me->slots) != 0) {
            UI_TerminalInput_fail_();
            return;
        }
        if (UI_TerminalInput_isStopping_(me)) {
            return;
        }

        int status = PlatformMutex_lock(&me->queue.mutex);
        DBC_ASSERT(110, status == 0);
        DBC_ASSERT(111, me->queue.used < UI_TERMINAL_INPUT_QLEN_);
        me->queue.buffer[me->queue.head] = input;
        me->queue.head = (uint16_t)(
            (me->queue.head + 1U) % UI_TERMINAL_INPUT_QLEN_);
        ++me->queue.used;
        status = PlatformMutex_unlock(&me->queue.mutex);
        DBC_ASSERT(112, status == 0);

        status = PlatformWake_signal(&me->wake);
        (void)status;
        if (id == NCKEY_EOF) {
            return;
        }
    }
}

int UI_TerminalInput_init(struct notcurses * const nc) {
    DBC_REQUIRE(200, nc != (struct notcurses *)0);
    DBC_REQUIRE(201, UI_terminalInput_.nc == (struct notcurses *)0);

    if (PlatformSemaphore_init(&UI_terminalInput_.slots,
                               UI_TERMINAL_INPUT_QLEN_) != 0)
    {
        return 1;
    }
    if (PlatformWake_init(&UI_terminalInput_.wake) != 0) {
        PlatformSemaphore_deinit(&UI_terminalInput_.slots);
        return 1;
    }

    UI_terminalInput_.nc = nc;
    UI_terminalInput_.failed = false;
    UI_terminalInput_.stopping = false;
    if (PlatformThread_start(&UI_terminalInput_.thread,
                             &UI_TerminalInput_run_,
                             &UI_terminalInput_) != 0)
    {
        UI_terminalInput_.nc = (struct notcurses *)0;
        PlatformWake_deinit(&UI_terminalInput_.wake);
        PlatformSemaphore_deinit(&UI_terminalInput_.slots);
        return 1;
    }
    return 0;
}

int UI_TerminalInput_deinit(void) {
    DBC_REQUIRE(210,
                UI_terminalInput_.nc != (struct notcurses *)0);

    int status = PlatformMutex_lock(&UI_terminalInput_.queue.mutex);
    DBC_ASSERT(211, status == 0);
    UI_terminalInput_.stopping = true;
    status = PlatformMutex_unlock(&UI_terminalInput_.queue.mutex);
    DBC_ASSERT(212, status == 0);

    // Release a producer that might be waiting for queue capacity.
    (void)PlatformSemaphore_post(&UI_terminalInput_.slots);
    if (PlatformThread_join(&UI_terminalInput_.thread) != 0) {
        return 1;
    }

    PlatformWake_deinit(&UI_terminalInput_.wake);
    PlatformSemaphore_deinit(&UI_terminalInput_.slots);
    UI_terminalInput_.nc = (struct notcurses *)0;
    UI_terminalInput_.queue.head = 0U;
    UI_terminalInput_.queue.tail = 0U;
    UI_terminalInput_.queue.used = 0U;
    return 0;
}

PlatformWaitObject UI_TerminalInput_waitObject(void) {
    return PlatformWake_waitObject(&UI_terminalInput_.wake);
}

int UI_TerminalInput_read(ncinput * const input,
                          size_t const capacity)
{
    DBC_REQUIRE(300, input != (ncinput *)0);
    DBC_REQUIRE(301, (capacity > 0U) && (capacity <= (size_t)INT_MAX));

    int status = PlatformMutex_lock(&UI_terminalInput_.queue.mutex);
    DBC_ASSERT(302, status == 0);

    size_t count = 0U;
    while ((count < capacity) && (UI_terminalInput_.queue.used > 0U)) {
        input[count] = UI_terminalInput_.queue.buffer[
            UI_terminalInput_.queue.tail];
        UI_terminalInput_.queue.tail = (uint16_t)(
            (UI_terminalInput_.queue.tail + 1U)
            % UI_TERMINAL_INPUT_QLEN_);
        --UI_terminalInput_.queue.used;
        ++count;
    }
    bool const failed = UI_terminalInput_.failed && (count == 0U);

    status = PlatformMutex_unlock(&UI_terminalInput_.queue.mutex);
    DBC_ASSERT(303, status == 0);

    for (size_t i = 0U; i < count; ++i) {
        status = PlatformSemaphore_post(&UI_terminalInput_.slots);
        DBC_ASSERT(304, status == 0);
    }
    (void)status;
    return failed ? -1 : (int)count;
}

#else

static struct notcurses *UI_terminalInputNc_;
static PlatformWaitObject UI_terminalInputWait_ =
    PLATFORM_WAIT_OBJECT_INITIALIZER;

int UI_TerminalInput_init(struct notcurses * const nc) {
    DBC_REQUIRE(200, nc != (struct notcurses *)0);
    DBC_REQUIRE(201, UI_terminalInputNc_ == (struct notcurses *)0);

    int const descriptor = notcurses_inputready_fd(nc);
    if (descriptor < 0) {
        return 1;
    }
    UI_terminalInputNc_ = nc;
    UI_terminalInputWait_ =
        PlatformWaitObject_fromDescriptor(descriptor);
    return 0;
}

int UI_TerminalInput_deinit(void) {
    DBC_REQUIRE(210,
                UI_terminalInputNc_ != (struct notcurses *)0);

    UI_terminalInputNc_ = (struct notcurses *)0;
    UI_terminalInputWait_ = PLATFORM_WAIT_OBJECT_INVALID;
    return 0;
}

PlatformWaitObject UI_TerminalInput_waitObject(void) {
    return UI_terminalInputWait_;
}

int UI_TerminalInput_read(ncinput * const input,
                          size_t const capacity)
{
    DBC_REQUIRE(300, input != (ncinput *)0);
    DBC_REQUIRE(301, (capacity > 0U) && (capacity <= (size_t)INT_MAX));

    struct timespec const deadline = {0};
    return notcurses_getvec(UI_terminalInputNc_, &deadline, input,
                            (int)capacity);
}

#endif
