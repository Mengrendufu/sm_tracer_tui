//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
//============================================================================
//=== Key HSM — placeholder: idle state only
#include "sm_port.h"
#include "sm_hsm.h"
#include "dbc_assert.h"
#include "ui_evt.h"
#include "sm_ui_key.h"
DBC_MODULE_NAME("sm_ui_key")

//============================================================================
//=== States

static SM_StatePtr SM_UI_Key_TOP_initial(SM_Hsm *me) SM_HSM_RETT;

static void        SM_UI_Key_idle_entry_(SM_Hsm *me) SM_HSM_RETT;
static SM_RetState SM_UI_Key_idle_(SM_Hsm *me, void const *e) SM_HSM_RETT;
SM_HsmState SM_HSM_ROM SM_UI_Key_idle = {
    (SM_StatePtr)0,                     // super (top)
    (SM_InitHandler)0,                  // init_ (leaf)
    (SM_ActionHandler)&SM_UI_Key_idle_entry_, // entry_
    (SM_ActionHandler)0,                // exit_
    (SM_StateHandler)&SM_UI_Key_idle_         // handler_
};

//============================================================================
//=== HSM implementations

static SM_StatePtr SM_UI_Key_TOP_initial(SM_Hsm * const me) SM_HSM_RETT {
    (void)me;
    return _SM_INIT(&SM_UI_Key_idle);
}

static void SM_UI_Key_idle_entry_(SM_Hsm * const me) SM_HSM_RETT {
    (void)me;
}

static SM_RetState SM_UI_Key_idle_(SM_Hsm * const me, void const * const e) {
    (void)me;
    (void)e;
    return _SM_SUPER();
}

//============================================================================
//=== Constructor

void SM_UI_Key_ctor(SM_UI_Key * const me) {
    DBC_REQUIRE(100, me != (SM_UI_Key *)0);
    (void)me;
}

void SM_UI_Key_init(SM_UI_Key * const me) {
    DBC_REQUIRE(200, me != (SM_UI_Key *)0);
    SM_Hsm_init_(&me->super, (SM_InitHandler)SM_UI_Key_TOP_initial);
}
