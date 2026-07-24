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
#include <notcurses/notcurses.h>
#include "sm_port.h"
#include "sm_hsm.h"
#include "dbc_assert.h"
#include "sm_ui_key.h"
#include "widgets/text_buffer_view.h"
#include "sm_ui.h"
DBC_MODULE_NAME("ui_hsm")

//============================================================================
//=== Module-owned SM_UI instance and internal component graph.

struct Menu {
    struct ncplane *plane;
    uint32_t        sel;
    bool            visible;
};

enum {
    SM_UI_STATUS_SHORT_LEN_ = 16,
    SM_UI_STATUS_PORT_LEN_  = 128,
    SM_UI_STATUS_PROTO_LEN_ = 32
};

struct StatusLine {
    char connection[SM_UI_STATUS_SHORT_LEN_];
    char port[SM_UI_STATUS_PORT_LEN_];
    char baud[SM_UI_STATUS_SHORT_LEN_];
    char dataBits[SM_UI_STATUS_SHORT_LEN_];
    char stopBits[SM_UI_STATUS_SHORT_LEN_];
    char parity[SM_UI_STATUS_SHORT_LEN_];
    char flow[SM_UI_STATUS_SHORT_LEN_];
    char protocol[SM_UI_STATUS_PROTO_LEN_];
};

struct NcDisp {
    struct notcurses *nc;
    struct ncplane   *titlePlane;
    struct ncplane   *statusPlane;
    struct StatusLine status;
    struct TextBufferView mainBuffer;
    struct ncplane   *keybarPlane;
    struct Menu       menu;
    bool              mainBufferDirty;
};

typedef struct {
    SM_Hsm super;
    VC_Handler init;
    VC_Handler dispatch;
    struct NcDisp disp;
    SM_UI_Key cmdHsm;
} SM_UI;

static SM_UI SM_UI_inst_;
static SM_UI_HostOps SM_UI_hostOps_;

//============================================================================
//--- Private declarations ---

typedef enum {
    MENU_ACT_RESUME,
    MENU_ACT_CLEAR,
    MENU_ACT_ABOUT,
    MENU_ACT_QUIT
} MenuAction;

typedef struct {
    char const *key;   // bold yellow
    char const *desc;  // normal gray
} KeyItem_;

typedef struct {
    unsigned rows;
    unsigned cols;
} SM_UI_MainBufferMetrics_;

static bool        SM_UI_plane_strWidth_(char const *text,
                                         unsigned *width);
static bool        SM_UI_plane_canPutStr_(struct ncplane *plane,
                                          unsigned x,
                                          char const *text,
                                          unsigned *width);
static bool        SM_UI_plane_putStrYx_(struct ncplane *plane,
                                         int y,
                                         unsigned x,
                                         char const *text);
static bool        SM_UI_plane_putStr_(struct ncplane *plane,
                                       char const *text);
static unsigned    SM_UI_std_panelCols_(unsigned cols);
static int         SM_UI_std_keybarY_(unsigned rows);

// Component IO helper declarations:
// - touches one component or one direct parent-child geometry relation.
// - does not know HSM transitions.
// - does not coordinate unrelated components.

// Title plane IO:
// - local IO writes title-local style and text only.
// - std/title IO allocates or resizes title against the stdplane.
// Title-local IO.
static void        SM_UI_title_draw_(struct ncplane *titlePlane);

// Std/title plane IO.
static struct ncplane *SM_UI_title_create_(struct ncplane *stdPlane,
                                           void *owner, unsigned cols);
static void        SM_UI_std_title_resize_(struct ncplane *stdPlane,
                                           struct ncplane *titlePlane);

// Status plane IO:
// - local IO writes status-local style and text only.
// - std/status IO allocates or resizes status against the stdplane.
// Status-local IO.
static void        SM_UI_status_init_(struct StatusLine *status);
static void        SM_UI_status_setText_(char *dst, size_t dstLen,
                                         char const *value,
                                         char const *fallback);
static void        SM_UI_status_setConnection_(struct StatusLine *status,
                                               bool connected);
static void        SM_UI_status_setSerial_(
                        struct StatusLine *status,
                        char const *port,
                        char const *baud,
                        char const *dataBits,
                        char const *stopBits,
                        char const *parity,
                        char const *flow);
static void        SM_UI_status_setProtocol_(struct StatusLine *status,
                                             char const *protocol);
static void        SM_UI_status_draw_(struct ncplane *statusPlane,
                                      struct StatusLine const *status);
static void        SM_UI_status_drawCell_(struct ncplane *statusPlane,
                                          unsigned *x,
                                          char const *label,
                                          char const *value);

// Std/status plane IO.
static struct ncplane *SM_UI_status_create_(struct ncplane *stdPlane,
                                            void *owner, unsigned cols,
                                            struct StatusLine const *status);
static void        SM_UI_std_status_resize_(struct ncplane *stdPlane,
                                            struct ncplane *statusPlane);

// Menu IO:
// - local state IO updates or reads Menu without dispatching HSM actions.
// - local repaint IO draws menu-local visual state.
// - std/menu IO owns menu plane allocation, layout, and visibility.
// Menu-local state IO.
static void        SM_UI_menu_selectNext_(struct Menu *menu);
static void        SM_UI_menu_selectPrev_(struct Menu *menu);
static MenuAction  SM_UI_menu_decodeAction_(struct Menu const *menu);

// Menu-local repaint IO.
static void        SM_UI_menu_draw_(struct Menu const *menu);
static void        SM_UI_menu_show_(struct Menu *menu);
static void        SM_UI_menu_hide_(struct Menu *menu);
static void        SM_UI_menu_drawItem_(struct Menu const *menu,
                                        uint32_t idx);

// Std/menu plane IO.
static void        SM_UI_menu_create_(struct Menu *menu,
                                      struct ncplane *parent, void *owner);
static void        SM_UI_std_menu_layout_(struct ncplane *stdPlane,
                                          struct Menu *menu);
static void        SM_UI_std_menu_show_(struct ncplane *stdPlane,
                                        struct Menu *menu);
static void        SM_UI_std_menu_hide_(struct ncplane *stdPlane,
                                        struct Menu *menu);

// Keybar IO:
// - local hint IO writes supplied or canned hint text.
// - std/keybar IO allocates or resizes keybar against the stdplane.
// Keybar-local hint IO.
static void        SM_UI_keybar_set_(struct ncplane *keybarPlane,
                                     KeyItem_ const *items, uint32_t nItems);
