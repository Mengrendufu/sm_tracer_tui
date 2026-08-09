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
// Provides the UI-thread terminal-input contract while delegating native
// readiness and input acquisition to TerminalInputPlatform.
#include <limits.h>
#include "dbc_assert.h"
#include "terminal_input_platform.h"
#include "ui_terminal_input_priv.h"
DBC_MODULE_NAME("ui_terminal_input")

int UI_TerminalInput_init(struct notcurses * const nc) {
    DBC_REQUIRE(200, nc != (struct notcurses *)0);
    return TerminalInputPlatform_init(nc);
}

int UI_TerminalInput_deinit(void) {
    return TerminalInputPlatform_deinit();
}

PlatformWaitObject UI_TerminalInput_waitObject(void) {
    return TerminalInputPlatform_waitObject();
}

int UI_TerminalInput_read(ncinput * const input,
                          size_t const capacity)
{
    DBC_REQUIRE(300, input != (ncinput *)0);
    DBC_REQUIRE(301, (capacity > 0U) && (capacity <= (size_t)INT_MAX));
    return TerminalInputPlatform_read(input, capacity);
}
