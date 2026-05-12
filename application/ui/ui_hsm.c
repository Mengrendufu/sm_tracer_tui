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
#include <string.h>
#include "sm_port.h"
#include "sm_hsm.h"
#include "dbc_assert.h"
#include "ui.h"
#include "ui_cmd_hsm.h"
#include "ui_hsm.h"
DBC_MODULE_NAME("ui_hsm")

//============================================================================
//=== States — forward declarations

// top-initial
static SM_StatePtr UI_topInitial_(SM_Hsm *me) SM_HSM_RETT;

// active (parent — holds shared logic)
static SM_StatePtr UI_activeInit_(SM_Hsm *me) SM_HSM_RETT;
static SM_RetState UI_activeHandler_(SM_Hsm *me, void const *e) SM_HSM_RETT;
SM_HsmState SM_HSM_ROM UI_active_ = {
    (SM_StatePtr)0,                      // super (top)
    (SM_InitHandler)&UI_activeInit_,     // init_ → normal
    (SM_ActionHandler)0,                 // entry_
    (SM_ActionHandler)0,                 // exit_
    (SM_StateHandler)&UI_activeHandler_  // handler_
};

// normal (sub-state of active)
static SM_StatePtr UI_normalInit_(SM_Hsm *me) SM_HSM_RETT;
static void        UI_normalEntry_(SM_Hsm *me) SM_HSM_RETT;
static SM_RetState UI_normalHandler_(SM_Hsm *me, void const *e) SM_HSM_RETT;
SM_HsmState SM_HSM_ROM UI_normal_ = {
    (SM_StatePtr)&UI_active_,            // super
    (SM_InitHandler)&UI_normalInit_,     // init_ → idle
    (SM_ActionHandler)&UI_normalEntry_,  // entry_
    (SM_ActionHandler)0,                 // exit_
    (SM_StateHandler)&UI_normalHandler_  // handler_
};

// normal::idle
static SM_RetState UI_normalIdleHandler_(SM_Hsm *me, void const *e) SM_HSM_RETT;
SM_HsmState SM_HSM_ROM UI_normal_idle_ = {
    (SM_StatePtr)&UI_normal_,                  // super
    (SM_InitHandler)0,                         // leaf
    (SM_ActionHandler)0,                       // entry_
    (SM_ActionHandler)0,                       // exit_
    (SM_StateHandler)&UI_normalIdleHandler_    // handler_
};

// normal::command
static void        UI_normalCmdEntry_(SM_Hsm *me) SM_HSM_RETT;
static SM_RetState UI_normalCmdHandler_(SM_Hsm *me, void const *e) SM_HSM_RETT;
SM_HsmState SM_HSM_ROM UI_normal_command_ = {
    (SM_StatePtr)&UI_normal_,                // super
    (SM_InitHandler)0,                       // leaf
    (SM_ActionHandler)&UI_normalCmdEntry_,   // entry_
    (SM_ActionHandler)0,                     // exit_
    (SM_StateHandler)&UI_normalCmdHandler_   // handler_
};

// menu (sub-state of active)
static void        UI_menuEntry_(SM_Hsm *me) SM_HSM_RETT;
static void        UI_menuExit_(SM_Hsm *me) SM_HSM_RETT;
static SM_RetState UI_menuHandler_(SM_Hsm *me, void const *e) SM_HSM_RETT;
SM_HsmState SM_HSM_ROM UI_menu_ = {
    (SM_StatePtr)&UI_active_,          // super
    (SM_InitHandler)0,                 // leaf
    (SM_ActionHandler)&UI_menuEntry_,  // entry_
    (SM_ActionHandler)&UI_menuExit_,   // exit_
    (SM_StateHandler)&UI_menuHandler_  // handler_
};

// connect (sub-state of active)
static void        UI_connectEntry_(SM_Hsm *me) SM_HSM_RETT;
static void        UI_connectExit_(SM_Hsm *me) SM_HSM_RETT;
static SM_RetState UI_connectHandler_(SM_Hsm *me, void const *e) SM_HSM_RETT;
SM_HsmState SM_HSM_ROM UI_connect_ = {
    (SM_StatePtr)&UI_active_,            // super
    (SM_InitHandler)0,                   // leaf
    (SM_ActionHandler)&UI_connectEntry_, // entry_
    (SM_ActionHandler)&UI_connectExit_,  // exit_
    (SM_StateHandler)&UI_connectHandler_ // handler_
};

//============================================================================
//=== HSM implementations — top / active

static SM_StatePtr UI_topInitial_(SM_Hsm * const me) SM_HSM_RETT {
    (void)me;
    return _SM_INIT(&UI_active_);
}

