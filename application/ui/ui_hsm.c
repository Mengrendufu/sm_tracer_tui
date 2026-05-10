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
//=== UI HSM — state definitions, handlers, virtual functions
#include "sm_port.h"
#include "sm_hsm.h"
#include "dbc_assert.h"
#include "ui.h"
#include "ui_hsm.h"
DBC_MODULE_NAME("ui_hsm")

//============================================================================
//=== State: active

static SM_RetState UI_activeHandler_(SM_Hsm *me, void const *e) SM_HSM_RETT;

SM_HsmState SM_HSM_ROM UI_active_ = {
    (SM_StatePtr)0,                     // super (top)
    (SM_InitHandler)0,                  // init_ (leaf)
    (SM_ActionHandler)0,                // entry_
    (SM_ActionHandler)0,                // exit_
    (SM_StateHandler)&UI_activeHandler_ // handler_
};

//============================================================================
//=== Top-initial

static SM_StatePtr UI_topInitial_(SM_Hsm *me) SM_HSM_RETT;

static SM_StatePtr UI_topInitial_(SM_Hsm * const me) SM_HSM_RETT {
    (void)me;
    return _SM_INIT(&UI_active_);
}

//============================================================================
//=== active handler

static SM_RetState UI_activeHandler_(SM_Hsm * const me, void const * const e) {
    UI_AO     *ao  = containerof(me, UI_AO, super);
    UI_Signal  sig = *(UI_Signal const *)e;

    switch (sig) {
    case UI_KEY_SIG: {
        UI_KeyEvt const *ke = (UI_KeyEvt const *)e;
        (void)ke;
        // TODO: handle key
        return _SM_HANDLED();
    }
    case UI_QUIT_SIG:
        ao->quit = true;
        return _SM_HANDLED();
    case UI_TIMER_SIG:
        // periodic tick — render gate advances here
        return _SM_HANDLED();
    default:
        return _SM_SUPER();
    }
}

//============================================================================
//=== Virtual functions

static void UI_AO_init_(void * const me, void const * const e) SM_HSM_RETT {
    (void)e;
    SM_Hsm_init_(&((UI_AO *)me)->super, (SM_InitHandler)UI_topInitial_);
}

static void UI_AO_dispatch_(void * const me, void const * const e) SM_HSM_RETT {
    SM_Hsm_dispatch_(&((UI_AO *)me)->super, e);
}

//============================================================================
//=== Constructor

void UI_AO_ctor(UI_AO * const me) {
    DBC_REQUIRE(100, me != (UI_AO *)0);

    me->init     = (VC_Handler)UI_AO_init_;
    me->dispatch = (VC_Handler)UI_AO_dispatch_;

    me->nc    = (struct notcurses *)0;
    me->quit  = false;
    me->dirty = false;
    me->lastRender.tv_sec  = 0;
    me->lastRender.tv_nsec = 0;
}

//============================================================================
//=== Init

void UI_AO_init(UI_AO * const me) {
    DBC_REQUIRE(200, me != (UI_AO *)0);
    DBC_REQUIRE(201, me->init != (VC_Handler)0);
    (*me->init)(me, (void const *)0);
}
