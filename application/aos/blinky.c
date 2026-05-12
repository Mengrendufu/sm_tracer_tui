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
#include "sst.h"
#include "sm_port.h"
#include "sm_hsm.h"
#include "bsp.h"
#include "dbc_assert.h"
#include "ui.h"
#include "blinky.h"
DBC_MODULE_NAME("blinky")

//============================================================================
//=== Blinky AO

typedef struct {
    SST_Task super;
    SM_Hsm hsm;
    SST_TimeEvt timer;
} Blinky;

Blinky l_blinky;
SST_Evt const *Blinky_qBuf_[BLINKY_Q_LEN_];

//============================================================================
//=== HSM states

static SM_StatePtr Blinky_TOP_initial(SM_Hsm *me) SM_HSM_RETT;

static void        Blinky_off_entry_(SM_Hsm *me) SM_HSM_RETT;
static SM_RetState Blinky_off_(SM_Hsm *me, void const *e) SM_HSM_RETT;
SM_HsmState SM_HSM_ROM Blinky_off = {
    (SM_StatePtr)0,                      // super (top)
    (SM_InitHandler)0,                   // init_ (leaf)
    (SM_ActionHandler)&Blinky_off_entry_,// entry_
    (SM_ActionHandler)0,                 // exit_
    (SM_StateHandler)&Blinky_off_        // handler_
};

static void        Blinky_on_entry_(SM_Hsm *me) SM_HSM_RETT;
static SM_RetState Blinky_on_(SM_Hsm *me, void const *e) SM_HSM_RETT;
SM_HsmState SM_HSM_ROM Blinky_on = {
    (SM_StatePtr)0,                     // super (top)
    (SM_InitHandler)0,                  // init_ (leaf)
    (SM_ActionHandler)&Blinky_on_entry_,// entry_
    (SM_ActionHandler)0,                // exit_
    (SM_StateHandler)&Blinky_on_        // handler_
};

//============================================================================
//=== HSM implementations

static SM_StatePtr Blinky_TOP_initial(SM_Hsm * const me) SM_HSM_RETT {
    (void)me;
    return _SM_INIT(&Blinky_off);
}

static void Blinky_off_entry_(SM_Hsm * const me) SM_HSM_RETT {
    Blinky *b = containerof(me, Blinky, hsm);
    SST_TimeEvt_arm(&b->timer, BSP_TICKS_PER_SEC, 0U);
}

static SM_RetState Blinky_off_(SM_Hsm * const me, void const * const e) {
    (void)me;
    if (((SST_Evt const *)e)->sig == SST_TIMEOUT_SIG) {
        return _SM_TRAN(&Blinky_on);
    }
    return _SM_SUPER();
}

static void Blinky_on_entry_(SM_Hsm * const me) SM_HSM_RETT {
    Blinky *b = containerof(me, Blinky, hsm);
    UI_postText(UI_BLINKY_TEXT_SIG, "blink on!", 9);
    SST_TimeEvt_arm(&b->timer, BSP_TICKS_PER_SEC, 0U);
}

static SM_RetState Blinky_on_(SM_Hsm * const me, void const * const e) {
    (void)me;
    if (((SST_Evt const *)e)->sig == SST_TIMEOUT_SIG) {
        return _SM_TRAN(&Blinky_off);
    }
    return _SM_SUPER();
}

//============================================================================
//=== SST virtuals

static void Blinky_init(SST_Task * const me, SST_Evt const * const e) {
    (void)e;
    Blinky *b = (Blinky *)me;
    SM_Hsm_init_(&b->hsm, (SM_InitHandler)Blinky_TOP_initial);
}

static void Blinky_dispatch(SST_Task * const me, SST_Evt const * const e) {
    Blinky *b = (Blinky *)me;
    SM_Hsm_dispatch_(&b->hsm, e);
}

//============================================================================
//=== Constructor / Start

void Blinky_ctor(void) {
    Blinky *me = &l_blinky;
    SST_Task_ctor(&me->super, &Blinky_init, &Blinky_dispatch);
    SST_TimeEvt_ctor(&me->timer, SST_TIMEOUT_SIG, &me->super);
}

SST_Task *Blinky_getTask(void) {
    return &l_blinky.super;
}
