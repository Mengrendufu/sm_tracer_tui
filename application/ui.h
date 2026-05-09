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

#include <stdint.h>
#include <notcurses/notcurses.h>

//============================================================================
//=== UI module — main thread event loop, notcurses rendering, sm_hsm host

// UI signal type (independent from SST)
typedef uint16_t UI_Signal;

// UI event base type
typedef struct {
    UI_Signal sig;
} UI_Evt;

// UI key event
typedef struct {
    UI_Evt super;
    uint32_t key;
} UI_KeyEvt;

// UI signal values
enum {
    UI_NULL_SIG = 0,
    UI_KEY_SIG,
    UI_QUIT_SIG,
};

// Post a signal-only event to the UI (thread-safe, callable from SST AOs)
void UI_postSignal(UI_Signal sig);

// Run the UI main loop (blocks until quit)
void UI_run(struct notcurses *nc);

#endif // UI_H_
