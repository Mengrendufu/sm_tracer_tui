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
//=== Blinky AO — SST task + HSM, idle/active states
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

static Blinky l_blinky;
static SST_Evt const *l_blinkyQ_[BLINKY_Q_LEN_];

//============================================================================
//=== HSM states

static SM_StatePtr Blinky_topInitial_(SM_Hsm *me) SM_HSM_RETT;

static void        Blinky_idleEntry_(SM_Hsm *me) SM_HSM_RETT;
static SM_RetState Blinky_idleHandler_(SM_Hsm *me, void const *e) SM_HSM_RETT;
SM_HsmState SM_HSM_ROM Blinky_idle_ = {
    (SM_StatePtr)0,                       // super (top)
    (SM_InitHandler)0,                    // init_ (leaf)
    (SM_ActionHandler)&Blinky_idleEntry_, // entry_
    (SM_ActionHandler)0,                  // exit_
    (SM_StateHandler)&Blinky_idleHandler_ // handler_
};

static void        Blinky_activeEntry_(SM_Hsm *me) SM_HSM_RETT;
static SM_RetState Blinky_activeHandler_(SM_Hsm *me, void const *e) SM_HSM_RETT;
SM_HsmState SM_HSM_ROM Blinky_active_ = {
    (SM_StatePtr)0,                        // super (top)
    (SM_InitHandler)0,                     // init_ (leaf)
    (SM_ActionHandler)&Blinky_activeEntry_, // entry_
    (SM_ActionHandler)0,                   // exit_
    (SM_StateHandler)&Blinky_activeHandler_ // handler_
};

//============================================================================
//=== HSM implementations

static SM_StatePtr Blinky_topInitial_(SM_Hsm * const me) SM_HSM_RETT {
    (void)me;
    return _SM_INIT(&Blinky_idle_);
}

// idle — arm timer, wait for timeout
static void Blinky_idleEntry_(SM_Hsm * const me) SM_HSM_RETT {
    Blinky *b = containerof(me, Blinky, hsm);
    SST_TimeEvt_arm(&b->timer, BSP_TICKS_PER_SEC, 0U);
}

static SM_RetState Blinky_idleHandler_(SM_Hsm * const me, void const * const e) {
    (void)me;

    switch (((SST_Evt const *)e)->sig) {
    case SST_TIMEOUT_SIG:
        return _SM_TRAN(&Blinky_active_);
    default:
        return _SM_SUPER();
    }
}

// active — send text to UI, arm timer, wait for timeout
static void Blinky_activeEntry_(SM_Hsm * const me) SM_HSM_RETT {
    Blinky *b = containerof(me, Blinky, hsm);
    (void)b;

    UI_postText(UI_BLINKY_TEXT_SIG, "blink on!", 9);
    SST_TimeEvt_arm(&b->timer, BSP_TICKS_PER_SEC, 0U);
}

static SM_RetState Blinky_activeHandler_(SM_Hsm * const me, void const * const e) {
    (void)me;

    switch (((SST_Evt const *)e)->sig) {
    case SST_TIMEOUT_SIG:
        return _SM_TRAN(&Blinky_idle_);
    default:
        return _SM_SUPER();
    }
}

//============================================================================
//=== SST virtuals

static void Blinky_init(SST_Task * const me, SST_Evt const * const e) {
    (void)e;
    Blinky *b = (Blinky *)me;
    SM_Hsm_init_(&b->hsm, (SM_InitHandler)Blinky_topInitial_);
}

static void Blinky_dispatch(SST_Task * const me, SST_Evt const * const e) {
    Blinky *b = (Blinky *)me;
    SM_Hsm_dispatch_(&b->hsm, e);
}

//============================================================================
//=== Constructor

void Blinky_ctor(void) {
    Blinky *me = &l_blinky;

    SST_Task_ctor(&me->super, &Blinky_init, &Blinky_dispatch);
    SST_TimeEvt_ctor(&me->timer, SST_TIMEOUT_SIG, &me->super);
}

SST_Task *Blinky_task(void) {
    return &l_blinky.super;
}

SST_Evt const **Blinky_qBuf(void) {
    return l_blinkyQ_;
}
