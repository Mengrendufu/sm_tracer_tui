//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#ifndef UI_HSM_H_
#define UI_HSM_H_

#include <stdbool.h>
#include <time.h>
#include "sm_hsm.h"

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
    bool quit;
    bool dirty;
    struct timespec lastRender;
} UI_AO;

void UI_AO_ctor(UI_AO *me);
void UI_AO_init(UI_AO *me);

#endif // UI_HSM_H_
