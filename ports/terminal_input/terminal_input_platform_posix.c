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
//=== Component: TerminalInputPlatform (POSIX)
#include <limits.h>
#include <time.h>
#include "dbc_assert.h"
#include "terminal_input_platform.h"
DBC_MODULE_NAME("terminal_input_platform")

static struct notcurses *TerminalInputPlatform_nc_;
static PlatformWaitObject TerminalInputPlatform_wait_ =
    PLATFORM_WAIT_OBJECT_INITIALIZER;

int TerminalInputPlatform_init(struct notcurses * const nc) {
    DBC_REQUIRE(200, nc != (struct notcurses *)0);
    DBC_REQUIRE(201,
                TerminalInputPlatform_nc_ == (struct notcurses *)0);

    int const descriptor = notcurses_inputready_fd(nc);
    if (descriptor < 0) {
        return 1;
    }
    TerminalInputPlatform_nc_ = nc;
    TerminalInputPlatform_wait_ =
        PlatformWaitObject_fromDescriptor(descriptor);
    return 0;
}

int TerminalInputPlatform_deinit(void) {
    DBC_REQUIRE(210,
                TerminalInputPlatform_nc_ != (struct notcurses *)0);

    TerminalInputPlatform_nc_ = (struct notcurses *)0;
    TerminalInputPlatform_wait_ = PLATFORM_WAIT_OBJECT_INVALID;
    return 0;
}

PlatformWaitObject TerminalInputPlatform_waitObject(void) {
    return TerminalInputPlatform_wait_;
}

int TerminalInputPlatform_read(ncinput * const input,
                               size_t const capacity)
{
    DBC_REQUIRE(300, input != (ncinput *)0);
    DBC_REQUIRE(301, (capacity > 0U) && (capacity <= (size_t)INT_MAX));

    struct timespec const deadline = {0};
    return notcurses_getvec(TerminalInputPlatform_nc_, &deadline,
                            input, (int)capacity);
}
