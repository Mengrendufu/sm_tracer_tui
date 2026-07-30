//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#ifndef SM_UI_KEY_H_
#define SM_UI_KEY_H_

#include "sm_hsm.h"
#include "sm_ui_evt.h"

struct InputComposer;

//============================================================================
//=== Key HSM — event-driven input editing state owner

typedef struct {
    SM_Hsm super;
    struct InputComposer *composer; // borrowed from SM_UI
} SM_UI_Key;

void SM_UI_Key_ctor(SM_UI_Key *me, struct InputComposer *composer);
void SM_UI_Key_init(SM_UI_Key *me);
// One-way state-machine event dispatch; no action result is returned.
void SM_UI_Key_dispatchEvt(SM_UI_Key *me, UI_Evt const *e);

#endif // SM_UI_KEY_H_
