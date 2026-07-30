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
//=== AO_SpMngr subsystem root: SST task + HSM
#include "sst.h"
#include "sm_port.h"
#include "sm_hsm.h"
#include "dbc_assert.h"
#include "ui_evt.h"
#include <bits/sockaddr.h>
#include "sp_mngr.h"
DBC_MODULE_NAME("sp_mngr")

//============================================================================
//=== AO instance

typedef struct {
    SST_Task super;
    SM_Hsm hsm;
} SpMngr;

static SpMngr SpMngr_inst_;
SST_Task * const AO_SpMngr = &SpMngr_inst_.super;

//============================================================================
//=== HSM states

static SM_StatePtr SpMngr_TOP_initial_(SM_Hsm * const me) SM_HSM_RETT;
static SM_RetState SpMngr_active_(SM_Hsm * const me, SST_Evt const * const e) SM_HSM_RETT;

static SM_HsmState SM_HSM_ROM SpMngr_active = {
    SM_HSM_TOP,                         // super (top)
    (SM_InitHandler)0,                  // init_ (leaf)
    (SM_ActionHandler)0,                // entry_
    (SM_ActionHandler)0,                // exit_
    (SM_StateHandler)&SpMngr_active_    // handler_
};

//============================================================================
//=== HSM implementations

static SM_StatePtr SpMngr_TOP_initial_(SM_Hsm * const me) SM_HSM_RETT {
    (void)me;
    UI_postText("SpMngr initialized.\n");
    return _SM_INIT(&SpMngr_active);
}

static SM_RetState SpMngr_active_(SM_Hsm * const me, SST_Evt const * const e) SM_HSM_RETT {
    (void)me;

    switch (e->sig) {
        default: {
            return _SM_SUPER();
        }
    }
}

//============================================================================
//=== SST virtuals

static void SpMngr_init_(SpMngr * const me,
                         SST_Evt const * const e)
{
    DBC_REQUIRE(100, me != (SpMngr *)0);
    (void)e;

    SM_Hsm_init_(&me->hsm, (SM_InitHandler)SpMngr_TOP_initial_);
}

static void SpMngr_dispatch_(SpMngr * const me,
                             SST_Evt const * const e)
{
    DBC_REQUIRE(200, me != (SpMngr *)0);
    DBC_REQUIRE(201, e != (SST_Evt const *)0);

    SM_Hsm_dispatch_(&me->hsm, e);
}

//============================================================================
//=== Constructor

void SpMngr_ctor(void) {
    SpMngr * const me = &SpMngr_inst_;

    SST_Task_ctor(&me->super,
                  (SST_Handler)&SpMngr_init_,
                  (SST_Handler)&SpMngr_dispatch_);
}
