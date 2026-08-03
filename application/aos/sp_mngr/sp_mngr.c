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
#include <stdio.h>
#include <stdlib.h>
#include "sst.h"
#include "sm_port.h"
#include "sm_hsm.h"
#include "dbc_assert.h"
#include "app_sig.h"
#include "ui_evt.h"
#include "sp_thread/sp_thread.h"
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

static void SpMngr_reportConfig_(char const *action,
                                 SpMngrConfigEvt const *e);

//============================================================================
//=== HSM states

// TOP-INIT
static SM_StatePtr SpMngr_TOP_initial_(SM_Hsm * const me) SM_HSM_RETT;

// active
static SM_RetState SpMngr_active_(SM_Hsm * const me, SST_Evt const * const e) SM_HSM_RETT;

static SM_HsmState SM_HSM_ROM SpMngr_active = {
    SM_HSM_TOP,                         // super
    (SM_InitHandler)0,                  // init_
    (SM_ActionHandler)0,                // entry_
    (SM_ActionHandler)0,                // exit_
    (SM_StateHandler)&SpMngr_active_    // handler
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
        case SPMNGR_CONFIG_UPDATE_SIG: {
            SpMngr_reportConfig_(
                "configuration updated",
                SST_EVT_DOWNCAST(SpMngrConfigEvt, e));
            return _SM_HANDLED();
        }

        case SPMNGR_PORT_CONNECT_SIG: {
            SpMngr_reportConfig_(
                "connect requested",
                SST_EVT_DOWNCAST(SpMngrConfigEvt, e));
            return _SM_HANDLED();
        }

        case SPMNGR_PORT_DISCONNECT_SIG: {
            UI_postText("SpMngr: disconnect requested.\n");
            return _SM_HANDLED();
        }

        case SPMNGR_REFRESH_PORTS_SIG: {
            SpThread_postRefreshPorts();
            return _SM_HANDLED();
        }

        case SPMNGR_REFRESHED_PORTS_SIG: {
            SpMngrPortsEvt const * const result =
                SST_EVT_DOWNCAST(SpMngrPortsEvt, e);
            if (result->portNames != (char *)0) {
                UI_postPortList(result->portNames,
                                result->portNamesSize);
                free(result->portNames);
            } else {
                UI_postText("Serial port refresh failed.\n");
            }
            return _SM_HANDLED();
        }

        default: {
            return _SM_SUPER();
        }
    }
}

static void SpMngr_reportConfig_(
    char const * const action,
    SpMngrConfigEvt const * const e)
{
    DBC_REQUIRE(300, action != (char const *)0);
    DBC_REQUIRE(301, e != (SpMngrConfigEvt const *)0);

    char text[2048];
    int const len = snprintf(
        text, sizeof(text),
        "SpMngr: %s: port=%s baud=%s data=%s stop=%s "
        "parity=%s flow=%s proto=%s\n",
        action, e->config.port, e->config.baudrate,
        e->config.dataBits, e->config.stopBits,
        e->config.parity, e->config.flowControl,
        e->config.protocol);
    DBC_ASSERT(302, (len > 0) && ((size_t)len < sizeof(text)));
    (void)len;
    UI_postText(text);
}

//============================================================================
//=== SST virtuals

static void SpMngr_init_(SpMngr * const me,
                         SST_Evt const * const e)
{
    static SST_Evt const initialRefreshEvt = {
        .sig = SPMNGR_REFRESH_PORTS_SIG,
    };

    DBC_REQUIRE(100, me != (SpMngr *)0);
    (void)e;

    SM_Hsm_init_(&me->hsm, (SM_InitHandler)SpMngr_TOP_initial_);

    UI_postText("SpMngr: requesting initial serial port refresh.\n");

    // Queue the AO's initial business intent only after its HSM has reached
    // a stable leaf state; the worker may dispatch as soon as this is posted.
    SST_Task_post(&me->super, &initialRefreshEvt);
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
