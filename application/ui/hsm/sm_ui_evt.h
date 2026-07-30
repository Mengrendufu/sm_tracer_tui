//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#ifndef SM_UI_EVT_H_
#define SM_UI_EVT_H_

#include <stddef.h>
#include <stdint.h>
#include "ui_input.h"

//============================================================================
//=== Event contract shared by the UI thread and UI HSM

typedef uint16_t UI_Signal;

typedef struct {
    UI_Signal sig;
} UI_Evt;

typedef struct {
    UI_Evt super;
    union {
        struct {
            size_t len;
            char  *text;
        } msg;
    } pld;
} UI_AppEvt;

typedef struct {
    UI_Evt super;
    UI_Input input;
} UI_InputEvt;

enum UIEventSignals {
    UI_NULL_SIG = 0,

    // Terminal input signals produced by UI_InputRouter.
    // delivery to input composer manager
    UI_INPUT_SIG,

    UI_KEY_ESC_SIG,
    UI_KEY_CTRL_SLASH_SIG,
    UI_KEY_UP_SIG,
    UI_KEY_DOWN_SIG,
    UI_KEY_ENTER_SIG,
    UI_KEY_J_SIG,
    UI_KEY_K_SIG,
    UI_KEY_CTRL_N_SIG,
    UI_KEY_CTRL_P_SIG,
    UI_KEY_PGUP_SIG,
    UI_KEY_PGDN_SIG,
    UI_RESIZE_SIG,

    UI_TIMER_SIG,
    UI_TEXT_SIG,
};

#endif // SM_UI_EVT_H_
