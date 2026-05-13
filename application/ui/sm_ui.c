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
//=== UI HSM -- minimal, blinky interaction
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

static SM_StatePtr SM_UI_active_init_(SM_Hsm *me) SM_HSM_RETT;
static void        SM_UI_active_entry_(SM_Hsm *me) SM_HSM_RETT;
static SM_RetState SM_UI_active_(SM_Hsm *me, UI_Evt const *e) SM_HSM_RETT;

static void        SM_UI_showMain_entry_(SM_Hsm *me) SM_HSM_RETT;
static SM_RetState SM_UI_showMain_(SM_Hsm *me, UI_Evt const *e) SM_HSM_RETT;

static void        SM_UI_drawMenuItem_(SM_UI *ao, uint32_t idx);
static void        SM_UI_showMenu_entry_(SM_Hsm *me) SM_HSM_RETT;
static void        SM_UI_showMenu_exit_(SM_Hsm *me) SM_HSM_RETT;
static SM_RetState SM_UI_showMenu_(SM_Hsm *me, UI_Evt const *e) SM_HSM_RETT;

SM_HsmState SM_HSM_ROM SM_UI_active = {
    (SM_StatePtr)0,                           // super (top)
    (SM_InitHandler)SM_UI_active_init_,       // init_ → showMain
    (SM_ActionHandler)&SM_UI_active_entry_,   // entry_
    (SM_ActionHandler)0,                      // exit_
    (SM_StateHandler)&SM_UI_active_           // handler_
};

SM_HsmState SM_HSM_ROM SM_UI_showMain = {
    &SM_UI_active,                            // super
    (SM_InitHandler)0,                        // init_ (leaf)
    (SM_ActionHandler)&SM_UI_showMain_entry_, // entry_
    (SM_ActionHandler)0,                      // exit_
    (SM_StateHandler)&SM_UI_showMain_         // handler_
};

//--- menu items data ---
#define MENU_NUM_ITEMS_ 4U
static char const * const SM_UI_menuItems_[MENU_NUM_ITEMS_] = {
    "Resume",
    "Clear screen",
    "About",
    "Quit"
};

SM_HsmState SM_HSM_ROM SM_UI_showMenu = {
    &SM_UI_active,                             // super
    (SM_InitHandler)0,                         // init_ (leaf)
    (SM_ActionHandler)&SM_UI_showMenu_entry_,  // entry_
    (SM_ActionHandler)&SM_UI_showMenu_exit_,   // exit_
    (SM_StateHandler)&SM_UI_showMenu_          // handler_
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
        ao->titlePlane = ncplane_create(std, &nopts);
        ncplane_set_bg_rgb8(ao->titlePlane, 60, 60, 120);
        ncplane_set_fg_rgb8(ao->titlePlane, 230, 230, 255);
        ncplane_on_styles(ao->titlePlane, NCSTYLE_BOLD);
        ncplane_puttext(ao->titlePlane, 0, NCALIGN_LEFT,
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
        ncplane_set_bg_rgb8(ao->mainPlane, 25, 25, 40);
        ncplane_set_fg_rgb8(ao->mainPlane, 200, 220, 200);
        {
            nccell base = NCCELL_TRIVIAL_INITIALIZER;
            nccell_set_bg_rgb8(&base, 25, 25, 40);
            nccell_set_fg_rgb8(&base, 200, 220, 200);
            nccell_load_char(ao->mainPlane, &base, ' ');
            ncplane_set_base_cell(ao->mainPlane, &base);
            nccell_release(ao->mainPlane, &base);
        }
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
        ncplane_move_yx(ao->keybarPlane, (int)dimY, 0); // hidden by default
    }

    SM_UI_Key_ctor(&ao->cmdHsm);
    SM_UI_Key_init(&ao->cmdHsm);

    return _SM_INIT(&SM_UI_active);
}

static SM_StatePtr SM_UI_active_init_(SM_Hsm * const me) SM_HSM_RETT {
    (void)me;
    return _SM_INIT(&SM_UI_showMain);
}

static void SM_UI_active_entry_(SM_Hsm * const me) SM_HSM_RETT {
    (void)me;
}

