//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#ifndef UI_H_
#define UI_H_

//============================================================================
//=== UI module — main thread event loop, notcurses rendering

// Initialize notcurses, UI planes, input/wake infrastructure, and tick hook.
// Returns 0 on success, nonzero on failure.
int UI_init(void);

// Run the UI event loop (blocks until quit), then cleanup notcurses.
// Returns 0 on normal quit, nonzero on failure.
int UI_run(void);

#endif // UI_H_
