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
#include <stdio.h>
#include "sm_port.h"
#include "sm_hsm.h"
#include "dbc_assert.h"
#include "ui.h"
#include "sm_ui_key.h"
#include "sm_ui.h"
DBC_MODULE_NAME("ui_hsm")

//============================================================================
//--- IO helpers ---

typedef enum {
    MENU_ACT_RESUME,
    MENU_ACT_CLEAR,
    MENU_ACT_ABOUT,
    MENU_ACT_QUIT
} MenuAction;

static void        SM_UI_drawMenuItem_(struct ncplane *mp, uint32_t idx,
                                       uint32_t sel, uint32_t width);
static void        SM_UI_menuCreate_(SM_UI *ao, struct ncplane *parent);
static void        SM_UI_menuLayout_(SM_UI *ao);
static void        SM_UI_menuDraw_(SM_UI *ao);
static void        SM_UI_menuShow_(SM_UI *ao);
static void        SM_UI_menuHide_(SM_UI *ao);
static MenuAction  SM_UI_decodeMenuAction_(uint32_t sel);
static void        SM_UI_drawTextArea_(struct TextBufferView *view,
                                       uint32_t rows,
                                       uint32_t cols);
static void        SM_UI_drawScrollBar_(struct ncplane *plane, int x,
                                        ScrollBar const *bar,
                                        ScrollBar_Thumb const *thumb);
static void        SM_UI_refreshMain_(SM_UI *ao);
static void        SM_UI_setKeybarClosed_(struct ncplane *kp);
static void        SM_UI_setKeybarOpen_(struct ncplane *kp);

// resize callbacks (registered via ncplane_options.resizecb)
static int         SM_UI_title_cb_(struct ncplane *n);
static int         SM_UI_status_cb_(struct ncplane *n);
static int         SM_UI_main_cb_(struct ncplane *n);
static int         SM_UI_content_cb_(struct ncplane *n);
static int         SM_UI_keybar_cb_(struct ncplane *n);
static int         SM_UI_menu_cb_(struct ncplane *n);

//============================================================================
//=== State tables

static SM_StatePtr SM_UI_TOP_initial(SM_Hsm *me) SM_HSM_RETT;

static SM_StatePtr SM_UI_active_init_(SM_Hsm *me) SM_HSM_RETT;
static void        SM_UI_active_entry_(SM_Hsm *me) SM_HSM_RETT;
static SM_RetState SM_UI_active_(SM_Hsm *me, UI_Evt const *e) SM_HSM_RETT;

SM_HsmState SM_HSM_ROM SM_UI_active = {
    (SM_StatePtr)0,                           // super (top)
    (SM_InitHandler)SM_UI_active_init_,       // init_ → showMain
    (SM_ActionHandler)&SM_UI_active_entry_,   // entry_
    (SM_ActionHandler)0,                      // exit_
    (SM_StateHandler)&SM_UI_active_           // handler_
};

static void        SM_UI_showMain_entry_(SM_Hsm *me) SM_HSM_RETT;
static SM_RetState SM_UI_showMain_(SM_Hsm *me, UI_Evt const *e) SM_HSM_RETT;

SM_HsmState SM_HSM_ROM SM_UI_showMain = {
    (SM_StatePtr)&SM_UI_active,                // super
    (SM_InitHandler)0,                        // init_ (leaf)
    (SM_ActionHandler)&SM_UI_showMain_entry_, // entry_
    (SM_ActionHandler)0,                      // exit_
    (SM_StateHandler)&SM_UI_showMain_         // handler_
};

static void        SM_UI_showMenu_entry_(SM_Hsm *me) SM_HSM_RETT;
static void        SM_UI_showMenu_exit_(SM_Hsm *me) SM_HSM_RETT;
static SM_RetState SM_UI_showMenu_(SM_Hsm *me, UI_Evt const *e) SM_HSM_RETT;

