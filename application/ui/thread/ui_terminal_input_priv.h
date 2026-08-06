//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#ifndef UI_TERMINAL_INPUT_PRIV_H_
#define UI_TERMINAL_INPUT_PRIV_H_

#include <stddef.h>
#include <notcurses/notcurses.h>
#include "platform_port.h"

// Bind the borrowed notcurses runtime and prepare terminal readiness.
int UI_TerminalInput_init(struct notcurses *nc);

// Return the borrowed wait object representing parsed notcurses input.
PlatformWaitObject UI_TerminalInput_waitObject(void);

// Read up to capacity parsed inputs without blocking. Return a count or -1.
int UI_TerminalInput_read(ncinput *input, size_t capacity);

#endif // UI_TERMINAL_INPUT_PRIV_H_