static void        SM_UI_keybar_setClosed_(struct ncplane *keybarPlane);
static void        SM_UI_keybar_setOpen_(struct ncplane *keybarPlane);

// Std/keybar plane IO.
static struct ncplane *SM_UI_keybar_create_(struct ncplane *stdPlane,
                                            void *owner, unsigned rows,
                                            unsigned cols);
static void        SM_UI_std_keybar_resize_(struct ncplane *stdPlane,
                                            struct ncplane *keybarPlane);

// Interaction handler declarations:
// - coordinates multiple components or adapts external callback context.
// - owns the explicit coupling between leaf component IO helpers.
// - menu_keybar mirrors menu visibility into keybar mode.
// - std_menu reapplies menu layout and visibility after geometry changes.
// - std_menu_keybar actions combine menu and keybar updates.
// - std_mainBufferFrame owns stdplane sizing policy for TextBufferView.
static uint64_t    SM_UI_std_borderCh_(void);
static SM_UI_MainBufferMetrics_ SM_UI_std_mainBufferMetrics_(
                        unsigned rows,
                        unsigned cols);
static void        SM_UI_menu_keybar_sync_(struct Menu const *menu,
                                           struct ncplane *keybarPlane);
static void        SM_UI_std_menu_sync_(struct ncplane *stdPlane,
                                        struct Menu *menu);
static void        SM_UI_std_menu_keybar_show_(struct ncplane *stdPlane,
                                               struct Menu *menu,
                                               struct ncplane *keybarPlane);
static void        SM_UI_std_menu_keybar_hide_(struct ncplane *stdPlane,
                                               struct Menu *menu,
                                               struct ncplane *keybarPlane);
static void        SM_UI_std_menu_keybar_resize_(struct ncplane *stdPlane,
                                                 struct Menu *menu,
                                                 struct ncplane *keybarPlane);
static void        SM_UI_std_mainBufferFrame_resize_(
                        struct ncplane *stdPlane,
                        struct ncplane *framePlane);

// Display graph handlers:
// - HSM and callback code enter component IO through this layer.
// - owns NcDisp-level component wiring and component refresh state.
// - create wires the full display graph from the stdplane.
// - text, clear, and scroll actions mark main-buffer refresh in one place.
// - resize and flush centralize deferred repaint scheduling.
static void        SM_UI_disp_create_(struct NcDisp *disp, SM_UI *owner);
static void        SM_UI_disp_pushText_(struct NcDisp *disp,
                                        char const *text, size_t len);
static void        SM_UI_disp_clearMain_(struct NcDisp *disp);
static void        SM_UI_disp_scrollMainPageUp_(struct NcDisp *disp);
static void        SM_UI_disp_scrollMainPageDown_(struct NcDisp *disp);
static void        SM_UI_disp_showMenu_(struct NcDisp *disp);
static void        SM_UI_disp_hideMenu_(struct NcDisp *disp);
static void        SM_UI_disp_syncMenu_(struct NcDisp *disp);
static void        SM_UI_disp_selectMenuNext_(struct NcDisp *disp);
static void        SM_UI_disp_selectMenuPrev_(struct NcDisp *disp);
static MenuAction  SM_UI_disp_menuAction_(struct NcDisp const *disp);
static void        SM_UI_disp_resizeContent_(struct NcDisp *disp,
                                             struct ncplane *framePlane,
                                             struct ncplane *contentPlane);
static void        SM_UI_disp_resizeKeybar_(struct NcDisp *disp,
                                            struct ncplane *stdPlane,
                                            struct ncplane *keybarPlane);
static void        SM_UI_disp_resizeMenu_(struct NcDisp *disp,
                                          struct ncplane *stdPlane);
static void        SM_UI_disp_flush_(struct NcDisp *disp);
static void        SM_UI_disp_markMainBufferDirty_(struct NcDisp *disp);
static void        SM_UI_requestFrame_(void);

