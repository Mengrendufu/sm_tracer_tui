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
//=== Blinky AO — SST task + HSM, off/on states
#include "app_sig.h"
#include "sst.h"
#include "sm_port.h"
#include "sm_hsm.h"
#include "bsp.h"
#include "dbc_assert.h"
#include "ui_evt.h"
#include "blinky.h"

//============================================================================
// DBC_MODULE_NAME("blinky")

//============================================================================
//=== Blinky AO

typedef struct {
    SST_Task super;
    SM_Hsm hsm;
    SST_TimeEvt timer;
} Blinky;

Blinky Blinky_inst;
SST_Task * const AO_Blinky = &Blinky_inst.super;

//============================================================================
//=== HSM states

// TOP-INIT
static SM_StatePtr Blinky_TOP_initial(SM_Hsm * const me) SM_HSM_RETT;

// off
static SM_RetState Blinky_off_(SM_Hsm * const me, SST_Evt const * const e) SM_HSM_RETT;
SM_HsmState SM_HSM_ROM Blinky_off = {
    SM_HSM_TOP,                           // super
    (SM_InitHandler)0,                    // init_
    (SM_ActionHandler)0,                  // entry_
    (SM_ActionHandler)0,                  // exit_
    (SM_StateHandler)&Blinky_off_         // handler
};

// on
static SM_RetState Blinky_on_(SM_Hsm * const me, SST_Evt const * const e) SM_HSM_RETT;
SM_HsmState SM_HSM_ROM Blinky_on = {
    SM_HSM_TOP,                          // super
    (SM_InitHandler)0,                   // init_
    (SM_ActionHandler)0,                 // entry_
    (SM_ActionHandler)0,                 // exit_
    (SM_StateHandler)&Blinky_on_         // handler
};

//============================================================================
//=== HSM implementations

// TOP-INIT
static SM_StatePtr Blinky_TOP_initial(SM_Hsm * const me) SM_HSM_RETT {
    Blinky *b = containerof(me, Blinky, hsm);
    SST_TimeEvt_arm(&b->timer, BSP_TICKS_PER_SEC, BSP_TICKS_PER_SEC);
    UI_postText("BlinkyTOPInit.\n");
    return _SM_INIT(&Blinky_off);
}

// off
static SM_RetState Blinky_off_(
    SM_Hsm * const me, SST_Evt const * const e) SM_HSM_RETT
{
    (void)me;
    switch (e->sig) {
        case BLINKY_TIMEOUT_SIG: {
            return _SM_TRAN(&Blinky_on);
        }
        default: {
            return _SM_SUPER();
        }
    }
}

// on
static SM_RetState Blinky_on_(
    SM_Hsm * const me, SST_Evt const * const e) SM_HSM_RETT
{
    (void)me;
    switch (e->sig) {
        case BLINKY_TIMEOUT_SIG: {
            return _SM_TRAN(&Blinky_off);
        }
        default: {
            return _SM_SUPER();
        }
    }
}

//============================================================================
//=== SST virtuals

static void Blinky_init(Blinky * const me, SST_Evt const * const e) {
    (void)e;
    SM_Hsm_init_(&me->hsm, (SM_InitHandler)Blinky_TOP_initial);
}

static void Blinky_dispatch(Blinky * const me, SST_Evt const * const e) {
    SM_Hsm_dispatch_(&me->hsm, e);
}

//============================================================================
//=== Constructor / Start

void Blinky_ctor(void) {
    Blinky * const me = &Blinky_inst;
    SST_Task_ctor(&me->super, (SST_Handler)&Blinky_init,
                  (SST_Handler)&Blinky_dispatch);
    SST_TimeEvt_ctor(&me->timer, BLINKY_TIMEOUT_SIG, &me->super);
}