static SM_StatePtr UI_activeInit_(SM_Hsm * const me) SM_HSM_RETT {
    (void)me;
    return _SM_INIT(&UI_normal_);
}

static SM_RetState UI_activeHandler_(SM_Hsm * const me, void const * const e) {
    UI_AO    *ao  = containerof(me, UI_AO, super);
    UI_Signal sig = *(UI_Signal const *)e;

    switch (sig) {
    case UI_TIMER_SIG:
        return _SM_HANDLED();

    case UI_QUIT_SIG:
    case UI_CMD_QUIT_SIG:
        ao->quit = true;
        return _SM_HANDLED();

    case UI_CMD_ACTIVE_SIG:
        ao->cmdHsm.active = true;
        return _SM_HANDLED();

    case UI_CMD_IDLE_SIG:
        ao->cmdHsm.active = false;
        return _SM_HANDLED();

    case UI_CMD_MENU_SIG:
        return _SM_TRAN(&UI_menu_);

    case UI_CMD_CONNECT_SIG:
        return _SM_TRAN(&UI_connect_);

    default:
        return _SM_SUPER();
    }
}

//============================================================================
//=== HSM implementations — normal

static SM_StatePtr UI_normalInit_(SM_Hsm * const me) SM_HSM_RETT {
    (void)me;
    return _SM_INIT(&UI_normal_idle_);
}

static void UI_normalEntry_(SM_Hsm * const me) SM_HSM_RETT {
    UI_AO *ao = containerof(me, UI_AO, super);
    struct ncplane *std = notcurses_stdplane(ao->nc);

    unsigned dimY, dimX;
    ncplane_dim_yx(std, &dimY, &dimX);

    uint64_t borderCh = NCCHANNELS_INITIALIZER(60, 60, 120, 15, 15, 35);

    // outer box
    ncplane_ascii_box(std, 0, borderCh, dimY, dimX, 0);

    // title
    {
        ncplane_options nopts = {
            .y = 1, .x = 2, .rows = 1, .cols = dimX - 4, .name = "title"
        };
        struct ncplane *title = ncplane_create(std, &nopts);
        ncplane_set_bg_rgb8(title, 60, 60, 120);
        ncplane_set_fg_rgb8(title, 230, 230, 255);
        ncplane_on_styles(title, NCSTYLE_BOLD);
        ncplane_puttext(title, 0, NCALIGN_CENTER,
                        " termbox ─ sm_tracer ", (size_t)0);
    }

    // status
    {
        ncplane_options nopts = {
            .y = 3, .x = 2, .rows = 1, .cols = dimX - 4, .name = "status"
        };
        ao->statusPlane = ncplane_create(std, &nopts);
        ncplane_set_bg_rgb8(ao->statusPlane, 35, 35, 60);
        ncplane_set_fg_rgb8(ao->statusPlane, 200, 200, 200);
        ncplane_puttext(ao->statusPlane, 0, NCALIGN_LEFT,
                        " ● disconnected ", (size_t)0);
    }

    // main scroll
    {
        unsigned mainRows = dimY - 7;
        ncplane_options nopts = {
            .y = 5, .x = 2, .rows = mainRows, .cols = dimX - 4, .name = "main"
        };
        ao->mainPlane = ncplane_create(std, &nopts);
        ncplane_set_bg_rgb8(ao->mainPlane, 15, 15, 35);
        ncplane_set_fg_rgb8(ao->mainPlane, 180, 220, 180);
        ncplane_set_scrolling(ao->mainPlane, true);
        ncplane_ascii_box(ao->mainPlane, 0, borderCh, mainRows, dimX - 4, 0);
    }

    // keybar
    {
        ncplane_options nopts = {
            .y = (int)(dimY - 2), .x = 2, .rows = 1, .cols = dimX - 4,
            .name = "keybar"
        };
        ao->keybarPlane = ncplane_create(std, &nopts);
        ncplane_set_bg_rgb8(ao->keybarPlane, 50, 50, 80);
        ncplane_set_fg_rgb8(ao->keybarPlane, 160, 160, 180);
        ncplane_puttext(ao->keybarPlane, 0, NCALIGN_CENTER,
                        " ESC:quit | :cmd | /menu | /connect ", (size_t)0);
    }
}

