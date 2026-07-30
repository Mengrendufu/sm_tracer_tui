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
//=== InputComposer Manager HSM -- input editing state owner
#include "sm_port.h"
#include "sm_hsm.h"
#include "dbc_assert.h"
#include "widgets/input_composer.h"
#include "sm_ui_evt.h"
#include "sm_input_composer_manager.h"
DBC_MODULE_NAME("sm_input_composer_manager")

//============================================================================
//=== States

static SM_StatePtr SM_InputComposerManager_TOP_initial(SM_Hsm *me) SM_HSM_RETT;

static void        SM_InputComposerManager_idle_entry_(SM_Hsm *me) SM_HSM_RETT;
static SM_RetState SM_InputComposerManager_idle_(SM_Hsm *me, UI_Evt const *e) SM_HSM_RETT;
SM_HsmState SM_HSM_ROM SM_InputComposerManager_idle = {
    (SM_StatePtr)0,                                  // super (top)
    (SM_InitHandler)0,                               // init_ (leaf)
    (SM_ActionHandler)&SM_InputComposerManager_idle_entry_, // entry_
    (SM_ActionHandler)0,                             // exit_
    (SM_StateHandler)&SM_InputComposerManager_idle_  // handler_
};

//============================================================================
//=== HSM implementations

static SM_StatePtr SM_InputComposerManager_TOP_initial(SM_Hsm * const me) SM_HSM_RETT {
    (void)me;
    return _SM_INIT(&SM_InputComposerManager_idle);
}

static void SM_InputComposerManager_idle_entry_(SM_Hsm * const me) SM_HSM_RETT {
    (void)me;
}

static SM_RetState SM_InputComposerManager_idle_(SM_Hsm * const me,
                                                 UI_Evt const * const e) SM_HSM_RETT
{
    SM_InputComposerManager *manager =
        containerof(me, SM_InputComposerManager, super);

    switch (e->sig) {
        case UI_KEY_ENTER_SIG: {
            return _SM_HANDLED();
        }

        case UI_INPUT_SIG:
        case UI_KEY_ESC_SIG:
        case UI_KEY_UP_SIG:
        case UI_KEY_DOWN_SIG:
        case UI_KEY_J_SIG:
        case UI_KEY_K_SIG:
        case UI_KEY_CTRL_N_SIG:
        case UI_KEY_CTRL_P_SIG: {
            UI_InputEvt const * const inputEvt =
                (UI_InputEvt const *)e;
            InputComposer_offerInput(manager->composer,
                                     &inputEvt->input);
            return _SM_HANDLED();
        }

        default: {
            return _SM_SUPER();
        }
    }
}

//============================================================================
//=== Constructor

void SM_InputComposerManager_ctor(
    SM_InputComposerManager * const me,
    struct InputComposer * const composer)
{
    DBC_REQUIRE(100, me != (SM_InputComposerManager *)0);
    DBC_REQUIRE(101, composer != (struct InputComposer *)0);

    me->composer = composer;
}

void SM_InputComposerManager_init(
    SM_InputComposerManager * const me)
{
    DBC_REQUIRE(200, me != (SM_InputComposerManager *)0);
    SM_Hsm_init_(
        &me->super,
        (SM_InitHandler)SM_InputComposerManager_TOP_initial);
}

void SM_InputComposerManager_dispatchEvt(
    SM_InputComposerManager * const me,
    UI_Evt const * const e)
{
    DBC_REQUIRE(300, me != (SM_InputComposerManager *)0);
    DBC_REQUIRE(301, e != (UI_Evt const *)0);

    SM_Hsm_dispatch_(&me->super, e);
}
