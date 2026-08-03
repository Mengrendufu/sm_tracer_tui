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
//=== SM_SpThread: serial-port thread state owner
#include <stddef.h>
#include "sst.h"
#include "sm_port.h"
#include "sm_hsm.h"
#include "dbc_assert.h"
#include "app_sig.h"
#include "sp_mngr/sp_mngr.h"
#include "sm_sp_thread.h"
#include "sp_thread/thread/serial_port_runtime_priv.h"
#include "ui_evt.h"
DBC_MODULE_NAME("sm_sp_thread")

//============================================================================
//=== HSM states

// TOP-INIT
static SM_StatePtr SM_SpThread_TOP_initial_(SM_Hsm * const me) SM_HSM_RETT;

// idle
static SM_RetState SM_SpThread_idle_(SM_Hsm * const me, SpThreadEvt const * const e) SM_HSM_RETT;

static SM_HsmState SM_HSM_ROM SM_SpThread_idle = {
    SM_HSM_TOP,                              // super
    (SM_InitHandler)0,                       // init_
    (SM_ActionHandler)0,                     // entry_
    (SM_ActionHandler)0,                     // exit_
    (SM_StateHandler)&SM_SpThread_idle_      // handler
};

//============================================================================
//=== HSM implementations

static SM_StatePtr SM_SpThread_TOP_initial_(SM_Hsm * const me) SM_HSM_RETT {
    (void)me;
    UI_postText("SerialPortThread initialized.\n");
    return _SM_INIT(&SM_SpThread_idle);
}

static SM_RetState SM_SpThread_idle_(SM_Hsm * const me, SpThreadEvt const * const e) SM_HSM_RETT {
    (void)me;

    switch (e->sig) {
        case SPTHRD_REFRESH_PORTS_SIG: {
            SpMngrPortsEvt * const result = SST_NEW(SpMngrPortsEvt);
            result->super.sig = SPMNGR_REFRESHED_PORTS_SIG;
            result->portNames = SerialPortRuntime_listPorts(
                &result->portNamesSize);
            SST_Task_post(AO_SpMngr, &result->super);
            return _SM_HANDLED();
        }

        default: {
            return _SM_SUPER();
        }
    }
}

//============================================================================
//=== Lifecycle

void SM_SpThread_ctor(SM_SpThread * const me) {
    DBC_REQUIRE(100, me != (SM_SpThread *)0);

    me->super.curr = (SM_StatePtr)0;
    me->super.next = (SM_StatePtr)0;
}

void SM_SpThread_init(SM_SpThread * const me) {
    DBC_REQUIRE(200, me != (SM_SpThread *)0);

    SM_Hsm_init_(&me->super,
                 (SM_InitHandler)SM_SpThread_TOP_initial_);
}

void SM_SpThread_dispatchEvt(SM_SpThread * const me,
                             SpThreadEvt const * const e)
{
    DBC_REQUIRE(300, me != (SM_SpThread *)0);
    DBC_REQUIRE(301, e != (SpThreadEvt const *)0);
    DBC_REQUIRE(302, e->sig > SPTHRD_NULL_SIG);

    SM_Hsm_dispatch_(&me->super, e);
}
