//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#ifndef SM_INPUT_COMPOSER_MANAGER_H_
#define SM_INPUT_COMPOSER_MANAGER_H_

#include "sm_hsm.h"
#include "sm_ui_evt.h"

struct InputComposer;

//============================================================================
//=== InputComposer Manager HSM -- input editing state owner

typedef struct {
    SM_Hsm super;
    struct InputComposer *composer; // borrowed from SM_UI
} SM_InputComposerManager;

void SM_InputComposerManager_ctor(
    SM_InputComposerManager *me,
    struct InputComposer *composer);
void SM_InputComposerManager_init(SM_InputComposerManager *me);
// One-way state-machine event dispatch; no action result is returned.
void SM_InputComposerManager_dispatchEvt(
    SM_InputComposerManager *me,
    UI_Evt const *e);

#endif // SM_INPUT_COMPOSER_MANAGER_H_
