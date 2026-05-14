//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#ifndef UI_EVT_H_
#define UI_EVT_H_

#include <stddef.h>
#include <stdint.h>

//============================================================================
//=== UI event system — independent from notcurses, usable from SST AOs

// UI signal type (independent from SST)
typedef uint16_t UI_Signal;

// Base UI event — signal only, used in HSM handler signatures
typedef struct {
    UI_Signal sig;
} UI_Evt;

// Application event with payload — extends UI_Evt, allocated when data travels
typedef struct {
    UI_Evt super;
    union {
        struct {
            size_t len;
            char  *text; // points to extra alloc past struct
        } msg;
    } pld;
} UI_AppEvt;

// UI signal values — concrete key combos, translated by UI_routeInput_
enum {
    UI_NULL_SIG = 0,
    UI_KEY_ESC_SIG,       // ESC
    UI_KEY_CTRL_SLASH_SIG, // Ctrl+/ (also Ctrl+_ in terminal)
    UI_KEY_UP_SIG,          // arrow up
    UI_KEY_DOWN_SIG,        // arrow down
    UI_KEY_ENTER_SIG,       // enter / return
    UI_KEY_J_SIG,           // j
    UI_KEY_K_SIG,           // k
    UI_KEY_PGUP_SIG,        // page up
    UI_KEY_PGDN_SIG,        // page down
    UI_KEY_DEBUG_SIG,        // any other key — routed to mainPlane for debug
    UI_TIMER_SIG,
    UI_BLINKY_TEXT_SIG,
    UI_RESIZE_SIG,
};

// Post a signal-only event to the UI (thread-safe, callable from SST AOs)
void UI_postSignal(UI_Signal sig);

// Post a text event to the UI (thread-safe, callable from SST AOs)
void UI_postText(UI_Signal sig, char const *text);

// --- internal: dequeue for the main loop ---

// Opaque event queue handle (defined in ui_evt.c)
typedef struct UI_EvtQueue UI_EvtQueue;

// Initialize the event subsystem (call once before any post)
void UI_evtInit(void);

// Return the eventfd for poll (call after UI_evtInit)
int UI_evtFd(void);

// Dequeue one event (returns NULL if empty)
UI_Evt *UI_evtDequeue(void);

// Free an event (mirrors the internal allocator)
void UI_evtFree(UI_Evt *e);

#endif // UI_EVT_H_