//--- menu items data ---
#define MENU_W_ 24U
#define MENU_NUM_ITEMS_ 4U
#define MENU_H_ (MENU_NUM_ITEMS_ + 2U)
static char const * const SM_UI_menuItems_[MENU_NUM_ITEMS_] = {
    "Resume",
    "Clear screen",
    "About",
    "Quit"
};

SM_HsmState SM_HSM_ROM SM_UI_showMenu = {
    (SM_StatePtr)&SM_UI_active,                // super
    (SM_InitHandler)0,                         // init_ (leaf)
    (SM_ActionHandler)&SM_UI_showMenu_entry_,  // entry_
    (SM_ActionHandler)&SM_UI_showMenu_exit_,   // exit_
    (SM_StateHandler)&SM_UI_showMenu_          // handler_
};

//============================================================================
//=== HSM implementations

static SM_StatePtr SM_UI_TOP_initial(SM_Hsm * const me) SM_HSM_RETT {
    SM_UI *ao = containerof(me, SM_UI, super);
    struct ncplane *std = notcurses_stdplane(ao->disp.nc);

    unsigned dimY, dimX;
    ncplane_dim_yx(std, &dimY, &dimX);

    uint64_t borderCh = NCCHANNELS_INITIALIZER(60, 60, 120, 15, 15, 35);

    // outer box
    ncplane_ascii_box(std, 0, borderCh, dimY, dimX, 0);

    // title
    {
        ncplane_options nopts = {
            .y = 1, .x = 2, .rows = 1, .cols = dimX - 4, .name = "title",
            .userptr = ao, .resizecb = SM_UI_title_cb_,
        };
        ao->disp.titlePlane = ncplane_create(std, &nopts);
        DBC_ENSURE(400, ao->disp.titlePlane != (struct ncplane *)0);
        ncplane_set_bg_rgb8(ao->disp.titlePlane, 60, 60, 120);
        ncplane_set_fg_rgb8(ao->disp.titlePlane, 230, 230, 255);
        ncplane_on_styles(ao->disp.titlePlane, NCSTYLE_BOLD);
        ncplane_puttext(ao->disp.titlePlane, 0, NCALIGN_LEFT,
                        " termbox ─ sm_tracer ", NULL);
    }

    // status
    {
        ncplane_options nopts = {
            .y = 3, .x = 2, .rows = 1, .cols = dimX - 4, .name = "status",
            .userptr = ao, .resizecb = SM_UI_status_cb_,
        };
        ao->disp.statusPlane = ncplane_create(std, &nopts);
        DBC_ENSURE(401, ao->disp.statusPlane != (struct ncplane *)0);
        ncplane_set_bg_rgb8(ao->disp.statusPlane, 35, 35, 60);
        ncplane_set_fg_rgb8(ao->disp.statusPlane, 200, 200, 200);
        ncplane_puttext(ao->disp.statusPlane, 0, NCALIGN_LEFT,
                        " ● disconnected ", NULL);
    }

    // main scroll — container with border + scrolling content child
    {
        unsigned mainRows = dimY - 7;
        unsigned mainCols = dimX - 4U;
        ncplane_options nopts = {
            .y = 5, .x = 2, .rows = mainRows, .cols = mainCols, .name = "main",
            .userptr = ao, .resizecb = SM_UI_main_cb_,
        };
        ao->disp.mainBuffer.framePlane = ncplane_create(std, &nopts);
        DBC_ENSURE(402, ao->disp.mainBuffer.framePlane != (struct ncplane *)0);
        ncplane_set_bg_rgb8(ao->disp.mainBuffer.framePlane, 25, 25, 40);
        ncplane_set_fg_rgb8(ao->disp.mainBuffer.framePlane, 200, 220, 200);
        {
            nccell base = NCCELL_TRIVIAL_INITIALIZER;
            nccell_set_bg_rgb8(&base, 25, 25, 40);
            nccell_set_fg_rgb8(&base, 200, 220, 200);
            nccell_load_char(ao->disp.mainBuffer.framePlane, &base, ' ');
            ncplane_set_base_cell(ao->disp.mainBuffer.framePlane, &base);
            nccell_release(ao->disp.mainBuffer.framePlane, &base);
        }
        ncplane_ascii_box(ao->disp.mainBuffer.framePlane, 0, borderCh, mainRows, mainCols, 0);

        // child content plane (scrolling, no border)
        unsigned contentRows = mainRows - 2U;
        unsigned contentCols = mainCols - 3U;
        ncplane_options cnopts = {
            .y = 1, .x = 1, .rows = contentRows, .cols = contentCols,
            .name = "mainContent",
            .userptr = ao, .resizecb = SM_UI_content_cb_,
        };
        ao->disp.mainBuffer.contentPlane = ncplane_create(ao->disp.mainBuffer.framePlane, &cnopts);
        DBC_ENSURE(405, ao->disp.mainBuffer.contentPlane != (struct ncplane *)0);
        ncplane_set_bg_rgb8(ao->disp.mainBuffer.contentPlane, 25, 25, 40);
        ncplane_set_fg_rgb8(ao->disp.mainBuffer.contentPlane, 200, 220, 200);
        {
            nccell base = NCCELL_TRIVIAL_INITIALIZER;
            nccell_set_bg_rgb8(&base, 25, 25, 40);
            nccell_set_fg_rgb8(&base, 200, 220, 200);
            nccell_load_char(ao->disp.mainBuffer.contentPlane, &base, ' ');
            ncplane_set_base_cell(ao->disp.mainBuffer.contentPlane, &base);
            nccell_release(ao->disp.mainBuffer.contentPlane, &base);
        }
    }

    // keybar
    {
        ncplane_options nopts = {
            .y = (int)(dimY - 2), .x = 2, .rows = 1, .cols = dimX - 4,
            .name = "keybar",
            .userptr = ao, .resizecb = SM_UI_keybar_cb_,
        };
        ao->disp.keybarPlane = ncplane_create(std, &nopts);
        DBC_ENSURE(403, ao->disp.keybarPlane != (struct ncplane *)0);
        SM_UI_setKeybarClosed_(ao->disp.keybarPlane);
    }

    SM_UI_menuCreate_(ao, std);

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

    case UI_RESIZE_SIG: {
        // callbacks handle geometry + borders/keybar text at new size.
        // title/status text is preserved by ncplane_resize_simple overlap.
        if (ao->disp.menu.visible) {
            SM_UI_menuShow_(ao);
        } else {
            SM_UI_menuHide_(ao);
        }
        return _SM_HANDLED();
    }

    case UI_BLINKY_TEXT_SIG:
    case UI_KEY_DEBUG_SIG: {
        UI_AppEvt const *ae = (UI_AppEvt const *)e;
        uint32_t written = TextArea_push(&ao->disp.mainBuffer.textArea,
                                          ae->pld.msg.text, ae->pld.msg.len);
        TextArea_preserveScrollOnAppend(&ao->disp.mainBuffer.textArea, written);
        SM_UI_refreshMain_(ao);
        return _SM_HANDLED();
    }

    case UI_KEY_PGUP_SIG: {
        unsigned rows;
        ncplane_dim_yx(ao->disp.mainBuffer.contentPlane, &rows, NULL);
        TextArea_scrollBy(&ao->disp.mainBuffer.textArea, (int32_t)(rows / 2U), rows);
        SM_UI_refreshMain_(ao);
        return _SM_HANDLED();
    }

    case UI_KEY_PGDN_SIG: {
        unsigned rows;
        ncplane_dim_yx(ao->disp.mainBuffer.contentPlane, &rows, NULL);
        TextArea_scrollBy(&ao->disp.mainBuffer.textArea, -(int32_t)(rows / 2U), rows);
        SM_UI_refreshMain_(ao);
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
static void SM_UI_showMenu_entry_(SM_Hsm * const me) SM_HSM_RETT {
    SM_UI *ao = containerof(me, SM_UI, super);

    ao->disp.menu.sel = 0U;
    SM_UI_menuShow_(ao);
    SM_UI_setKeybarOpen_(ao->disp.keybarPlane);
    ao->dirty = true;
}

static void SM_UI_showMenu_exit_(SM_Hsm * const me) SM_HSM_RETT {
    SM_UI *ao = containerof(me, SM_UI, super);
    SM_UI_menuHide_(ao);
    SM_UI_setKeybarClosed_(ao->disp.keybarPlane);
    ao->dirty = true;
}

static SM_RetState SM_UI_showMenu_(SM_Hsm * const me, UI_Evt const * const e) {
    SM_UI *ao = containerof(me, SM_UI, super);

    switch (e->sig) {
    case UI_KEY_DOWN_SIG:
    case UI_KEY_J_SIG: {
        uint32_t oldSel = ao->disp.menu.sel;
        uint32_t maxIdx = MENU_NUM_ITEMS_ - 1U;
        ao->disp.menu.sel = (ao->disp.menu.sel >= maxIdx) ? 0U : (ao->disp.menu.sel + 1U);
        SM_UI_drawMenuItem_(ao->disp.menu.plane, oldSel,
                            ao->disp.menu.sel, MENU_W_);
        SM_UI_drawMenuItem_(ao->disp.menu.plane, ao->disp.menu.sel,
                            ao->disp.menu.sel, MENU_W_);
        ao->dirty = true;
        return _SM_HANDLED();
    }

    case UI_KEY_UP_SIG:
    case UI_KEY_K_SIG: {
        uint32_t oldSel = ao->disp.menu.sel;
        uint32_t maxIdx = MENU_NUM_ITEMS_ - 1U;
        ao->disp.menu.sel = (ao->disp.menu.sel == 0U) ? maxIdx : (ao->disp.menu.sel - 1U);
        SM_UI_drawMenuItem_(ao->disp.menu.plane, oldSel,
                            ao->disp.menu.sel, MENU_W_);
        SM_UI_drawMenuItem_(ao->disp.menu.plane, ao->disp.menu.sel,
                            ao->disp.menu.sel, MENU_W_);
        ao->dirty = true;
        return _SM_HANDLED();
    }

    case UI_KEY_ENTER_SIG: {
        switch (SM_UI_decodeMenuAction_(ao->disp.menu.sel)) {
        case MENU_ACT_RESUME: {
            SM_UI_refreshMain_(ao);
            return _SM_TRAN(&SM_UI_showMain);
        }
        case MENU_ACT_CLEAR: {
            TextArea_clear(&ao->disp.mainBuffer.textArea);
            SM_UI_refreshMain_(ao);
            return _SM_TRAN(&SM_UI_showMain);
        }
        case MENU_ACT_ABOUT: {
            UI_postText(UI_BLINKY_TEXT_SIG,
                        "termbox v0.1 -- HSM demo\n"
                        "notcurses + SST + sm_hsm\n");
            SM_UI_refreshMain_(ao);
            return _SM_TRAN(&SM_UI_showMain);
        }
        case MENU_ACT_QUIT: {
            ao->quit = true;
            SM_UI_refreshMain_(ao);
            return _SM_TRAN(&SM_UI_showMain);
        }
        default:
            break;
        }
        return _SM_HANDLED();
    }

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

    me->disp.nc          = (struct notcurses *)0;
    me->disp.titlePlane  = (struct ncplane *)0;
    me->disp.statusPlane = (struct ncplane *)0;
    TextBufferView_init(&me->disp.mainBuffer);
    me->disp.keybarPlane = (struct ncplane *)0;
    me->disp.menu.plane = (struct ncplane *)0;
    me->disp.menu.sel    = 0U;
    me->disp.menu.visible = false;
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

//============================================================================
//=== IO helpers

static void SM_UI_menuCreate_(SM_UI * const ao,
                              struct ncplane * const parent)
{
    DBC_REQUIRE(304, ao != (SM_UI *)0);
    DBC_REQUIRE(305, parent != (struct ncplane *)0);

    ncplane_options nopts = {
        .y = 0, .x = 0,
        .rows = MENU_H_, .cols = MENU_W_,
        .name = "menu",
        .userptr = ao, .resizecb = SM_UI_menu_cb_,
    };
    ao->disp.menu.plane = ncplane_create(parent, &nopts);
    DBC_ENSURE(404, ao->disp.menu.plane != (struct ncplane *)0);
    ao->disp.menu.visible = false;
    SM_UI_menuHide_(ao);
}

static void SM_UI_menuLayout_(SM_UI * const ao) {
    DBC_REQUIRE(306, ao != (SM_UI *)0);
    DBC_REQUIRE(307, ao->disp.menu.plane != (struct ncplane *)0);

    struct ncplane * const std = notcurses_stdplane(ao->disp.nc);
    unsigned dimY;
    unsigned dimX;
    ncplane_dim_yx(std, &dimY, &dimX);

    int menuY = 0;
    int menuX = 0;
    if (dimY > MENU_H_) {
        menuY = (int)((dimY - MENU_H_) / 2U);
    }
    if (dimX > MENU_W_) {
        menuX = (int)((dimX - MENU_W_) / 2U);
    }

    struct ncplane * const mp = ao->disp.menu.plane;
    ncplane_move_yx(mp, menuY, menuX);

    unsigned rows;
    unsigned cols;
    ncplane_dim_yx(mp, &rows, &cols);
    if (rows != MENU_H_ || cols != MENU_W_) {
        ncplane_resize_simple(mp, MENU_H_, MENU_W_);
    }
}

static void SM_UI_menuDraw_(SM_UI * const ao) {
    DBC_REQUIRE(308, ao != (SM_UI *)0);
    DBC_REQUIRE(309, ao->disp.menu.plane != (struct ncplane *)0);

    struct ncplane * const mp = ao->disp.menu.plane;
    ncplane_erase(mp);

    ncplane_set_bg_rgb8(mp, 50, 50, 100);
    ncplane_set_fg_rgb8(mp, 140, 140, 200);
    ncplane_cursor_move_yx(mp, 0, 0);
    ncplane_putstr(mp, "|     ----menu----     |");

    ncplane_cursor_move_yx(mp, MENU_NUM_ITEMS_ + 1U, 0);
    ncplane_putstr(mp, "|----------------------|");

    for (uint32_t i = 0U; i < MENU_NUM_ITEMS_; ++i) {
        SM_UI_drawMenuItem_(mp, i, ao->disp.menu.sel, MENU_W_);
    }
}

static void SM_UI_menuShow_(SM_UI * const ao) {
    DBC_REQUIRE(310, ao != (SM_UI *)0);
    DBC_REQUIRE(311, ao->disp.menu.plane != (struct ncplane *)0);

    ao->disp.menu.visible = true;
    SM_UI_menuLayout_(ao);
    SM_UI_menuDraw_(ao);
    ncplane_move_top(ao->disp.menu.plane);
}

static void SM_UI_menuHide_(SM_UI * const ao) {
    DBC_REQUIRE(312, ao != (SM_UI *)0);
    DBC_REQUIRE(313, ao->disp.menu.plane != (struct ncplane *)0);

    ao->disp.menu.visible = false;
    SM_UI_menuLayout_(ao);
    ncplane_erase(ao->disp.menu.plane);
    ncplane_move_bottom(ao->disp.menu.plane);
}

static void SM_UI_drawMenuItem_(struct ncplane * const mp, uint32_t const idx,
                                uint32_t const sel, uint32_t const width)
{
    DBC_REQUIRE(301, idx < MENU_NUM_ITEMS_);
    DBC_REQUIRE(302, mp != (struct ncplane *)0);
    DBC_REQUIRE(303, width >= 3U);

    char line[32];
    (void)snprintf(line, sizeof(line), "| %-*s|", (int)(width - 3U),
                   SM_UI_menuItems_[idx]);

    if (idx == sel) {
        ncplane_set_bg_rgb8(mp, 80, 80, 160);
        ncplane_set_fg_rgb8(mp, 255, 255, 255);
    } else {
        ncplane_set_bg_rgb8(mp, 50, 50, 100);
        ncplane_set_fg_rgb8(mp, 200, 200, 220);
    }
    ncplane_cursor_move_yx(mp, (int)(idx + 1U), 0);
    ncplane_putstr(mp, line);
}

static MenuAction SM_UI_decodeMenuAction_(uint32_t const sel) {
    switch (sel) {
    case 0U:
        return MENU_ACT_RESUME;
    case 1U:
        return MENU_ACT_CLEAR;
    case 2U:
        return MENU_ACT_ABOUT;
    case 3U:
        return MENU_ACT_QUIT;
    default:
        return MENU_ACT_RESUME;
    }
}

static void SM_UI_drawTextArea_(struct TextBufferView * const view,
                                uint32_t const rows,
                                uint32_t const cols)
{
    TextArea_scrollBy(&view->textArea, 0, rows);
    uint32_t const start = TextArea_firstVisible(&view->textArea, rows);

    for (uint32_t i = 0U; i < rows; ++i) {
        uint32_t const bufIdx = start + i;
        if (cols > 1U && bufIdx < TextArea_total(&view->textArea)) {
            char textLine[512];
            int tw = (int)(cols - 1U);
            (void)snprintf(textLine, sizeof(textLine), "%-*.*s",
                           tw, tw, TextArea_lineAt(&view->textArea, bufIdx));
            ncplane_cursor_move_yx(view->contentPlane, (int)i, 0);
            ncplane_set_bg_rgb8(view->contentPlane, 25, 25, 40);
            ncplane_set_fg_rgb8(view->contentPlane, 200, 220, 200);
            ncplane_putstr(view->contentPlane, textLine);
        }
    }
}

static void SM_UI_drawScrollBar_(struct ncplane * const plane,
                                 int const x,
                                 ScrollBar const * const bar,
                                 ScrollBar_Thumb const * const thumb)
{
    if (!plane) { return; }
    if (!bar) { return; }
    if (!thumb) { return; }
    if (!bar->enabled) { return; }

    unsigned rows, cols;
    ncplane_dim_yx(plane, &rows, &cols);
    if ((int)x < 0 || (unsigned)x >= cols || rows == 0) { return; }

    for (uint32_t i = 0U; i < rows; ++i) {
        uint32_t const ch = ScrollBar_cellCh(thumb, i);
        if (ch) {
            ncplane_cursor_move_yx(plane, (int)i, x);
            ScrollBar_Color const *color = (ch == 0x2588U)
                                           ? &bar->style.thumb
                                           : &bar->style.track;
            ncplane_set_fg_rgb8(plane, color->r, color->g, color->b);
            ncplane_putstr(plane, ch == 0x2588U ? "\xe2\x96\x88" : "\xe2\x96\x91");
        }
    }
}

static void SM_UI_refreshMain_(SM_UI * const ao) {
    if (!ao->disp.mainBuffer.contentPlane) { return; }
    ncplane_set_bg_rgb8(ao->disp.mainBuffer.contentPlane, 25, 25, 40);
    ncplane_set_fg_rgb8(ao->disp.mainBuffer.contentPlane, 200, 220, 200);
    ncplane_erase(ao->disp.mainBuffer.contentPlane);

    unsigned rows, cols;
    ncplane_dim_yx(ao->disp.mainBuffer.contentPlane, &rows, &cols);
    if (rows == 0) { return; }

    SM_UI_drawTextArea_(&ao->disp.mainBuffer, rows, cols);

    if (cols > 0U) {
        ScrollBar_Thumb const thumb = ScrollBar_computeThumb(
            TextArea_total(&ao->disp.mainBuffer.textArea),
            rows,
            (uint32_t)TextArea_scrollOffset(&ao->disp.mainBuffer.textArea));
        SM_UI_drawScrollBar_(ao->disp.mainBuffer.contentPlane,
                             (int)(cols - 1U),
                             &ao->disp.mainBuffer.scrollBar,
                             &thumb);
    }
    ao->dirty = true;
}

typedef struct {
    char const *key;   // bold yellow
    char const *desc;  // normal gray
} KeyItem_;

static void SM_UI_setKeybar_(struct ncplane * const kp,
                             KeyItem_ const *items, uint32_t nItems)
{
    ncplane_erase(kp);
    ncplane_set_bg_rgb8(kp, 50, 50, 80);
    for (uint32_t i = 0U; i < nItems; ++i) {
        ncplane_set_fg_rgb8(kp, 230, 200, 100);
        ncplane_on_styles(kp, NCSTYLE_BOLD);
        ncplane_putstr(kp, items[i].key);
        ncplane_off_styles(kp, NCSTYLE_BOLD);
        ncplane_set_fg_rgb8(kp, 160, 160, 180);
        ncplane_putstr(kp, items[i].desc);
    }
}

static void SM_UI_setKeybarClosed_(struct ncplane * const kp) {
    static KeyItem_ const items[] = {
        { "  ctrl+/", " open menu" },
    };
    SM_UI_setKeybar_(kp, items, sizeof(items)/sizeof(items[0]));
}

static void SM_UI_setKeybarOpen_(struct ncplane * const kp) {
    static KeyItem_ const items[] = {
        { "  ctrl+/", " close menu" },
        { "  j/k \xe2\x86\x91\xe2\x86\x93", " navigate" },
        { "  enter", " select" },
    };
    SM_UI_setKeybar_(kp, items, sizeof(items)/sizeof(items[0]));
}

//============================================================================
//=== Resize callbacks — auto-triggered by notcurses when parent plane resizes

static int SM_UI_title_cb_(struct ncplane * const n) {
    struct ncplane *parent = ncplane_parent(n);
    unsigned px;
    ncplane_dim_yx(parent, NULL, &px);
    ncplane_resize_simple(n, 1, px - 4U);
    return 0;
}

static int SM_UI_status_cb_(struct ncplane * const n) {
    struct ncplane *parent = ncplane_parent(n);
    unsigned px;
    ncplane_dim_yx(parent, NULL, &px);
    ncplane_resize_simple(n, 1, px - 4U);
    return 0;
}

static int SM_UI_main_cb_(struct ncplane * const n) {
    struct ncplane *parent = ncplane_parent(n); // std
    unsigned py, px;
    ncplane_dim_yx(parent, &py, &px);
    unsigned rows = py - 7U;
    unsigned cols = px - 4U;
    ncplane_resize_simple(n, rows, cols);
    // draw borders here — callback fires AFTER std has new dims
    uint64_t bc = NCCHANNELS_INITIALIZER(60, 60, 120, 15, 15, 35);
    ncplane_ascii_box(n, 0, bc, rows, cols, 0);
    ncplane_ascii_box(parent, 0, bc, py, px, 0);
    return 0;
}

static int SM_UI_content_cb_(struct ncplane * const n) {
    struct ncplane *parent = ncplane_parent(n);
    unsigned py, px;
    ncplane_dim_yx(parent, &py, &px);
    if (py >= 2U && px >= 2U) {
        ncplane_resize_simple(n, py - 2U, px - 3U);
    }
    SM_UI *ao = ncplane_userptr(n);
    SM_UI_refreshMain_(ao);
    return 0;
}

static int SM_UI_keybar_cb_(struct ncplane * const n) {
    struct ncplane *parent = ncplane_parent(n); // std
    unsigned py, px;
    ncplane_dim_yx(parent, &py, &px);
    ncplane_move_yx(n, (int)(py - 2U), 2);
    ncplane_resize_simple(n, 1, px - 4U);
    SM_UI *ao = ncplane_userptr(n);
    if (ao->disp.menu.visible) {
        SM_UI_setKeybarOpen_(n);
    } else {
        SM_UI_setKeybarClosed_(n);
    }
    return 0;
}

static int SM_UI_menu_cb_(struct ncplane * const n) {
    SM_UI *ao = ncplane_userptr(n);
    DBC_REQUIRE(314, ao != (SM_UI *)0);
    if (ao->disp.menu.visible) {
        SM_UI_menuShow_(ao);
    } else {
        SM_UI_menuHide_(ao);
    }
    return 0;
}
