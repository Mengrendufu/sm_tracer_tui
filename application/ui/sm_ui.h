//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#ifndef SM_UI_H_
#define SM_UI_H_

#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include "sm_hsm.h"
#include "sm_ui_key.h"
#include "text_buffer_view.h"

struct notcurses;
struct ncplane;

//============================================================================
//=== Menu: popup panel + selection state.
struct Menu {
    struct ncplane *plane;
    uint32_t        sel;
    bool            visible;
};

//============================================================================
//=== notcurses display tree.
struct NcDisp {
    struct notcurses *nc;
    struct ncplane   *titlePlane;
    struct ncplane   *statusPlane;
    struct TextBufferView mainBuffer;
    struct ncplane   *keybarPlane;
    struct Menu       menu;
};

//============================================================================
//=== UI Active Object — HSM host
typedef struct {
    // --- HSM virtual table ---
    SM_Hsm super;
    VC_Handler init;
    VC_Handler dispatch;

    // --- notcurses display ---
    struct NcDisp disp;

    // --- command substate machine ---
    SM_UI_Key cmdHsm;

    bool quit;
    bool dirty;
    struct timespec lastRender;
} SM_UI;

void SM_UI_ctor(SM_UI *me);
void SM_UI_start(SM_UI *me);

#endif // SM_UI_H_
