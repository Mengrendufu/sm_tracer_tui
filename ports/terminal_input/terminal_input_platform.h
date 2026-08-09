//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#ifndef TERMINAL_INPUT_PLATFORM_H_
#define TERMINAL_INPUT_PLATFORM_H_

#include <stddef.h>
#include <notcurses/notcurses.h>
#include "platform_port.h"

//============================================================================
//=== Component: TerminalInputPlatform
//=== Interface: ITerminalInputPlatform

// Bind the borrowed notcurses runtime and prepare terminal readiness.
int TerminalInputPlatform_init(struct notcurses *nc);

// Stop owned input infrastructure before the borrowed runtime is destroyed.
int TerminalInputPlatform_deinit(void);

// Return the borrowed wait object representing parsed notcurses input.
PlatformWaitObject TerminalInputPlatform_waitObject(void);

// Read up to capacity parsed inputs without blocking. Return a count or -1.
int TerminalInputPlatform_read(ncinput *input, size_t capacity);

#endif // TERMINAL_INPUT_PLATFORM_H_
