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

// UI event — single type, payload via union
typedef struct {
    UI_Signal sig;
    union {
        uint32_t key;    // UI_KEY_SIG
        struct {
            size_t len;
            char  *text; // points to extra alloc past struct
        } msg;           // UI_BLINKY_TEXT_SIG etc
    } pld;
} UI_Evt;

// UI signal values
enum {
    UI_NULL_SIG = 0,
    UI_KEY_SIG,
    UI_QUIT_SIG,
    UI_TIMER_SIG,
    UI_BLINKY_TEXT_SIG,
    UI_CMD_ACTIVE_SIG,
    UI_CMD_IDLE_SIG,
    UI_CMD_QUIT_SIG,
    UI_CMD_MENU_SIG,
    UI_CMD_CONNECT_SIG,
};

// Post a signal-only event to the UI (thread-safe, callable from SST AOs)
void UI_postSignal(UI_Signal sig);

// Post a text event to the UI (thread-safe, callable from SST AOs)
void UI_postText(UI_Signal sig, char const *text, size_t len);

// Initialize UI planes, eventfd, tick registration (call before SST starts)
void UI_prepare(struct notcurses *nc);

// Run the UI event loop (blocks until quit)
void UI_loop(void);

#endif // UI_H_
