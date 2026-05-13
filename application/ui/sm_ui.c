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
//=== UI HSM — minimal: active state only, blinky interaction
#include <string.h>
#include "sm_port.h"
#include "sm_hsm.h"
#include "dbc_assert.h"
#include "ui.h"
#include "sm_ui_key.h"
#include "sm_ui.h"
DBC_MODULE_NAME("ui_hsm")

//============================================================================
//=== States

static SM_StatePtr SM_UI_TOP_initial(SM_Hsm *me) SM_HSM_RETT;

static void        SM_UI_active_entry_(SM_Hsm *me) SM_HSM_RETT;
static SM_RetState SM_UI_active_(SM_Hsm *me, UI_Evt const *e) SM_HSM_RETT;
SM_HsmState SM_HSM_ROM SM_UI_active = {
    (SM_StatePtr)0,                     // super (top)
    (SM_InitHandler)0,                  // init_ (leaf)
    (SM_ActionHandler)&SM_UI_active_entry_,// entry_
    (SM_ActionHandler)0,                // exit_
    (SM_StateHandler)&SM_UI_active_        // handler_
};

//============================================================================
//=== HSM implementations

static SM_StatePtr SM_UI_TOP_initial(SM_Hsm * const me) SM_HSM_RETT {
    SM_UI *ao = containerof(me, SM_UI, super);
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
                        " termbox ─ sm_tracer ", NULL);
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
                        " ● disconnected ", NULL);
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
                        " Alt+Q:quit ", NULL);
    }

    SM_UI_Key_ctor(&ao->cmdHsm);
    SM_UI_Key_init(&ao->cmdHsm);

    return _SM_INIT(&SM_UI_active);
}

static void SM_UI_active_entry_(SM_Hsm * const me) SM_HSM_RETT {
    (void)me;
}

static SM_RetState SM_UI_active_(SM_Hsm * const me, UI_Evt const * const e) {
    SM_UI *ao = containerof(me, SM_UI, super);

    switch (e->sig) {
    //------------------------------------------------------------------------
    //--- user input events
    case UI_KEY_ALT_Q_SIG: {
        ao->quit = true;
        return _SM_HANDLED();
    }

    case UI_TIMER_SIG: {
        return _SM_HANDLED();
    }

    case UI_BLINKY_TEXT_SIG:
    case UI_KEY_DEBUG_SIG: {
        UI_AppEvt const *ae = (UI_AppEvt const *)e;
        enum { UI_MAIN_MAX_LINES_ = 10000U };
        if (ao->mainLineCnt >= UI_MAIN_MAX_LINES_) {
            ncplane_scrollup(ao->mainPlane,
                             (int)(ao->mainLineCnt - UI_MAIN_MAX_LINES_ / 2U));
            ao->mainLineCnt = UI_MAIN_MAX_LINES_ / 2U;
        }

        ncplane_puttext(ao->mainPlane, -1, NCALIGN_LEFT,
                        ae->pld.msg.text, NULL);
        ++ao->mainLineCnt;
        return _SM_HANDLED();
    }

    default: {
        return _SM_SUPER();
    }
    }
}

//============================================================================
//=== Virtual functions

static void SM_UI_init(SM_UI * const me, void const * const e) {
    (void)e;
    SM_Hsm_init_(&me->super, (SM_InitHandler)SM_UI_TOP_initial);
}

static void SM_UI_dispatch(SM_UI * const me, void const * const e) {
    SM_Hsm_dispatch_(&me->super, e);
}

//============================================================================
//=== Constructor / Init / Render

void SM_UI_ctor(SM_UI * const me) {
    DBC_REQUIRE(100, me != (SM_UI *)0);

    me->init     = (VC_Handler)SM_UI_init;
    me->dispatch = (VC_Handler)SM_UI_dispatch;

    me->nc          = (struct notcurses *)0;
    me->statusPlane = (struct ncplane *)0;
    me->mainPlane   = (struct ncplane *)0;
    me->keybarPlane = (struct ncplane *)0;
    me->mainLineCnt   = 0U;
    me->quit        = false;
    me->dirty       = false;
    me->lastRender.tv_sec  = 0;
    me->lastRender.tv_nsec = 0;
}

void SM_UI_start(SM_UI * const me) {
    DBC_REQUIRE(200, me != (SM_UI *)0);
    DBC_REQUIRE(201, me->init != (VC_Handler)0);
    (*me->init)(me, (void const *)0);
}
