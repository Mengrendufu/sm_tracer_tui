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
#include <time.h>
#include "sm_hsm.h"
#include "sm_ui_key.h"

//============================================================================
//=== UI Active Object — HSM host

typedef struct {
    SM_Hsm super;
    VC_Handler init;     // virtual: → SM_Hsm_init_
    VC_Handler dispatch; // virtual: → SM_Hsm_dispatch_

    struct notcurses *nc;
    struct ncplane   *statusPlane;
    struct ncplane   *mainPlane;
    struct ncplane   *keybarPlane;
    uint32_t          mainLineCnt;
    bool quit;
    bool dirty;
    struct timespec lastRender;

    // command subsystem (self-contained)
    SM_UI_Key cmdHsm;
} SM_UI;

void SM_UI_ctor(SM_UI *me);
void SM_UI_start(SM_UI *me);

#endif // SM_UI_H_