// Notcurses callback adapters:
// - adapt raw ncplane callback context back into the display graph layer.
// - keep resize callbacks free of component policy.
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
#define SM_UI_PLANE_X_ 2U
#define SM_UI_MAIN_Y_  5U
#define SM_UI_MAIN_MIN_ROWS_ 3U
#define SM_UI_MAIN_MIN_COLS_ 4U
#define SM_UI_SIDE_MARGIN_COLS_ 4U
#define SM_UI_MAIN_BOTTOM_ROWS_ 2U
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
    SM_UI_disp_create_(&ao->disp, ao);

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
        //--------------------------------------------------------------------
        //--- user input events
        case UI_TIMER_SIG: {
            return _SM_HANDLED();
        }

        case UI_RESIZE_SIG: {
            SM_UI_disp_syncMenu_(&ao->disp);
            return _SM_HANDLED();
        }

        case UI_TEXT_SIG:
        case UI_KEY_DEBUG_SIG: {
            UI_AppEvt const *ae = (UI_AppEvt const *)e;
            SM_UI_disp_pushText_(&ao->disp, ae->pld.msg.text,
                                ae->pld.msg.len);
            return _SM_HANDLED();
        }

        case UI_KEY_PGUP_SIG: {
            SM_UI_disp_scrollMainPageUp_(&ao->disp);
            return _SM_HANDLED();
        }

        case UI_KEY_PGDN_SIG: {
            SM_UI_disp_scrollMainPageDown_(&ao->disp);
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

static SM_RetState SM_UI_showMain_(SM_Hsm * const me,
                                   UI_Evt const * const e)
{
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
    SM_UI_disp_showMenu_(&ao->disp);
}

static void SM_UI_showMenu_exit_(SM_Hsm * const me) SM_HSM_RETT {
    SM_UI *ao = containerof(me, SM_UI, super);
    SM_UI_disp_hideMenu_(&ao->disp);
}

static SM_RetState SM_UI_showMenu_(SM_Hsm * const me,
                                   UI_Evt const * const e)
{
    SM_UI *ao = containerof(me, SM_UI, super);

    switch (e->sig) {
        case UI_KEY_DOWN_SIG:
        case UI_KEY_CTRL_N_SIG:
        case UI_KEY_J_SIG: {
            SM_UI_disp_selectMenuNext_(&ao->disp);
            return _SM_HANDLED();
        }

        case UI_KEY_UP_SIG:
        case UI_KEY_CTRL_P_SIG:
        case UI_KEY_K_SIG: {
            SM_UI_disp_selectMenuPrev_(&ao->disp);
            return _SM_HANDLED();
        }

        case UI_KEY_ENTER_SIG: {
            switch (SM_UI_disp_menuAction_(&ao->disp)) {
            case MENU_ACT_RESUME: {
                SM_UI_requestFrame_();
                return _SM_TRAN(&SM_UI_showMain);
            }
            case MENU_ACT_CLEAR: {
                SM_UI_disp_clearMain_(&ao->disp);
                return _SM_TRAN(&SM_UI_showMain);
            }
            case MENU_ACT_ABOUT: {
                char const about[] =
                    "termbox v0.1 -- HSM demo\n"
                    "notcurses + SST + sm_hsm\n";
                SM_UI_disp_pushText_(&ao->disp, about,
                                     sizeof(about) - 1U);
                return _SM_TRAN(&SM_UI_showMain);
            }
            case MENU_ACT_QUIT: {
                (*SM_UI_hostOps_.requestQuit)(SM_UI_hostOps_.ctx);
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

        case UI_KEY_ESC_SIG: {
            return _SM_TRAN(&SM_UI_showMain);
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

static void SM_UI_ctor_(SM_UI * const me) {
    DBC_REQUIRE(100, me != (SM_UI *)0);

    me->init     = (VC_Handler)SM_UI_init;
    me->dispatch = (VC_Handler)SM_UI_dispatch;

    me->disp.nc          = (struct notcurses *)0;
    me->disp.titlePlane  = (struct ncplane *)0;
    me->disp.statusPlane = (struct ncplane *)0;
    SM_UI_status_init_(&me->disp.status);
    TextBufferView_init(&me->disp.mainBuffer);
    me->disp.keybarPlane = (struct ncplane *)0;
    me->disp.menu.plane = (struct ncplane *)0;
    me->disp.menu.sel    = 0U;
    me->disp.menu.visible = false;
    me->disp.mainBufferDirty = false;
}

static void SM_UI_start_(SM_UI * const me) {
    DBC_REQUIRE(200, me != (SM_UI *)0);
    DBC_REQUIRE(201, me->init != (VC_Handler)0);
    (*me->init)(me, (void const *)0);
}

void SM_UI_setup(struct notcurses * const nc,
                 SM_UI_HostOps const * const hostOps)
{
    DBC_REQUIRE(503, nc != (struct notcurses *)0);
    DBC_REQUIRE(507, hostOps != (SM_UI_HostOps const *)0);
    DBC_REQUIRE(508, hostOps->requestQuit != (void (*)(void *))0);
    DBC_REQUIRE(509, hostOps->requestFrame != (void (*)(void *))0);

    SM_UI_hostOps_ = *hostOps;
    SM_UI_ctor_(&SM_UI_inst_);
    SM_UI_inst_.disp.nc = nc;
    SM_UI_start_(&SM_UI_inst_);
}

void SM_UI_flush(void) {
    SM_UI_disp_flush_(&SM_UI_inst_.disp);
}

void SM_UI_dispatchEvt(UI_Evt const * const e) {
    DBC_REQUIRE(505, e != (UI_Evt const *)0);
    DBC_REQUIRE(506, SM_UI_inst_.dispatch != (VC_Handler)0);

    (*SM_UI_inst_.dispatch)(&SM_UI_inst_, e);
}

//============================================================================
//=== Component IO Helpers
//
// These static IO helpers are intentionally local and narrow. They either
// operate on a single component or on one direct parent-child plane relation.
// They do not know about HSM transitions or coordinate unrelated components.

static bool SM_UI_plane_strWidth_(char const * const text,
                                  unsigned * const width)
{
    DBC_REQUIRE(338, text != (char const *)0);
    DBC_REQUIRE(339, width != (unsigned *)0);

    int const textWidth = ncstrwidth(text, NULL, NULL);
    if (textWidth < 0) {
        return false;
    }

    *width = (unsigned)textWidth;
    return true;
}

static bool SM_UI_plane_canPutStr_(struct ncplane * const plane,
                                   unsigned const x,
                                   char const * const text,
                                   unsigned * const width)
{
    DBC_REQUIRE(340, plane != (struct ncplane *)0);
    DBC_REQUIRE(341, text != (char const *)0);
    DBC_REQUIRE(366, width != (unsigned *)0);

    unsigned cols;
    ncplane_dim_yx(plane, NULL, &cols);
    if (!SM_UI_plane_strWidth_(text, width)) {
        return false;
    }

    return x < cols && *width <= (cols - x);
}

static bool SM_UI_plane_putStrYx_(struct ncplane * const plane,
                                  int const y,
                                  unsigned const x,
                                  char const * const text)
{
    DBC_REQUIRE(367, plane != (struct ncplane *)0);
    DBC_REQUIRE(368, text != (char const *)0);

    unsigned width;
    if (!SM_UI_plane_canPutStr_(plane, x, text, &width)) {
        return false;
    }

    return ncplane_putstr_yx(plane, y, (int)x, text) >= 0;
}

static bool SM_UI_plane_putStr_(struct ncplane * const plane,
                                char const * const text)
{
    DBC_REQUIRE(369, plane != (struct ncplane *)0);
    DBC_REQUIRE(370, text != (char const *)0);

    unsigned y;
    unsigned x;
    ncplane_cursor_yx(plane, &y, &x);
    return SM_UI_plane_putStrYx_(plane, (int)y, x, text);
}

static unsigned SM_UI_std_panelCols_(unsigned const cols) {
    if (cols > SM_UI_SIDE_MARGIN_COLS_) {
        return cols - SM_UI_SIDE_MARGIN_COLS_;
    }
    return 1U;
}

static int SM_UI_std_keybarY_(unsigned const rows) {
    SM_UI_MainBufferMetrics_ const main =
        SM_UI_std_mainBufferMetrics_(rows, SM_UI_MAIN_MIN_COLS_);
    return (int)(SM_UI_MAIN_Y_ + main.rows);
}

static struct ncplane *SM_UI_title_create_(struct ncplane * const stdPlane,
                                           void * const owner,
                                           unsigned const cols)
{
    DBC_REQUIRE(320, stdPlane != (struct ncplane *)0);

    ncplane_options nopts = {
        .y = 1, .x = 2, .rows = 1, .cols = cols, .name = "title",
        .userptr = owner, .resizecb = SM_UI_title_cb_,
    };
    struct ncplane * const titlePlane = ncplane_create(stdPlane, &nopts);
    DBC_ENSURE(400, titlePlane != (struct ncplane *)0);
    SM_UI_title_draw_(titlePlane);
    return titlePlane;
}

static void SM_UI_title_draw_(struct ncplane * const titlePlane) {
    DBC_REQUIRE(321, titlePlane != (struct ncplane *)0);

    ncplane_erase(titlePlane);
    ncplane_set_bg_rgb8(titlePlane, 60, 60, 120);
    ncplane_off_styles(titlePlane, NCSTYLE_BOLD);
    ncplane_set_fg_rgb8(titlePlane, 170, 175, 215);
    (void)SM_UI_plane_putStrYx_(titlePlane, 0, 0U, " sm_tracer_tui: ");

    ncplane_on_styles(titlePlane, NCSTYLE_BOLD);
    ncplane_set_fg_rgb8(titlePlane, 235, 235, 255);
    (void)SM_UI_plane_putStr_(titlePlane, "v0.0.1");

    ncplane_off_styles(titlePlane, NCSTYLE_BOLD);
    ncplane_set_fg_rgb8(titlePlane, 140, 145, 185);
    (void)SM_UI_plane_putStr_(titlePlane, " ");

    ncplane_on_styles(titlePlane, NCSTYLE_BOLD);
    ncplane_set_fg_rgb8(titlePlane, 170, 230, 210);
    (void)SM_UI_plane_putStr_(titlePlane, "notcurses ");
    ncplane_off_styles(titlePlane, NCSTYLE_BOLD);
}

static void SM_UI_std_title_resize_(struct ncplane * const stdPlane,
                                    struct ncplane * const titlePlane)
{
    DBC_REQUIRE(322, stdPlane != (struct ncplane *)0);
    DBC_REQUIRE(323, titlePlane != (struct ncplane *)0);

    unsigned cols;
    ncplane_dim_yx(stdPlane, NULL, &cols);
    ncplane_resize_simple(titlePlane, 1, SM_UI_std_panelCols_(cols));
}

static void SM_UI_status_init_(struct StatusLine * const status) {
    DBC_REQUIRE(328, status != (struct StatusLine *)0);

    SM_UI_status_setConnection_(status, false);
    SM_UI_status_setSerial_(status, (char const *)0, (char const *)0,
                            (char const *)0, (char const *)0,
                            (char const *)0, (char const *)0);
    SM_UI_status_setProtocol_(status, (char const *)0);
}

static void SM_UI_status_setText_(char * const dst,
                                  size_t const dstLen,
                                  char const * const value,
                                  char const * const fallback)
{
    DBC_REQUIRE(371, dst != (char *)0);
    DBC_REQUIRE(372, dstLen > 0U);
    DBC_REQUIRE(373, fallback != (char const *)0);

    char const * const src =
        (value != (char const *)0) ? value : fallback;
    int const n = snprintf(dst, dstLen, "%s", src);
    DBC_REQUIRE(374, n >= 0);
    DBC_ENSURE(402, dst[dstLen - 1U] == '\0');
}

static void SM_UI_status_setConnection_(struct StatusLine * const status,
                                        bool const connected)
{
    DBC_REQUIRE(375, status != (struct StatusLine *)0);

    SM_UI_status_setText_(status->connection,
                          sizeof(status->connection),
                          connected ? "connected" : "disconnected",
                          "disconnected");
}

static void SM_UI_status_setSerial_(
    struct StatusLine * const status,
    char const * const port,
    char const * const baud,
    char const * const dataBits,
    char const * const stopBits,
    char const * const parity,
    char const * const flow)
{
    DBC_REQUIRE(376, status != (struct StatusLine *)0);

    SM_UI_status_setText_(status->port, sizeof(status->port), port, "none");
    SM_UI_status_setText_(status->baud, sizeof(status->baud),
                          baud, "115200");
    SM_UI_status_setText_(status->dataBits, sizeof(status->dataBits),
                          dataBits, "8");
    SM_UI_status_setText_(status->stopBits, sizeof(status->stopBits),
                          stopBits, "1");
    SM_UI_status_setText_(status->parity, sizeof(status->parity),
                          parity, "none");
    SM_UI_status_setText_(status->flow, sizeof(status->flow), flow, "none");
}

static void SM_UI_status_setProtocol_(struct StatusLine * const status,
                                      char const * const protocol)
{
    DBC_REQUIRE(377, status != (struct StatusLine *)0);

    SM_UI_status_setText_(status->protocol, sizeof(status->protocol),
                          protocol, "none");
}

static struct ncplane *SM_UI_status_create_(
                           struct ncplane * const stdPlane,
                           void * const owner,
                           unsigned const cols,
                           struct StatusLine const * const status)
{
    DBC_REQUIRE(324, stdPlane != (struct ncplane *)0);
    DBC_REQUIRE(329, status != (struct StatusLine const *)0);

    ncplane_options nopts = {
        .y = 3, .x = 2, .rows = 1, .cols = cols, .name = "status",
        .userptr = owner, .resizecb = SM_UI_status_cb_,
    };
    struct ncplane * const statusPlane = ncplane_create(stdPlane, &nopts);
    DBC_ENSURE(401, statusPlane != (struct ncplane *)0);
    SM_UI_status_draw_(statusPlane, status);
    return statusPlane;
}

static void SM_UI_status_draw_(struct ncplane * const statusPlane,
                               struct StatusLine const * const status)
{
    DBC_REQUIRE(325, statusPlane != (struct ncplane *)0);
    DBC_REQUIRE(330, status != (struct StatusLine const *)0);

    ncplane_erase(statusPlane);
    ncplane_set_bg_rgb8(statusPlane, 35, 35, 60);
    ncplane_set_fg_rgb8(statusPlane, 200, 200, 200);

    unsigned x = 0U;
    SM_UI_status_drawCell_(statusPlane, &x, "status", status->connection);
    SM_UI_status_drawCell_(statusPlane, &x, "port", status->port);
    SM_UI_status_drawCell_(statusPlane, &x, "baud", status->baud);
    SM_UI_status_drawCell_(statusPlane, &x, "data", status->dataBits);
    SM_UI_status_drawCell_(statusPlane, &x, "stop", status->stopBits);
    SM_UI_status_drawCell_(statusPlane, &x, "parity", status->parity);
    SM_UI_status_drawCell_(statusPlane, &x, "flow", status->flow);
    SM_UI_status_drawCell_(statusPlane, &x, "proto", status->protocol);
}

static void SM_UI_status_drawCell_(struct ncplane * const statusPlane,
                                   unsigned * const x,
                                   char const * const label,
                                   char const * const value)
{
    DBC_REQUIRE(331, statusPlane != (struct ncplane *)0);
    DBC_REQUIRE(332, x != (unsigned *)0);
    DBC_REQUIRE(333, label != (char const *)0);
    DBC_REQUIRE(334, value != (char const *)0);

    char cell[32];
    char labelPart[16];
    char valuePart[24];

    int const labelN = snprintf(labelPart, sizeof(labelPart),
                                " %s: ", label);
    DBC_REQUIRE(335, labelN >= 0);
    if ((unsigned)labelN >= sizeof(labelPart)) {
        ncplane_dim_yx(statusPlane, NULL, x);
        return;
    }

    int const valueN = snprintf(valuePart, sizeof(valuePart),
                                "%s ", value);
    DBC_REQUIRE(336, valueN >= 0);
    if ((unsigned)valueN >= sizeof(valuePart)) {
        ncplane_dim_yx(statusPlane, NULL, x);
        return;
    }

    int const n = snprintf(cell, sizeof(cell), "%s%s",
                           labelPart, valuePart);
    DBC_REQUIRE(337, n >= 0);
    if ((unsigned)n >= sizeof(cell)) {
        ncplane_dim_yx(statusPlane, NULL, x);
        return;
    }

    unsigned width;
    if (!SM_UI_plane_canPutStr_(statusPlane, *x, cell, &width)) {
        ncplane_dim_yx(statusPlane, NULL, x);
        return;
    }

    ncplane_on_styles(statusPlane, NCSTYLE_BOLD);
    ncplane_set_fg_rgb8(statusPlane, 155, 165, 220);
    (void)SM_UI_plane_putStrYx_(statusPlane, 0, *x, labelPart);

    ncplane_off_styles(statusPlane, NCSTYLE_BOLD);
    ncplane_set_fg_rgb8(statusPlane, 220, 225, 240);
    (void)SM_UI_plane_putStr_(statusPlane, valuePart);
    ncplane_set_fg_rgb8(statusPlane, 200, 200, 200);

    *x += width + 2U;
}

static void SM_UI_std_status_resize_(struct ncplane * const stdPlane,
                                     struct ncplane * const statusPlane)
{
    DBC_REQUIRE(326, stdPlane != (struct ncplane *)0);
    DBC_REQUIRE(327, statusPlane != (struct ncplane *)0);

    unsigned cols;
    ncplane_dim_yx(stdPlane, NULL, &cols);
    ncplane_resize_simple(statusPlane, 1, SM_UI_std_panelCols_(cols));
}

static void SM_UI_menu_create_(struct Menu * const menu,
                               struct ncplane * const parent,
                               void * const owner)
{
    DBC_REQUIRE(304, menu != (struct Menu *)0);
    DBC_REQUIRE(305, parent != (struct ncplane *)0);

    ncplane_options nopts = {
        .y = 0, .x = 0,
        .rows = MENU_H_, .cols = MENU_W_,
        .name = "menu",
        .userptr = owner, .resizecb = SM_UI_menu_cb_,
    };
    menu->plane = ncplane_create(parent, &nopts);
    DBC_ENSURE(404, menu->plane != (struct ncplane *)0);
    menu->visible = false;
    SM_UI_std_menu_hide_(parent, menu);
}

static void SM_UI_std_menu_layout_(struct ncplane * const stdPlane,
                                   struct Menu * const menu)
{
    DBC_REQUIRE(306, stdPlane != (struct ncplane *)0);
    DBC_REQUIRE(307, menu != (struct Menu *)0);
    DBC_REQUIRE(308, menu->plane != (struct ncplane *)0);

    unsigned dimY;
    unsigned dimX;
    ncplane_dim_yx(stdPlane, &dimY, &dimX);

    int menuY = 0;
    int menuX = 0;
    if (dimY > MENU_H_) {
        menuY = (int)((dimY - MENU_H_) / 2U);
    }
    if (dimX > MENU_W_) {
        menuX = (int)((dimX - MENU_W_) / 2U);
    }

    ncplane_move_yx(menu->plane, menuY, menuX);

    unsigned rows;
    unsigned cols;
    ncplane_dim_yx(menu->plane, &rows, &cols);
    if (rows != MENU_H_ || cols != MENU_W_) {
        ncplane_resize_simple(menu->plane, MENU_H_, MENU_W_);
    }
}

static void SM_UI_std_menu_show_(struct ncplane * const stdPlane,
                                 struct Menu * const menu)
{
    SM_UI_std_menu_layout_(stdPlane, menu);
    SM_UI_menu_show_(menu);
}

static void SM_UI_std_menu_hide_(struct ncplane * const stdPlane,
                                 struct Menu * const menu)
{
    SM_UI_std_menu_layout_(stdPlane, menu);
    SM_UI_menu_hide_(menu);
}

static void SM_UI_menu_draw_(struct Menu const * const menu) {
    DBC_REQUIRE(309, menu != (struct Menu const *)0);
    DBC_REQUIRE(310, menu->plane != (struct ncplane *)0);

    struct ncplane * const mp = menu->plane;
    ncplane_erase(mp);

    ncplane_set_bg_rgb8(mp, 50, 50, 100);
    ncplane_set_fg_rgb8(mp, 140, 140, 200);
    (void)SM_UI_plane_putStrYx_(mp, 0, 0U,
                                "|     ----menu----     |");
    (void)SM_UI_plane_putStrYx_(mp, (int)(MENU_NUM_ITEMS_ + 1U), 0U,
                                "|----------------------|");

    for (uint32_t i = 0U; i < MENU_NUM_ITEMS_; ++i) {
        SM_UI_menu_drawItem_(menu, i);
    }
}

static void SM_UI_menu_show_(struct Menu * const menu) {
    DBC_REQUIRE(311, menu != (struct Menu *)0);
    DBC_REQUIRE(312, menu->plane != (struct ncplane *)0);

    menu->visible = true;
    SM_UI_menu_draw_(menu);
    ncplane_move_top(menu->plane);
}

static void SM_UI_menu_hide_(struct Menu * const menu) {
    DBC_REQUIRE(313, menu != (struct Menu *)0);
    DBC_REQUIRE(314, menu->plane != (struct ncplane *)0);

    menu->visible = false;
    ncplane_erase(menu->plane);
    ncplane_move_bottom(menu->plane);
}

static void SM_UI_menu_drawItem_(struct Menu const * const menu,
                                 uint32_t const idx)
{
    DBC_REQUIRE(301, menu != (struct Menu const *)0);
    DBC_REQUIRE(302, menu->plane != (struct ncplane *)0);
    DBC_REQUIRE(303, idx < MENU_NUM_ITEMS_);

    char line[32];
    (void)snprintf(line, sizeof(line), "| %-*s|", (int)(MENU_W_ - 3U),
                   SM_UI_menuItems_[idx]);

    if (idx == menu->sel) {
        ncplane_set_bg_rgb8(menu->plane, 80, 80, 160);
        ncplane_set_fg_rgb8(menu->plane, 255, 255, 255);
    } else {
        ncplane_set_bg_rgb8(menu->plane, 50, 50, 100);
        ncplane_set_fg_rgb8(menu->plane, 200, 200, 220);
    }
    (void)SM_UI_plane_putStrYx_(menu->plane, (int)(idx + 1U), 0U, line);
}

static void SM_UI_menu_selectNext_(struct Menu * const menu) {
    DBC_REQUIRE(315, menu != (struct Menu *)0);

    uint32_t const oldSel = menu->sel;
    uint32_t const maxIdx = MENU_NUM_ITEMS_ - 1U;
    menu->sel = (menu->sel >= maxIdx) ? 0U : (menu->sel + 1U);
    SM_UI_menu_drawItem_(menu, oldSel);
    SM_UI_menu_drawItem_(menu, menu->sel);
}

static void SM_UI_menu_selectPrev_(struct Menu * const menu) {
    DBC_REQUIRE(316, menu != (struct Menu *)0);

    uint32_t const oldSel = menu->sel;
    uint32_t const maxIdx = MENU_NUM_ITEMS_ - 1U;
    menu->sel = (menu->sel == 0U) ? maxIdx : (menu->sel - 1U);
    SM_UI_menu_drawItem_(menu, oldSel);
    SM_UI_menu_drawItem_(menu, menu->sel);
}

static MenuAction SM_UI_menu_decodeAction_(struct Menu const * const menu) {
    DBC_REQUIRE(317, menu != (struct Menu const *)0);

    switch (menu->sel) {
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

static struct ncplane *SM_UI_keybar_create_(struct ncplane * const stdPlane,
                                            void * const owner,
                                            unsigned const rows,
                                            unsigned const cols)
{
    DBC_REQUIRE(342, stdPlane != (struct ncplane *)0);

    ncplane_options nopts = {
        .y = SM_UI_std_keybarY_(rows), .x = SM_UI_PLANE_X_,
        .rows = 1, .cols = SM_UI_std_panelCols_(cols),
        .name = "keybar",
        .userptr = owner, .resizecb = SM_UI_keybar_cb_,
    };
    struct ncplane * const keybarPlane = ncplane_create(stdPlane, &nopts);
    DBC_ENSURE(403, keybarPlane != (struct ncplane *)0);
    SM_UI_keybar_setClosed_(keybarPlane);
    return keybarPlane;
}

static void SM_UI_keybar_set_(struct ncplane * const keybarPlane,
                              KeyItem_ const *items,
                              uint32_t const nItems)
{
    DBC_REQUIRE(343, keybarPlane != (struct ncplane *)0);
    DBC_REQUIRE(344, items != (KeyItem_ const *)0);

    ncplane_erase(keybarPlane);
    ncplane_set_bg_rgb8(keybarPlane, 50, 50, 80);
    for (uint32_t i = 0U; i < nItems; ++i) {
        ncplane_set_fg_rgb8(keybarPlane, 230, 200, 100);
        ncplane_on_styles(keybarPlane, NCSTYLE_BOLD);
        if (!SM_UI_plane_putStr_(keybarPlane, items[i].key)) {
            ncplane_off_styles(keybarPlane, NCSTYLE_BOLD);
            break;
        }
        ncplane_off_styles(keybarPlane, NCSTYLE_BOLD);
        ncplane_set_fg_rgb8(keybarPlane, 160, 160, 180);
        if (!SM_UI_plane_putStr_(keybarPlane, items[i].desc)) {
            break;
        }
    }
}

static void SM_UI_keybar_setClosed_(struct ncplane * const keybarPlane) {
    static KeyItem_ const items[] = {
        { "  ctrl+/", " open menu" },
    };
    SM_UI_keybar_set_(keybarPlane, items, sizeof(items) / sizeof(items[0]));
}

static void SM_UI_keybar_setOpen_(struct ncplane * const keybarPlane) {
    static KeyItem_ const items[] = {
        { "  ctrl+/", " close menu" },
        { "  j/k \xe2\x86\x91\xe2\x86\x93", " navigate" },
        { "  enter", " select" },
    };
    SM_UI_keybar_set_(keybarPlane, items, sizeof(items) / sizeof(items[0]));
}

static void SM_UI_std_keybar_resize_(struct ncplane * const stdPlane,
                                     struct ncplane * const keybarPlane)
{
    DBC_REQUIRE(345, stdPlane != (struct ncplane *)0);
    DBC_REQUIRE(346, keybarPlane != (struct ncplane *)0);

    unsigned rows;
    unsigned cols;
    ncplane_dim_yx(stdPlane, &rows, &cols);
    ncplane_move_yx(keybarPlane, SM_UI_std_keybarY_(rows), SM_UI_PLANE_X_);
    ncplane_resize_simple(keybarPlane, 1, SM_UI_std_panelCols_(cols));
}

//============================================================================
//=== Interaction Handlers
//
// This region is the explicit coupling layer between component IO helpers.
// Handlers coordinate components, operate on the display graph, or adapt raw
// callback context back into that display graph.

//----------------------------------------------------------------------------
//--- Cross-component handlers
//
// These handlers are the explicit joins between leaf components. Function
// names list the component boundary being crossed so callers do not need to
// infer hidden coupling from the function body.

static uint64_t SM_UI_std_borderCh_(void) {
    return NCCHANNELS_INITIALIZER(60, 60, 120, 15, 15, 35);
}

static SM_UI_MainBufferMetrics_ SM_UI_std_mainBufferMetrics_(
    unsigned const rows,
    unsigned const cols)
{
    unsigned mainRows = SM_UI_MAIN_MIN_ROWS_;
    if (rows > (SM_UI_MAIN_Y_ + SM_UI_MAIN_BOTTOM_ROWS_)) {
        mainRows = rows - SM_UI_MAIN_Y_ - SM_UI_MAIN_BOTTOM_ROWS_;
        if (mainRows < SM_UI_MAIN_MIN_ROWS_) {
            mainRows = SM_UI_MAIN_MIN_ROWS_;
        }
    }

    unsigned mainCols = SM_UI_MAIN_MIN_COLS_;
    if (cols > SM_UI_SIDE_MARGIN_COLS_) {
        mainCols = cols - SM_UI_SIDE_MARGIN_COLS_;
        if (mainCols < SM_UI_MAIN_MIN_COLS_) {
            mainCols = SM_UI_MAIN_MIN_COLS_;
        }
    }

    SM_UI_MainBufferMetrics_ const metrics = {
        .rows = mainRows,
        .cols = mainCols,
    };
    return metrics;
}

static void SM_UI_menu_keybar_sync_(struct Menu const * const menu,
                                    struct ncplane * const keybarPlane)
{
    DBC_REQUIRE(347, menu != (struct Menu const *)0);
    DBC_REQUIRE(348, keybarPlane != (struct ncplane *)0);

    if (menu->visible) {
        SM_UI_keybar_setOpen_(keybarPlane);
    } else {
        SM_UI_keybar_setClosed_(keybarPlane);
    }
}

static void SM_UI_std_menu_sync_(struct ncplane * const stdPlane,
                                 struct Menu * const menu)
{
    DBC_REQUIRE(350, stdPlane != (struct ncplane *)0);
    DBC_REQUIRE(351, menu != (struct Menu *)0);

    if (menu->visible) {
        SM_UI_std_menu_show_(stdPlane, menu);
    } else {
        SM_UI_std_menu_hide_(stdPlane, menu);
    }
}

static void SM_UI_std_menu_keybar_show_(
    struct ncplane * const stdPlane,
    struct Menu * const menu,
    struct ncplane * const keybarPlane)
{
    DBC_REQUIRE(352, stdPlane != (struct ncplane *)0);
    DBC_REQUIRE(353, menu != (struct Menu *)0);
    DBC_REQUIRE(354, keybarPlane != (struct ncplane *)0);

    SM_UI_std_menu_show_(stdPlane, menu);
    SM_UI_keybar_setOpen_(keybarPlane);
}

static void SM_UI_std_menu_keybar_hide_(
    struct ncplane * const stdPlane,
    struct Menu * const menu,
    struct ncplane * const keybarPlane)
{
    DBC_REQUIRE(355, stdPlane != (struct ncplane *)0);
    DBC_REQUIRE(356, menu != (struct Menu *)0);
    DBC_REQUIRE(357, keybarPlane != (struct ncplane *)0);

    SM_UI_std_menu_hide_(stdPlane, menu);
    SM_UI_keybar_setClosed_(keybarPlane);
}

static void SM_UI_std_menu_keybar_resize_(
    struct ncplane * const stdPlane,
    struct Menu * const menu,
    struct ncplane * const keybarPlane)
{
    DBC_REQUIRE(358, stdPlane != (struct ncplane *)0);
    DBC_REQUIRE(359, menu != (struct Menu *)0);
    DBC_REQUIRE(360, keybarPlane != (struct ncplane *)0);

    SM_UI_std_keybar_resize_(stdPlane, keybarPlane);
    SM_UI_menu_keybar_sync_(menu, keybarPlane);
}

static void SM_UI_std_mainBufferFrame_resize_(
    struct ncplane * const stdPlane,
    struct ncplane * const framePlane)
{
    DBC_REQUIRE(363, stdPlane != (struct ncplane *)0);
    DBC_REQUIRE(364, framePlane != (struct ncplane *)0);

    unsigned rows;
    unsigned cols;
    ncplane_dim_yx(stdPlane, &rows, &cols);

    SM_UI_MainBufferMetrics_ const main =
        SM_UI_std_mainBufferMetrics_(rows, cols);

    TextBufferView_resizeFrame(framePlane, main.rows, main.cols,
                               SM_UI_std_borderCh_());
}

//----------------------------------------------------------------------------
//--- Display graph handlers
//
// HSM handlers and notcurses callbacks enter component IO through this NcDisp
// layer. It owns component wiring, component refresh state, and operations
// over the display graph. Frame scheduling remains host-owned; SM_UI requests
// it through SM_UI_HostOps.
// TODO: refine this layer after component OOP extraction. NcDisp should keep
// the explicit component-coupling policy while leaf component methods stay
// behind their own view APIs.

static void SM_UI_disp_create_(struct NcDisp * const disp,
                               SM_UI * const owner)
{
    DBC_REQUIRE(510, disp != (struct NcDisp *)0);
    DBC_REQUIRE(511, owner != (SM_UI *)0);
    DBC_REQUIRE(512, disp->nc != (struct notcurses *)0);

    struct ncplane * const std = notcurses_stdplane(disp->nc);
    DBC_REQUIRE(513, std != (struct ncplane *)0);

    unsigned dimY;
    unsigned dimX;
    ncplane_dim_yx(std, &dimY, &dimX);

    uint64_t const borderCh = SM_UI_std_borderCh_();
    SM_UI_MainBufferMetrics_ const main =
        SM_UI_std_mainBufferMetrics_(dimY, dimX);

    ncplane_ascii_box(std, 0, borderCh, dimY, dimX, 0);

    disp->titlePlane =
        SM_UI_title_create_(std, owner, SM_UI_std_panelCols_(dimX));
    disp->statusPlane =
        SM_UI_status_create_(std, owner, SM_UI_std_panelCols_(dimX),
                             &disp->status);
    TextBufferView_create(&disp->mainBuffer, std, owner,
                          main.rows, main.cols, borderCh,
                          SM_UI_main_cb_, SM_UI_content_cb_);
    disp->keybarPlane = SM_UI_keybar_create_(std, owner, dimY, dimX);
    SM_UI_menu_create_(&disp->menu, std, owner);

    SM_UI_requestFrame_();
}

static void SM_UI_disp_pushText_(struct NcDisp * const disp,
                                 char const * const text,
                                 size_t const len)
{
    DBC_REQUIRE(514, disp != (struct NcDisp *)0);

    TextBufferView_pushText(&disp->mainBuffer, text, len);
    SM_UI_disp_markMainBufferDirty_(disp);
}

static void SM_UI_disp_clearMain_(struct NcDisp * const disp) {
    DBC_REQUIRE(515, disp != (struct NcDisp *)0);

    TextBufferView_clear(&disp->mainBuffer);
    SM_UI_disp_markMainBufferDirty_(disp);
}

static void SM_UI_disp_scrollMainPageUp_(struct NcDisp * const disp) {
    DBC_REQUIRE(516, disp != (struct NcDisp *)0);

    TextBufferView_scrollPageUp(&disp->mainBuffer);
    SM_UI_disp_markMainBufferDirty_(disp);
}

static void SM_UI_disp_scrollMainPageDown_(struct NcDisp * const disp) {
    DBC_REQUIRE(517, disp != (struct NcDisp *)0);

    TextBufferView_scrollPageDown(&disp->mainBuffer);
    SM_UI_disp_markMainBufferDirty_(disp);
}

static void SM_UI_disp_showMenu_(struct NcDisp * const disp) {
    DBC_REQUIRE(518, disp != (struct NcDisp *)0);
    DBC_REQUIRE(519, disp->nc != (struct notcurses *)0);

    struct ncplane * const std = notcurses_stdplane(disp->nc);
    disp->menu.sel = 0U;
    SM_UI_std_menu_keybar_show_(std, &disp->menu, disp->keybarPlane);
    SM_UI_requestFrame_();
}

static void SM_UI_disp_hideMenu_(struct NcDisp * const disp) {
    DBC_REQUIRE(520, disp != (struct NcDisp *)0);
    DBC_REQUIRE(521, disp->nc != (struct notcurses *)0);

    struct ncplane * const std = notcurses_stdplane(disp->nc);
    SM_UI_std_menu_keybar_hide_(std, &disp->menu, disp->keybarPlane);
    SM_UI_requestFrame_();
}

static void SM_UI_disp_syncMenu_(struct NcDisp * const disp) {
    DBC_REQUIRE(522, disp != (struct NcDisp *)0);
    DBC_REQUIRE(523, disp->nc != (struct notcurses *)0);

    struct ncplane * const std = notcurses_stdplane(disp->nc);
    SM_UI_std_menu_sync_(std, &disp->menu);
    SM_UI_menu_keybar_sync_(&disp->menu, disp->keybarPlane);
}

static void SM_UI_disp_selectMenuNext_(struct NcDisp * const disp) {
    DBC_REQUIRE(524, disp != (struct NcDisp *)0);

    SM_UI_menu_selectNext_(&disp->menu);
    SM_UI_requestFrame_();
}

static void SM_UI_disp_selectMenuPrev_(struct NcDisp * const disp) {
    DBC_REQUIRE(525, disp != (struct NcDisp *)0);

    SM_UI_menu_selectPrev_(&disp->menu);
    SM_UI_requestFrame_();
}

static MenuAction SM_UI_disp_menuAction_(struct NcDisp const * const disp) {
    DBC_REQUIRE(526, disp != (struct NcDisp const *)0);
    return SM_UI_menu_decodeAction_(&disp->menu);
}

static void SM_UI_disp_resizeContent_(
    struct NcDisp * const disp,
    struct ncplane * const framePlane,
    struct ncplane * const contentPlane)
{
    DBC_REQUIRE(530, disp != (struct NcDisp *)0);

    TextBufferView_resizeContent(framePlane, contentPlane);
    SM_UI_disp_markMainBufferDirty_(disp);
}

static void SM_UI_disp_resizeKeybar_(struct NcDisp * const disp,
                                     struct ncplane * const stdPlane,
                                     struct ncplane * const keybarPlane)
{
    DBC_REQUIRE(531, disp != (struct NcDisp *)0);

    SM_UI_std_menu_keybar_resize_(stdPlane, &disp->menu, keybarPlane);
}

static void SM_UI_disp_resizeMenu_(struct NcDisp * const disp,
                                   struct ncplane * const stdPlane)
{
    DBC_REQUIRE(532, disp != (struct NcDisp *)0);

    SM_UI_std_menu_sync_(stdPlane, &disp->menu);
}

static void SM_UI_disp_flush_(struct NcDisp * const disp) {
    DBC_REQUIRE(527, disp != (struct NcDisp *)0);

    if (disp->mainBufferDirty) {
        TextBufferView_refresh(&disp->mainBuffer);
        disp->mainBufferDirty = false;
    }
}

static void SM_UI_disp_markMainBufferDirty_(struct NcDisp * const disp) {
    DBC_REQUIRE(529, disp != (struct NcDisp *)0);

    disp->mainBufferDirty = true;
    SM_UI_requestFrame_();
}

static void SM_UI_requestFrame_(void) {
    (*SM_UI_hostOps_.requestFrame)(SM_UI_hostOps_.ctx);
}

//----------------------------------------------------------------------------
//--- Notcurses callback adapters
//
// These handlers adapt raw notcurses callback context back into the display
// graph coupling layer. Geometry-specific leaf callbacks remain direct.

static int SM_UI_title_cb_(struct ncplane * const n) {
    struct ncplane *parent = ncplane_parent(n);
    SM_UI_std_title_resize_(parent, n);
    return 0;
}

static int SM_UI_status_cb_(struct ncplane * const n) {
    struct ncplane *parent = ncplane_parent(n);
    SM_UI_std_status_resize_(parent, n);
    return 0;
}

static int SM_UI_main_cb_(struct ncplane * const n) {
    struct ncplane *parent = ncplane_parent(n); // std
    SM_UI_std_mainBufferFrame_resize_(parent, n);
    return 0;
}

static int SM_UI_content_cb_(struct ncplane * const n) {
    struct ncplane *parent = ncplane_parent(n);
    SM_UI *ao = ncplane_userptr(n);
    DBC_REQUIRE(361, ao != (SM_UI *)0);
    SM_UI_disp_resizeContent_(&ao->disp, parent, n);
    return 0;
}

static int SM_UI_keybar_cb_(struct ncplane * const n) {
    struct ncplane *parent = ncplane_parent(n); // std
    SM_UI *ao = ncplane_userptr(n);
    DBC_REQUIRE(362, ao != (SM_UI *)0);
    SM_UI_disp_resizeKeybar_(&ao->disp, parent, n);
    return 0;
}

static int SM_UI_menu_cb_(struct ncplane * const n) {
    SM_UI *ao = ncplane_userptr(n);
    DBC_REQUIRE(349, ao != (SM_UI *)0);
    struct ncplane *parent = ncplane_parent(n);
    SM_UI_disp_resizeMenu_(&ao->disp, parent);
    return 0;
}