static SM_RetState SM_UI_active_(SM_Hsm * const me, UI_Evt const * const e) {
    SM_UI *ao = containerof(me, SM_UI, super);

    switch (e->sig) {
    //------------------------------------------------------------------------
    //--- user input events
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

        ncplane_set_bg_rgb8(ao->mainPlane, 25, 25, 40);
        ncplane_set_fg_rgb8(ao->mainPlane, 200, 220, 200);
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

static void SM_UI_showMain_entry_(SM_Hsm * const me) SM_HSM_RETT {
    (void)me;
}

static SM_RetState SM_UI_showMain_(SM_Hsm * const me, UI_Evt const * const e) {
    (void)me;

    switch (e->sig) {
    case UI_KEY_CTRL_SLASH_SIG: {
        return _SM_TRAN(&SM_UI_showMenu);
    }

    default: {
        return _SM_SUPER();
    }
    }
}

//============================================================================
//=== Menu drawing -- single item, partial redraw

static void SM_UI_drawMenuItem_(SM_UI * const ao, uint32_t const idx) {
    DBC_REQUIRE(301, idx < MENU_NUM_ITEMS_);
    DBC_REQUIRE(302, ao->menuPlane != (struct ncplane *)0);

    unsigned dimY, dimX;
    ncplane_dim_yx(ao->menuPlane, &dimY, &dimX);

    char line[32];
    (void)snprintf(line, sizeof(line), "| %-*s", (int)(dimX - 2),
                   SM_UI_menuItems_[idx]);

    if (idx == ao->menuSel) {
        ncplane_set_bg_rgb8(ao->menuPlane, 80, 80, 160);
        ncplane_set_fg_rgb8(ao->menuPlane, 255, 255, 255);
    } else {
        ncplane_set_bg_rgb8(ao->menuPlane, 50, 50, 100);
        ncplane_set_fg_rgb8(ao->menuPlane, 200, 200, 220);
    }
    ncplane_cursor_move_yx(ao->menuPlane, (int)(idx + 1U), 0);
    ncplane_putstr(ao->menuPlane, line);
    ao->dirty = true;
}

static void SM_UI_showMenu_entry_(SM_Hsm * const me) SM_HSM_RETT {
    SM_UI *ao = containerof(me, SM_UI, super);
    struct ncplane *std = notcurses_stdplane(ao->nc);

    unsigned dimY, dimX;
    ncplane_dim_yx(std, &dimY, &dimX);

    uint32_t menuW = 24U;
    uint32_t menuH = MENU_NUM_ITEMS_ + 2U;
    int menuY = (int)(dimY / 2U - menuH / 2U);
    int menuX = (int)(dimX / 2U - menuW / 2U);

    ncplane_options nopts = {
        .y = menuY, .x = menuX,
        .rows = menuH, .cols = menuW,
        .name = "menu"
    };
    ao->menuPlane = ncplane_create(std, &nopts);
    ncplane_set_bg_rgb8(ao->menuPlane, 50, 50, 100);
    ncplane_set_fg_rgb8(ao->menuPlane, 200, 200, 220);
    ncplane_erase(ao->menuPlane);

    // top border with title
    ncplane_cursor_move_yx(ao->menuPlane, 0, 0);
    ncplane_set_bg_rgb8(ao->menuPlane, 50, 50, 100);
    ncplane_set_fg_rgb8(ao->menuPlane, 140, 140, 200);
    ncplane_putstr(ao->menuPlane, "|     ----menu----     |");

    // bottom border
    ncplane_cursor_move_yx(ao->menuPlane, MENU_NUM_ITEMS_ + 1U, 0);
    ncplane_putstr(ao->menuPlane, "|----------------------|");

    ao->menuSel = 0U;
    for (uint32_t i = 0U; i < MENU_NUM_ITEMS_; ++i) {
        SM_UI_drawMenuItem_(ao, i);
    }
}

static void SM_UI_showMenu_exit_(SM_Hsm * const me) SM_HSM_RETT {
    SM_UI *ao = containerof(me, SM_UI, super);
    if (ao->menuPlane) {
        ncplane_destroy(ao->menuPlane);
        ao->menuPlane = (struct ncplane *)0;
    }
}

static SM_RetState SM_UI_showMenu_(SM_Hsm * const me, UI_Evt const * const e) {
    SM_UI *ao = containerof(me, SM_UI, super);

    switch (e->sig) {
    case UI_KEY_DOWN_SIG: {
        uint32_t oldSel = ao->menuSel;
        uint32_t maxIdx = MENU_NUM_ITEMS_ - 1U;
        ao->menuSel = (ao->menuSel >= maxIdx) ? 0U : (ao->menuSel + 1U);
        SM_UI_drawMenuItem_(ao, oldSel);
        SM_UI_drawMenuItem_(ao, ao->menuSel);
        return _SM_HANDLED();
    }

    case UI_KEY_UP_SIG: {
        uint32_t oldSel = ao->menuSel;
        uint32_t maxIdx = MENU_NUM_ITEMS_ - 1U;
        ao->menuSel = (ao->menuSel == 0U) ? maxIdx : (ao->menuSel - 1U);
        SM_UI_drawMenuItem_(ao, oldSel);
        SM_UI_drawMenuItem_(ao, ao->menuSel);
        return _SM_HANDLED();
    }

    case UI_KEY_ENTER_SIG: {
        switch (ao->menuSel) {
        case 0U:
            return _SM_TRAN(&SM_UI_showMain);

        case 1U: {
            ncplane_erase(ao->mainPlane);
            ao->mainLineCnt = 0U;
            return _SM_TRAN(&SM_UI_showMain);
        }

        case 2U: {
            ncplane_puttext(ao->mainPlane, -1, NCALIGN_LEFT,
                            "termbox v0.1 -- HSM demo\n"
                            "notcurses + SST + sm_hsm\n", NULL);
            ++ao->mainLineCnt;
            return _SM_TRAN(&SM_UI_showMain);
        }

        case 3U: {
            ao->quit = true;
            return _SM_TRAN(&SM_UI_showMain);
        }

        default:
            return _SM_TRAN(&SM_UI_showMain);
        }
    }

    case UI_KEY_ESC_SIG:
    case UI_KEY_CTRL_SLASH_SIG: {
        return _SM_TRAN(&SM_UI_showMain);
    }

    default:
        return _SM_SUPER();
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
    me->titlePlane  = (struct ncplane *)0;
    me->statusPlane = (struct ncplane *)0;
    me->mainPlane   = (struct ncplane *)0;
    me->keybarPlane = (struct ncplane *)0;
    me->menuPlane   = (struct ncplane *)0;
    me->mainLineCnt   = 0U;
    me->menuSel       = 0U;
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