static SM_RetState UI_normalHandler_(SM_Hsm * const me, void const * const e) {
    UI_AO    *ao  = containerof(me, UI_AO, super);
    UI_Signal sig = *(UI_Signal const *)e;

    switch (sig) {
    case UI_BLINKY_TEXT_SIG: {
        UI_TextEvt const *te = (UI_TextEvt const *)e;

        enum { UI_MAIN_MAX_LINES_ = 10000U };
        if (ao->mainLines >= UI_MAIN_MAX_LINES_) {
            ncplane_scrollup(ao->mainPlane,
                             (int)(ao->mainLines - UI_MAIN_MAX_LINES_ / 2U));
            ao->mainLines = UI_MAIN_MAX_LINES_ / 2U;
        }

        ncplane_puttext(ao->mainPlane, -1, NCALIGN_LEFT,
                        te->text, (size_t)0);
        ++ao->mainLines;
        return _SM_HANDLED();
    }
    default:
        return _SM_SUPER();
    }
}

//============================================================================
//=== HSM implementations — normal::idle

static SM_RetState UI_normalIdleHandler_(SM_Hsm * const me,
                                          void const * const e) {
    UI_AO    *ao  = containerof(me, UI_AO, super);
    UI_Signal sig = *(UI_Signal const *)e;

    if (sig != UI_KEY_SIG) {
        return _SM_SUPER();
    }

    UI_KeyEvt const *ke = (UI_KeyEvt const *)e;

    switch (ke->key) {
    case (uint32_t)':':
        // activate command HSM, then enter command mode
        SM_Hsm_dispatch_(&ao->cmdHsm.super, e);
        return _SM_TRAN(&UI_normal_command_);

    case NCKEY_ESC:
    case (uint32_t)'q':
    case (uint32_t)'Q':
        UI_postSignal(UI_QUIT_SIG);
        return _SM_HANDLED();

    default:
        return _SM_HANDLED();
    }
}

//============================================================================
//=== HSM implementations — normal::command

static void UI_normalCmdEntry_(SM_Hsm * const me) SM_HSM_RETT {
    (void)me;
    // cmdHsm already activated by idle handler
}

static SM_RetState UI_normalCmdHandler_(SM_Hsm * const me,
                                         void const * const e) {
    UI_AO    *ao  = containerof(me, UI_AO, super);
    UI_Signal sig = *(UI_Signal const *)e;

    if (sig != UI_KEY_SIG) {
        return _SM_SUPER();
    }

    // delegate to command HSM
    SM_Hsm_dispatch_(&ao->cmdHsm.super, e);

    // check if cmdHsm returned to idle → exit command mode
    if (ao->cmdHsm.super.curr == &Cmd_idle_) {
        return _SM_TRAN(&UI_normal_idle_);
    }

    return _SM_HANDLED();
}

//============================================================================
//=== HSM implementations — menu (placeholder)

static void UI_menuEntry_(SM_Hsm * const me) SM_HSM_RETT {
    (void)me;
    // TODO: create menu overlay
}

static void UI_menuExit_(SM_Hsm * const me) SM_HSM_RETT {
    (void)me;
    // TODO: destroy menu overlay
}

static SM_RetState UI_menuHandler_(SM_Hsm * const me, void const * const e) {
    UI_Signal sig = *(UI_Signal const *)e;

    if (sig == UI_KEY_SIG) {
        UI_KeyEvt const *ke = (UI_KeyEvt const *)e;
        if (ke->key == NCKEY_ESC) {
            return _SM_TRAN(&UI_normal_);
        }
        return _SM_HANDLED();
    }
    return _SM_SUPER();
}

//============================================================================
//=== HSM implementations — connect (placeholder)

static void UI_connectEntry_(SM_Hsm * const me) SM_HSM_RETT {
    (void)me;
    // TODO: create connect dialog
}

static void UI_connectExit_(SM_Hsm * const me) SM_HSM_RETT {
    (void)me;
    // TODO: destroy connect dialog
}

static SM_RetState UI_connectHandler_(SM_Hsm * const me, void const * const e) {
    UI_Signal sig = *(UI_Signal const *)e;

    if (sig == UI_KEY_SIG) {
        UI_KeyEvt const *ke = (UI_KeyEvt const *)e;
        if (ke->key == NCKEY_ESC) {
            return _SM_TRAN(&UI_normal_);
        }
        return _SM_HANDLED();
    }
    return _SM_SUPER();
}

//============================================================================
//=== Virtual functions

static void UI_AO_init_(void * const me, void const * const e) SM_HSM_RETT {
    (void)e;
    UI_AO *ao = (UI_AO *)me;
    UI_CmdHsm_ctor(&ao->cmdHsm);
    UI_CmdHsm_init(&ao->cmdHsm);
    SM_Hsm_init_(&ao->super, (SM_InitHandler)UI_topInitial_);
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

    me->nc          = (struct notcurses *)0;
    me->statusPlane = (struct ncplane *)0;
    me->mainPlane   = (struct ncplane *)0;
    me->keybarPlane = (struct ncplane *)0;
    me->mainLines   = 0U;
    me->quit        = false;
    me->dirty       = false;
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
