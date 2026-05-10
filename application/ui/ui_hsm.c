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

static void        UI_activeEntry_(SM_Hsm *me) SM_HSM_RETT;
static SM_RetState UI_activeHandler_(SM_Hsm *me, void const *e) SM_HSM_RETT;

SM_HsmState SM_HSM_ROM UI_active_ = {
    (SM_StatePtr)0,                     // super (top)
    (SM_InitHandler)0,                  // init_ (leaf)
    (SM_ActionHandler)&UI_activeEntry_, // entry_
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
//=== active entry — create nested-box TUI layout

static void UI_activeEntry_(SM_Hsm * const me) SM_HSM_RETT {
    UI_AO *ao = containerof(me, UI_AO, super);
    struct ncplane *std = notcurses_stdplane(ao->nc);

    unsigned dimY, dimX;
    ncplane_dim_yx(std, &dimY, &dimX);

    // colour scheme
    uint64_t borderCh = NCCHANNELS_INITIALIZER(60, 60, 120, 15, 15, 35);

    // outer box (full screen)
    ncplane_ascii_box(std, 0, borderCh, dimY, dimX, 0);

    // title row (row 1, inside outer box)
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

    // status row (row 3)
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

    // main box (rows 5 ~ dimY-4)
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

    // keybar row (row dimY-2)
    {
        ncplane_options nopts = {
            .y = (int)(dimY - 2), .x = 2, .rows = 1, .cols = dimX - 4,
            .name = "keybar"
        };
        ao->keybarPlane = ncplane_create(std, &nopts);
        ncplane_set_bg_rgb8(ao->keybarPlane, 50, 50, 80);
        ncplane_set_fg_rgb8(ao->keybarPlane, 160, 160, 180);
        ncplane_puttext(ao->keybarPlane, 0, NCALIGN_CENTER,
                        " ^Q:quit | F1:connect | F2:config | F5:clear ",
                        (size_t)0);
    }
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
    case UI_BLINKY_TEXT_SIG: {
        UI_TextEvt const *te = (UI_TextEvt const *)e;
        ncplane_puttext(ao->mainPlane, -1, NCALIGN_LEFT,
                        te->text, (size_t)0);
        return _SM_HANDLED();
    }
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

    me->nc          = (struct notcurses *)0;
    me->statusPlane = (struct ncplane *)0;
    me->mainPlane   = (struct ncplane *)0;
    me->keybarPlane = (struct ncplane *)0;
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
