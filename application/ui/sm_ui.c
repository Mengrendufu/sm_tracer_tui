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
//--- IO helpers ---

typedef enum {
    MENU_ACT_RESUME,
    MENU_ACT_CLEAR,
    MENU_ACT_ABOUT,
    MENU_ACT_QUIT
} MenuAction;

static void        SM_UI_drawMenuItem_(struct ncplane *mp, uint32_t idx, uint32_t sel);
static MenuAction  SM_UI_execMenuAction_(uint32_t sel,
                                       uint32_t *total,
                                       uint32_t *head);
static void        SM_UI_refreshMain_(SM_UI *ao);
static void        SM_UI_setKeybarClosed_(struct ncplane *kp);
static void        SM_UI_setKeybarOpen_(struct ncplane *kp);

// resize callbacks (registered via ncplane_options.resizecb)
static int         SM_UI_title_cb_(struct ncplane *n);
static int         SM_UI_status_cb_(struct ncplane *n);
static int         SM_UI_main_cb_(struct ncplane *n);
static int         SM_UI_content_cb_(struct ncplane *n);
static int         SM_UI_keybar_cb_(struct ncplane *n);

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
#define MENU_NUM_ITEMS_ 4U
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
    struct ncplane *std = notcurses_stdplane(ao->nc);

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
        ao->titlePlane = ncplane_create(std, &nopts);
        DBC_ENSURE(400, ao->titlePlane != (struct ncplane *)0);
        ncplane_set_bg_rgb8(ao->titlePlane, 60, 60, 120);
        ncplane_set_fg_rgb8(ao->titlePlane, 230, 230, 255);
        ncplane_on_styles(ao->titlePlane, NCSTYLE_BOLD);
        ncplane_puttext(ao->titlePlane, 0, NCALIGN_LEFT,
                        " termbox ─ sm_tracer ", NULL);
    }

    // status
    {
        ncplane_options nopts = {
            .y = 3, .x = 2, .rows = 1, .cols = dimX - 4, .name = "status",
            .userptr = ao, .resizecb = SM_UI_status_cb_,
        };
        ao->statusPlane = ncplane_create(std, &nopts);
        DBC_ENSURE(401, ao->statusPlane != (struct ncplane *)0);
        ncplane_set_bg_rgb8(ao->statusPlane, 35, 35, 60);
        ncplane_set_fg_rgb8(ao->statusPlane, 200, 200, 200);
        ncplane_puttext(ao->statusPlane, 0, NCALIGN_LEFT,
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
        ao->mainPlane = ncplane_create(std, &nopts);
        DBC_ENSURE(402, ao->mainPlane != (struct ncplane *)0);
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
        ncplane_ascii_box(ao->mainPlane, 0, borderCh, mainRows, mainCols, 0);

        // child content plane (scrolling, no border)
        unsigned contentRows = mainRows - 2U;
        unsigned contentCols = mainCols - 3U;
        ncplane_options cnopts = {
            .y = 1, .x = 1, .rows = contentRows, .cols = contentCols,
            .name = "mainContent",
            .userptr = ao, .resizecb = SM_UI_content_cb_,
        };
        ao->mainContentPlane = ncplane_create(ao->mainPlane, &cnopts);
        DBC_ENSURE(405, ao->mainContentPlane != (struct ncplane *)0);
        ncplane_set_bg_rgb8(ao->mainContentPlane, 25, 25, 40);
        ncplane_set_fg_rgb8(ao->mainContentPlane, 200, 220, 200);
        {
            nccell base = NCCELL_TRIVIAL_INITIALIZER;
            nccell_set_bg_rgb8(&base, 25, 25, 40);
            nccell_set_fg_rgb8(&base, 200, 220, 200);
            nccell_load_char(ao->mainContentPlane, &base, ' ');
            ncplane_set_base_cell(ao->mainContentPlane, &base);
            nccell_release(ao->mainContentPlane, &base);
        }
    }

    // keybar
    {
        ncplane_options nopts = {
            .y = (int)(dimY - 2), .x = 2, .rows = 1, .cols = dimX - 4,
            .name = "keybar",
            .userptr = ao, .resizecb = SM_UI_keybar_cb_,
        };
        ao->keybarPlane = ncplane_create(std, &nopts);
        DBC_ENSURE(403, ao->keybarPlane != (struct ncplane *)0);
        SM_UI_setKeybarClosed_(ao->keybarPlane);
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

    case UI_RESIZE_SIG: {
        // callbacks handle geometry + borders/keybar text at new size.
        // title/status text is preserved by ncplane_resize_simple overlap.
        if (ao->menuPlane) {
            ncplane_destroy(ao->menuPlane);
            ao->menuPlane = (struct ncplane *)0;
        }
        return _SM_HANDLED();
    }

    case UI_BLINKY_TEXT_SIG:
    case UI_KEY_DEBUG_SIG: {
        UI_AppEvt const *ae = (UI_AppEvt const *)e;
        // split on \n and write each segment as a buffer line
        uint32_t written = 0U;
        char const *p = ae->pld.msg.text;
        char const *end = p + ae->pld.msg.len;
        while (p < end) {
            char const *nl = strchr(p, '\n');
            size_t segLen = nl ? (size_t)(nl - p) : (size_t)(end - p);

            // circular buffer: advance head when full
            uint32_t slot;
            if (ao->lineTotal >= LINE_BUF_CAP_) {
                slot = ao->lineHead;
                ao->lineHead = (ao->lineHead + 1U) % LINE_BUF_CAP_;
            } else {
                slot = (ao->lineHead + ao->lineTotal) % LINE_BUF_CAP_;
                ++ao->lineTotal;
            }
            size_t copyLen = (segLen < LINE_WIDTH_ - 1U) ? segLen : (LINE_WIDTH_ - 1U);
            memcpy(ao->lineBuf[slot], p, copyLen);
            ao->lineBuf[slot][copyLen] = '\0';
            ++written;
            p = nl ? (nl + 1) : end;
        }
        // keep view stable when scrolled up
        if (ao->scrollOff > 0) {
            ao->scrollOff += (int32_t)written;
        }
        SM_UI_refreshMain_(ao);
        return _SM_HANDLED();
    }

    case UI_KEY_PGUP_SIG: {
        unsigned rows;
        ncplane_dim_yx(ao->mainContentPlane, &rows, NULL);
        ao->scrollOff += (int32_t)(rows / 2U);
        SM_UI_refreshMain_(ao);
        return _SM_HANDLED();
    }

    case UI_KEY_PGDN_SIG: {
        unsigned rows;
        ncplane_dim_yx(ao->mainContentPlane, &rows, NULL);
        ao->scrollOff -= (int32_t)(rows / 2U);
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
    DBC_ENSURE(404, ao->menuPlane != (struct ncplane *)0);
    ncplane_set_bg_rgb8(ao->menuPlane, 50, 50, 100);
    ncplane_set_fg_rgb8(ao->menuPlane, 200, 200, 220);

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
        SM_UI_drawMenuItem_(ao->menuPlane, i, ao->menuSel);
    }
    SM_UI_setKeybarOpen_(ao->keybarPlane);
    ao->dirty = true;
}

static void SM_UI_showMenu_exit_(SM_Hsm * const me) SM_HSM_RETT {
    SM_UI *ao = containerof(me, SM_UI, super);
    if (ao->menuPlane) {
        ncplane_destroy(ao->menuPlane);
        ao->menuPlane = (struct ncplane *)0;
    }
    SM_UI_setKeybarClosed_(ao->keybarPlane);
    ao->dirty = true;
}

static SM_RetState SM_UI_showMenu_(SM_Hsm * const me, UI_Evt const * const e) {
    SM_UI *ao = containerof(me, SM_UI, super);

    switch (e->sig) {
    case UI_KEY_DOWN_SIG:
    case UI_KEY_J_SIG: {
        uint32_t oldSel = ao->menuSel;
        uint32_t maxIdx = MENU_NUM_ITEMS_ - 1U;
        ao->menuSel = (ao->menuSel >= maxIdx) ? 0U : (ao->menuSel + 1U);
        SM_UI_drawMenuItem_(ao->menuPlane, oldSel, ao->menuSel);
        SM_UI_drawMenuItem_(ao->menuPlane, ao->menuSel, ao->menuSel);
        ao->dirty = true;
        return _SM_HANDLED();
    }

    case UI_KEY_UP_SIG:
    case UI_KEY_K_SIG: {
        uint32_t oldSel = ao->menuSel;
        uint32_t maxIdx = MENU_NUM_ITEMS_ - 1U;
        ao->menuSel = (ao->menuSel == 0U) ? maxIdx : (ao->menuSel - 1U);
        SM_UI_drawMenuItem_(ao->menuPlane, oldSel, ao->menuSel);
        SM_UI_drawMenuItem_(ao->menuPlane, ao->menuSel, ao->menuSel);
        ao->dirty = true;
        return _SM_HANDLED();
    }

    case UI_KEY_ENTER_SIG: {
        switch (SM_UI_execMenuAction_(ao->menuSel, &ao->lineTotal, &ao->lineHead)) {
        case MENU_ACT_RESUME:
        case MENU_ACT_CLEAR:
            SM_UI_refreshMain_(ao);
            return _SM_TRAN(&SM_UI_showMain);
        case MENU_ACT_ABOUT:
            SM_UI_refreshMain_(ao);
            return _SM_TRAN(&SM_UI_showMain);
        case MENU_ACT_QUIT:
            ao->quit = true;
            SM_UI_refreshMain_(ao);
            return _SM_TRAN(&SM_UI_showMain);
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

    me->nc          = (struct notcurses *)0;
    me->titlePlane  = (struct ncplane *)0;
    me->statusPlane = (struct ncplane *)0;
    me->mainPlane   = (struct ncplane *)0;
    me->mainContentPlane = (struct ncplane *)0;
    me->keybarPlane = (struct ncplane *)0;
    me->menuPlane   = (struct ncplane *)0;
    me->lineTotal     = 0U;
    me->lineHead      = 0U;
    me->scrollOff     = 0;
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

//============================================================================
//=== IO helpers

static void SM_UI_drawMenuItem_(struct ncplane * const mp, uint32_t const idx,
                               uint32_t const sel) {
    DBC_REQUIRE(301, idx < MENU_NUM_ITEMS_);
    DBC_REQUIRE(302, mp != (struct ncplane *)0);

    unsigned dimX;
    ncplane_dim_yx(mp, NULL, &dimX);

    char line[32];
    (void)snprintf(line, sizeof(line), "| %-*s", (int)(dimX - 2),
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

static MenuAction SM_UI_execMenuAction_(uint32_t const sel,
                                       uint32_t * const total,
                                       uint32_t * const head)
{
    switch (sel) {
    case 0U:
        return MENU_ACT_RESUME;
    case 1U:
        *total = 0U;
        *head = 0U;
        return MENU_ACT_CLEAR;
    case 2U:
        UI_postText(UI_BLINKY_TEXT_SIG,
                    "termbox v0.1 -- HSM demo\n"
                    "notcurses + SST + sm_hsm\n");
        return MENU_ACT_ABOUT;
    case 3U:
        return MENU_ACT_QUIT;
    default:
        return MENU_ACT_RESUME;
    }
}

static void SM_UI_refreshMain_(SM_UI * const ao) {
    if (!ao->mainContentPlane) { return; }
    ncplane_set_bg_rgb8(ao->mainContentPlane, 25, 25, 40);
    ncplane_set_fg_rgb8(ao->mainContentPlane, 200, 220, 200);
    ncplane_erase(ao->mainContentPlane);

    unsigned rows, cols;
    ncplane_dim_yx(ao->mainContentPlane, &rows, &cols);
    if (rows == 0) { return; }

    // clamp scroll offset
    int32_t maxScroll = (ao->lineTotal > rows)
                        ? (int32_t)(ao->lineTotal - rows) : 0;
    if (ao->scrollOff > maxScroll) { ao->scrollOff = maxScroll; }
    if (ao->scrollOff < 0)         { ao->scrollOff = 0; }

    uint32_t start = (ao->lineTotal > rows)
                     ? ao->lineTotal - rows - (uint32_t)ao->scrollOff
                     : 0U;

    // proportional thumb
    uint32_t thumbHeight = 0U;
    uint32_t thumbPos = 0U;
    if (maxScroll > 0) {
        thumbHeight = (uint64_t)rows * (uint64_t)rows / (uint64_t)ao->lineTotal;
        if (thumbHeight < 1U) { thumbHeight = 1U; }
        if (thumbHeight > rows) { thumbHeight = rows; }
        uint32_t trackSlop = rows - thumbHeight;
        uint32_t distFromBottom = (uint32_t)(maxScroll - ao->scrollOff);
        thumbPos = (trackSlop > 0)
            ? (uint32_t)((uint64_t)distFromBottom * trackSlop / (uint64_t)maxScroll)
            : 0U;
    }

    for (uint32_t i = 0U; i < rows; ++i) {
        // text line (only if content exists at this row)
        uint32_t bufIdx = start + i;
        if (cols > 1U && bufIdx < ao->lineTotal) {
            uint32_t physIdx = (ao->lineHead + bufIdx) % LINE_BUF_CAP_;
            char textLine[512];
            int tw = (int)(cols - 1U);
            (void)snprintf(textLine, sizeof(textLine), "%-*.*s",
                           tw, tw, ao->lineBuf[physIdx]);
            ncplane_cursor_move_yx(ao->mainContentPlane, (int)i, 0);
            ncplane_set_bg_rgb8(ao->mainContentPlane, 25, 25, 40);
            ncplane_set_fg_rgb8(ao->mainContentPlane, 200, 220, 200);
            ncplane_putstr(ao->mainContentPlane, textLine);
        }

        // scrollbar at last column (drawn for ALL rows)
        if (cols > 0U) {
            ncplane_cursor_move_yx(ao->mainContentPlane, (int)i, (int)(cols - 1U));
            if (thumbHeight > 0 && i >= thumbPos && i < thumbPos + thumbHeight) {
                ncplane_set_fg_rgb8(ao->mainContentPlane, 140, 140, 200);
                ncplane_putstr(ao->mainContentPlane, "\xe2\x96\x88");
            } else if (maxScroll > 0) {
                ncplane_set_fg_rgb8(ao->mainContentPlane, 60, 60, 100);
                ncplane_putstr(ao->mainContentPlane, "\xe2\x96\x91");
            }
        }
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
    if (ao->menuPlane) {
        SM_UI_setKeybarOpen_(n);
    } else {
        SM_UI_setKeybarClosed_(n);
    }
    return 0;
}
