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
//=== UI HSM -- state and widget orchestration
#include <notcurses/notcurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sst.h"
#include "sst_evt_pool.h"
#include "app_sig.h"
#include "aos/sp_mngr/sp_mngr.h"
#include "sm_port.h"
#include "sm_hsm.h"
#include "dbc_assert.h"
#include "sm_input_cmps_mngr.h"
#include "widgets/title_bar.h"
#include "widgets/connection_status_bar.h"
#include "widgets/text_buffer_view.h"
#include "widgets/keybar.h"
#include "widgets/menu.h"
#include "sm_ui.h"
DBC_MODULE_NAME("ui_hsm")

//============================================================================
//=== Module-owned SM_UI instance and internal component graph.

typedef struct {
    SM_Hsm super;
    VC_Handler init;
    VC_Handler dispatch;
    struct notcurses *nc;
    struct TitleBar title;
    struct ConnectionStatusBar status;
    SpMngrConfig serialConfig;
    char activeProtocolPath[SPMNGR_VALUE_LEN];
    char *availablePortNames;
    size_t availablePortNamesSize;
    char *availableProtocolPaths;
    size_t availableProtocolPathsSize;
    struct TextBufferView mainBuffer;
    struct Keybar keybar;
    struct Menu menu;
    SM_InputCmpsMngr inputManager;
    unsigned inputRows;
} SM_UI;

static SM_UI SM_UI_inst_;
static SM_UI_HostOps SM_UI_hostOps_;

//============================================================================
//--- Private declarations ---

typedef struct {
    unsigned rows;
    unsigned cols;
} SM_UI_MainBufferMetrics_;

static unsigned    SM_UI_std_panelCols_(unsigned cols);
static int         SM_UI_std_inputY_(unsigned rows,
                                     unsigned inputRows);
static int         SM_UI_std_keybarY_(unsigned rows);
static unsigned    SM_UI_std_inputRows_(unsigned rows,
                                        unsigned desiredRows);

// Interaction handler declarations:
// - coordinates multiple components or adapts external callback context.
// - owns the explicit coupling between leaf component IO helpers.
// - keeps shared layout policy in SM_UI rather than a leaf widget.
static uint64_t    SM_UI_std_borderCh_(void);
static SM_UI_MainBufferMetrics_ SM_UI_std_mainBufferMetrics_(
                        unsigned rows,
                        unsigned cols,
                        unsigned inputRows);
static void        SM_UI_syncInputLayout_(SM_UI *me);
static void        SM_UI_widgets_create_(SM_UI *me);
static void        SM_UI_mainBuffer_pushText_(SM_UI *me,
                                        char const *text, size_t len);
static void        SM_UI_updateAvailablePorts_(
                        SM_UI *me,
                        UI_PortListEvt const *ports);
static void        SM_UI_updateAvailableProtocols_(
                        SM_UI *me,
                        UI_ProtocolListEvt const *protocols);
static void        SM_UI_protocolLoaded_(
                        SM_UI *me,
                        UI_AppEvt const *loaded);
static void        SM_UI_mainBuffer_clear_(SM_UI *me);
static void        SM_UI_mainBuffer_scrollPageUp_(SM_UI *me);
static void        SM_UI_mainBuffer_scrollPageDown_(SM_UI *me);
static void        SM_UI_menu_show_(SM_UI *me);
static void        SM_UI_menu_hide_(SM_UI *me);
static void        SM_UI_menu_sync_(SM_UI *me);
static void        SM_UI_menu_selectNext_(SM_UI *me);
static void        SM_UI_menu_selectPrev_(SM_UI *me);
static void        SM_UI_requestFrame_(void);
static void        SM_UI_submitCommand_(
                        void *ctx,
                        SM_InputCmpsMngrSubmission const *submission);
static void        SM_UI_rejectCommand_(void *ctx, char const *reason);

// Notcurses callback adapters:
// - adapt raw ncplane callback context back into SM_UI.
// - keep shared layout policy outside leaf widgets.
static int         SM_UI_title_cb_(struct ncplane *n);
static int         SM_UI_status_cb_(struct ncplane *n);
static int         SM_UI_main_cb_(struct ncplane *n);
static int         SM_UI_content_cb_(struct ncplane *n);
static int         SM_UI_input_cb_(struct ncplane *n);
static int         SM_UI_keybar_cb_(struct ncplane *n);
static int         SM_UI_menu_cb_(struct ncplane *n);

//--- shared layout constants ---
#define SM_UI_MAIN_Y_  7U
#define SM_UI_MAIN_MIN_ROWS_ 3U
#define SM_UI_MAIN_MIN_COLS_ 4U
#define SM_UI_SIDE_MARGIN_COLS_ 4U

//============================================================================
//=== HSM states

// TOP-INIT
static SM_StatePtr SM_UI_TOP_initial(SM_Hsm * const me) SM_HSM_RETT;

// active
static SM_StatePtr SM_UI_active_init_(SM_Hsm * const me) SM_HSM_RETT;
static void        SM_UI_active_entry_(SM_Hsm * const me) SM_HSM_RETT;
static SM_RetState SM_UI_active_(SM_Hsm * const me, UI_Evt const * const e) SM_HSM_RETT;

SM_HsmState SM_HSM_ROM SM_UI_active = {
    SM_HSM_TOP,                                  // super
    (SM_InitHandler)&SM_UI_active_init_,         // init_
    (SM_ActionHandler)&SM_UI_active_entry_,      // entry_
    (SM_ActionHandler)0,                         // exit_
    (SM_StateHandler)&SM_UI_active_              // handler
};

// showMain
static void        SM_UI_showMain_entry_(SM_Hsm * const me) SM_HSM_RETT;
static SM_RetState SM_UI_showMain_(SM_Hsm * const me, UI_Evt const * const e) SM_HSM_RETT;

SM_HsmState SM_HSM_ROM SM_UI_showMain = {
    (SM_StatePtr)&SM_UI_active,                  // super
    (SM_InitHandler)0,                           // init_
    (SM_ActionHandler)&SM_UI_showMain_entry_,    // entry_
    (SM_ActionHandler)0,                         // exit_
    (SM_StateHandler)&SM_UI_showMain_            // handler
};

// showMenu
static void        SM_UI_showMenu_entry_(SM_Hsm * const me) SM_HSM_RETT;
static void        SM_UI_showMenu_exit_(SM_Hsm * const me) SM_HSM_RETT;
static SM_RetState SM_UI_showMenu_(SM_Hsm * const me, UI_Evt const * const e) SM_HSM_RETT;
SM_HsmState SM_HSM_ROM SM_UI_showMenu = {
    (SM_StatePtr)&SM_UI_active,                  // super
    (SM_InitHandler)0,                           // init_
    (SM_ActionHandler)&SM_UI_showMenu_entry_,    // entry_
    (SM_ActionHandler)&SM_UI_showMenu_exit_,     // exit_
    (SM_StateHandler)&SM_UI_showMenu_            // handler
};

//============================================================================
//=== HSM implementations

static SM_StatePtr SM_UI_TOP_initial(SM_Hsm * const me) SM_HSM_RETT {
    SM_UI *ao = containerof(me, SM_UI, super);
    SM_UI_widgets_create_(ao);

    SM_InputCmpsMngr_init(&ao->inputManager);

    return _SM_INIT(&SM_UI_active);
}

static SM_StatePtr SM_UI_active_init_(SM_Hsm * const me) SM_HSM_RETT {
    (void)me;
    return _SM_INIT(&SM_UI_showMain);
}

static void SM_UI_active_entry_(SM_Hsm * const me) SM_HSM_RETT {
    (void)me;
}

static SM_RetState SM_UI_active_(
    SM_Hsm * const me, UI_Evt const * const e) SM_HSM_RETT
{
    SM_UI *ao = containerof(me, SM_UI, super);

    switch (e->sig) {
        //--------------------------------------------------------------------
        //--- subsystem events
        case UI_TIMER_SIG: {
            return _SM_HANDLED();
        }

        case UI_RESIZE_SIG: {
            SM_UI_menu_sync_(ao);
            SM_UI_requestFrame_();
            return _SM_HANDLED();
        }

        case UI_TEXT_SIG: {
            UI_AppEvt const *ae = (UI_AppEvt const *)e;
            SM_UI_mainBuffer_pushText_(ao, ae->pld.msg.text,
                                       ae->pld.msg.len);
            return _SM_HANDLED();
        }

        case UI_REFRESHED_PORTS_SIG: {
            SM_UI_updateAvailablePorts_(
                ao, (UI_PortListEvt const *)e);
            return _SM_HANDLED();
        }

        case UI_REFRESHED_PROTOCOLS_SIG: {
            SM_UI_updateAvailableProtocols_(
                ao, (UI_ProtocolListEvt const *)e);
            return _SM_HANDLED();
        }

        case UI_PROTOCOL_LOADED_SIG: {
            SM_UI_protocolLoaded_(ao, (UI_AppEvt const *)e);
            return _SM_HANDLED();
        }

        case UI_CONNECTION_STATUS_SIG: {
            UI_ConnectionEvt const * const connection =
                (UI_ConnectionEvt const *)e;
            ConnectionStatusBar_State state;
            if (connection->status == UI_CONNECTION_CONNECTING) {
                state = CONNECTION_STATUS_BAR_CONNECTING;
            } else if (connection->status == UI_CONNECTION_CONNECTED) {
                state = CONNECTION_STATUS_BAR_CONNECTED;
            } else if (connection->status
                       == UI_CONNECTION_DISCONNECTING)
            {
                state = CONNECTION_STATUS_BAR_DISCONNECTING;
            } else {
                DBC_ASSERT(615, connection->status
                                == UI_CONNECTION_DISCONNECTED);
                state = CONNECTION_STATUS_BAR_DISCONNECTED;
            }
            ConnectionStatusBar_setState(&ao->status, state);
            SM_UI_requestFrame_();
            return _SM_HANDLED();
        }

        default: {
            return _SM_SUPER();
        }
    }
}

static void SM_UI_showMain_entry_(SM_Hsm * const me) SM_HSM_RETT {
    SM_UI *ao = containerof(me, SM_UI, super);
    SM_InputCmpsMngr_setActive(&ao->inputManager, true);
    SM_UI_requestFrame_();
}

static SM_RetState SM_UI_showMain_(SM_Hsm * const me,
                                   UI_Evt const * const e) SM_HSM_RETT
{
    SM_UI *ao = containerof(me, SM_UI, super);

    switch (e->sig) {
        case UI_KEY_CTRL_SLASH_SIG: {
            return _SM_TRAN(&SM_UI_showMenu);
        }

        case UI_KEY_PGUP_SIG: {
            SM_UI_mainBuffer_scrollPageUp_(ao);
            return _SM_HANDLED();
        }

        case UI_KEY_PGDN_SIG: {
            SM_UI_mainBuffer_scrollPageDown_(ao);
            return _SM_HANDLED();
        }

        case UI_KEY_ESC_SIG:
        case UI_KEY_UP_SIG:
        case UI_KEY_DOWN_SIG:
        case UI_KEY_LEFT_SIG:
        case UI_KEY_RIGHT_SIG:
        case UI_KEY_CTRL_LEFT_SIG:
        case UI_KEY_CTRL_RIGHT_SIG:
        case UI_KEY_BACKSPACE_SIG:
        case UI_KEY_CTRL_U_SIG:
        case UI_KEY_CTRL_W_SIG:
        case UI_KEY_HOME_SIG:
        case UI_KEY_END_SIG:
        case UI_KEY_TAB_SIG:
        case UI_KEY_ENTER_SIG:
        case UI_KEY_J_SIG:
        case UI_KEY_K_SIG:
        case UI_KEY_CTRL_N_SIG:
        case UI_KEY_CTRL_P_SIG:
        case UI_INPUT_SIG: {
            SM_InputCmpsMngr_dispatchEvt(
                &ao->inputManager,
                (UI_InputEvt const *)e);
            SM_UI_syncInputLayout_(ao);
            SM_UI_requestFrame_();
            return _SM_HANDLED();
        }

        default: {
            return _SM_SUPER();
        }
    }
}

//============================================================================
static void SM_UI_showMenu_entry_(SM_Hsm * const me) SM_HSM_RETT {
    SM_UI *ao = containerof(me, SM_UI, super);
    SM_InputCmpsMngr_setActive(&ao->inputManager, false);
    SM_UI_menu_show_(ao);
}

static void SM_UI_showMenu_exit_(SM_Hsm * const me) SM_HSM_RETT {
    SM_UI *ao = containerof(me, SM_UI, super);
    SM_UI_menu_hide_(ao);
}

static SM_RetState SM_UI_showMenu_(SM_Hsm * const me,
                                   UI_Evt const * const e) SM_HSM_RETT
{
    SM_UI *ao = containerof(me, SM_UI, super);

    switch (e->sig) {
        case UI_KEY_DOWN_SIG:
        case UI_KEY_CTRL_N_SIG:
        case UI_KEY_J_SIG: {
            SM_UI_menu_selectNext_(ao);
            return _SM_HANDLED();
        }

        case UI_KEY_UP_SIG:
        case UI_KEY_CTRL_P_SIG:
        case UI_KEY_K_SIG: {
            SM_UI_menu_selectPrev_(ao);
            return _SM_HANDLED();
        }

        case UI_KEY_ENTER_SIG: {
            MenuAction const action = Menu_action(&ao->menu);

            if (action == MENU_ACT_RESUME) {
                SM_UI_requestFrame_();
                return _SM_TRAN(&SM_UI_showMain);
            } else if (action == MENU_ACT_CLEAR) {
                SM_UI_mainBuffer_clear_(ao);
                return _SM_TRAN(&SM_UI_showMain);
            } else if (action == MENU_ACT_ABOUT) {
                char const about[] =
                    "sm_tracer_tui v0.0.1\n"
                    "notcurses + sm_sst + sm_hsm\n";
                SM_UI_mainBuffer_pushText_(ao, about,
                                          sizeof(about) - 1U);
                return _SM_TRAN(&SM_UI_showMain);
            } else if (action == MENU_ACT_QUIT) {
                (*SM_UI_hostOps_.requestQuit)(SM_UI_hostOps_.ctx);
                return _SM_TRAN(&SM_UI_showMain);
            } else {
                return _SM_HANDLED();
            }
        }

        case UI_KEY_CTRL_SLASH_SIG:
        case UI_KEY_ESC_SIG: {
            return _SM_TRAN(&SM_UI_showMain);
        }

        case UI_INPUT_SIG:
        case UI_KEY_PGUP_SIG:
        case UI_KEY_PGDN_SIG: {
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
//=== Constructor / lifecycle

static void SM_UI_ctor_(SM_UI * const me) {
    DBC_REQUIRE(100, me != (SM_UI *)0);

    me->init     = (VC_Handler)SM_UI_init;
    me->dispatch = (VC_Handler)SM_UI_dispatch;

    me->nc = (struct notcurses *)0;
    TitleBar_init(&me->title);
    ConnectionStatusBar_init(&me->status);
    SpMngrConfig const defaultConfig = {
        .port = "none",
        .baudrate = "115200",
        .dataBits = "8",
        .stopBits = "1",
        .parity = "none",
        .flowControl = "none",
    };
    me->serialConfig = defaultConfig;
    (void)snprintf(me->activeProtocolPath,
                   sizeof(me->activeProtocolPath), "%s", "none");
    me->availablePortNames = (char *)0;
    me->availablePortNamesSize = 0U;
    me->availableProtocolPaths = (char *)0;
    me->availableProtocolPathsSize = 0U;
    TextBufferView_init(&me->mainBuffer);
    SM_InputCmpsMngr_ctor(&me->inputManager);
    SM_InputCmpsMngr_CommandSink const commandSink = {
        .submit = &SM_UI_submitCommand_,
        .reject = &SM_UI_rejectCommand_,
        .ctx = me,
    };
    SM_InputCmpsMngr_setCommandSink(&me->inputManager, &commandSink);
    Keybar_init(&me->keybar);
    Menu_init(&me->menu);
    me->inputRows = 1U;
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
    SM_UI_inst_.nc = nc;
    SM_UI_start_(&SM_UI_inst_);
}

void SM_UI_teardown(void) {
    DBC_REQUIRE(510, SM_UI_inst_.nc != (struct notcurses *)0);

    SM_InputCmpsMngr_destroy(&SM_UI_inst_.inputManager);
    free(SM_UI_inst_.availablePortNames);
    SM_UI_inst_.availablePortNames = (char *)0;
    SM_UI_inst_.availablePortNamesSize = 0U;
    free(SM_UI_inst_.availableProtocolPaths);
    SM_UI_inst_.availableProtocolPaths = (char *)0;
    SM_UI_inst_.availableProtocolPathsSize = 0U;
    SM_UI_inst_.nc = (struct notcurses *)0;
}

void SM_UI_flush(void) {
    TextBufferView_refresh(&SM_UI_inst_.mainBuffer);
}

void SM_UI_dispatchEvt(UI_Evt const * const e) {
    DBC_REQUIRE(505, e != (UI_Evt const *)0);
    DBC_REQUIRE(506, SM_UI_inst_.dispatch != (VC_Handler)0);

    (*SM_UI_inst_.dispatch)(&SM_UI_inst_, e);
}

//============================================================================
//=== Interaction Handlers
//
// This region is the explicit coupling layer between component APIs.
// Handlers coordinate components or adapt raw callback context into SM_UI.

//----------------------------------------------------------------------------
//--- Shared layout policy

static unsigned SM_UI_std_panelCols_(unsigned const cols) {
    return (cols > SM_UI_SIDE_MARGIN_COLS_)
           ? (cols - SM_UI_SIDE_MARGIN_COLS_) : 1U;
}

static uint64_t SM_UI_std_borderCh_(void) {
    return NCCHANNELS_INITIALIZER(60, 60, 120, 15, 15, 35);
}

static SM_UI_MainBufferMetrics_ SM_UI_std_mainBufferMetrics_(
    unsigned const rows,
    unsigned const cols,
    unsigned const inputRows)
{
    unsigned mainRows = SM_UI_MAIN_MIN_ROWS_;
    unsigned const bottomRows = inputRows + 1U;
    if (rows > (SM_UI_MAIN_Y_ + bottomRows)) {
        mainRows = rows - SM_UI_MAIN_Y_ - bottomRows;
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

static int SM_UI_std_keybarY_(unsigned const rows) {
    return rows > 0U ? (int)(rows - 1U) : 0;
}

static int SM_UI_std_inputY_(unsigned const rows,
                             unsigned const inputRows)
{
    return SM_UI_std_keybarY_(rows) - (int)inputRows;
}

static unsigned SM_UI_std_inputRows_(unsigned const rows,
                                     unsigned const desiredRows)
{
    unsigned maxRows = 1U;
    unsigned const fixedRows = SM_UI_MAIN_Y_
                             + SM_UI_MAIN_MIN_ROWS_ + 1U;
    if (rows > fixedRows) {
        maxRows = rows - fixedRows;
    }
    return desiredRows < maxRows ? desiredRows : maxRows;
}

//----------------------------------------------------------------------------
//--- Widget composition

static void SM_UI_widgets_create_(SM_UI * const me) {
    DBC_REQUIRE(600, me != (SM_UI *)0);
    DBC_REQUIRE(601, me->nc != (struct notcurses *)0);

    struct ncplane * const std = notcurses_stdplane(me->nc);
    DBC_REQUIRE(602, std != (struct ncplane *)0);

    unsigned rows;
    unsigned cols;
    ncplane_dim_yx(std, &rows, &cols);

    unsigned const panelCols = SM_UI_std_panelCols_(cols);
    me->inputRows = 1U;
    SM_UI_MainBufferMetrics_ const main =
        SM_UI_std_mainBufferMetrics_(rows, cols, me->inputRows);
    uint64_t const borderCh = SM_UI_std_borderCh_();

    ncplane_ascii_box(std, 0, borderCh, rows, cols, 0);
    TitleBar_create(&me->title, std, me, panelCols, SM_UI_title_cb_);
    ConnectionStatusBar_create(&me->status, std, me, panelCols,
                               SM_UI_status_cb_);
    TextBufferView_create(&me->mainBuffer, std, me, SM_UI_MAIN_Y_,
                          main.rows, main.cols, borderCh,
                          SM_UI_main_cb_, SM_UI_content_cb_);
    SM_InputCmpsMngr_create(&me->inputManager, std, me,
                            SM_UI_std_inputY_(rows, me->inputRows), panelCols,
                            SM_UI_input_cb_);
    Keybar_create(&me->keybar, std, me, SM_UI_std_keybarY_(rows),
                  panelCols, SM_UI_keybar_cb_);
    Menu_create(&me->menu, std, me, SM_UI_menu_cb_);
    SM_UI_requestFrame_();
}

static void SM_UI_syncInputLayout_(SM_UI * const me) {
    DBC_REQUIRE(603, me != (SM_UI *)0);
    struct ncplane * const std = notcurses_stdplane(me->nc);
    DBC_REQUIRE(604, std != (struct ncplane *)0);

    unsigned rows;
    unsigned cols;
    ncplane_dim_yx(std, &rows, &cols);
    unsigned const panelCols = SM_UI_std_panelCols_(cols);
    unsigned const desiredRows = SM_InputCmpsMngr_preferredRows(
        &me->inputManager, panelCols);
    unsigned const inputRows = SM_UI_std_inputRows_(rows, desiredRows);
    if (inputRows == me->inputRows) {
        return;
    }

    me->inputRows = inputRows;
    SM_UI_MainBufferMetrics_ const main =
        SM_UI_std_mainBufferMetrics_(rows, cols, inputRows);
    TextBufferView_resizeFrame(&me->mainBuffer, main.rows, main.cols,
                               SM_UI_std_borderCh_());
    TextBufferView_resizeContent(&me->mainBuffer);
    SM_InputCmpsMngr_resize(&me->inputManager,
                            SM_UI_std_inputY_(rows, inputRows),
                            inputRows, panelCols);
}

static void SM_UI_mainBuffer_pushText_(SM_UI * const me,
                                       char const * const text,
                                       size_t const len)
{
    DBC_REQUIRE(610, me != (SM_UI *)0);
    TextBufferView_pushText(&me->mainBuffer, text, len);
    SM_UI_requestFrame_();
}

static void SM_UI_updateAvailablePorts_(
    SM_UI * const me,
    UI_PortListEvt const * const ports)
{
    DBC_REQUIRE(614, me != (SM_UI *)0);
    DBC_REQUIRE(615, ports != (UI_PortListEvt const *)0);
    DBC_REQUIRE(616, ports->portNames != (char *)0);
    DBC_REQUIRE(617, ports->portNamesSize >= 2U);

    char * const copy = (char *)malloc(ports->portNamesSize);
    DBC_ASSERT(618, copy != (char *)0);
    memcpy(copy, ports->portNames, ports->portNamesSize);

    char * const oldPortNames = me->availablePortNames;
    me->availablePortNames = copy;
    me->availablePortNamesSize = ports->portNamesSize;
    SM_InputCmpsMngr_setPortCatalog(
        &me->inputManager, copy, ports->portNamesSize);
    free(oldPortNames);

    if (copy[0] == '\0') {
        char const empty[] = "[SYS_INFO]> Serial ports: none\n";
        SM_UI_mainBuffer_pushText_(me, empty, sizeof(empty) - 1U);
        return;
    }

    char const heading[] = "[SYS_INFO]> Serial ports:\n";
    SM_UI_mainBuffer_pushText_(me, heading, sizeof(heading) - 1U);

    size_t offset = 0U;
    while (copy[offset] != '\0') {
        size_t const remaining = me->availablePortNamesSize - offset;
        char const * const end =
            (char const *)memchr(&copy[offset], '\0', remaining);
        DBC_ASSERT(619, end != (char const *)0);
        size_t const nameSize = (size_t)(end - &copy[offset]);
        SM_UI_mainBuffer_pushText_(me, &copy[offset], nameSize);
        offset += nameSize + 1U;
    }
}

static void SM_UI_updateAvailableProtocols_(
    SM_UI * const me,
    UI_ProtocolListEvt const * const protocols)
{
    DBC_REQUIRE(625, me != (SM_UI *)0);
    DBC_REQUIRE(626, protocols != (UI_ProtocolListEvt const *)0);
    DBC_REQUIRE(627, protocols->protocolPaths != (char *)0);
    DBC_REQUIRE(628, protocols->protocolPathsSize >= 2U);

    char * const copy = (char *)malloc(protocols->protocolPathsSize);
    DBC_ASSERT(629, copy != (char *)0);
    memcpy(copy, protocols->protocolPaths,
           protocols->protocolPathsSize);

    char * const oldPaths = me->availableProtocolPaths;
    me->availableProtocolPaths = copy;
    me->availableProtocolPathsSize = protocols->protocolPathsSize;
    SM_InputCmpsMngr_setProtocolCatalog(
        &me->inputManager, copy, protocols->protocolPathsSize);
    free(oldPaths);

    if (copy[0] == '\0') {
        char const empty[] = "[SYS_INFO]> Protocol files: none\n";
        SM_UI_mainBuffer_pushText_(me, empty, sizeof(empty) - 1U);
        return;
    }

    char const heading[] = "[SYS_INFO]> Protocol files:\n";
    SM_UI_mainBuffer_pushText_(me, heading, sizeof(heading) - 1U);

    size_t offset = 0U;
    while (copy[offset] != '\0') {
        size_t const remaining =
            me->availableProtocolPathsSize - offset;
        char const * const end =
            (char const *)memchr(&copy[offset], '\0', remaining);
        DBC_ASSERT(634, end != (char const *)0);
        size_t const pathSize = (size_t)(end - &copy[offset]);
        SM_UI_mainBuffer_pushText_(me, &copy[offset], pathSize);
        offset += pathSize + 1U;
    }
}

static void SM_UI_protocolLoaded_(
    SM_UI * const me,
    UI_AppEvt const * const loaded)
{
    DBC_REQUIRE(635, me != (SM_UI *)0);
    DBC_REQUIRE(636, loaded != (UI_AppEvt const *)0);
    DBC_REQUIRE(637, loaded->pld.msg.text != (char *)0);
    DBC_REQUIRE(638, loaded->pld.msg.len < SPMNGR_VALUE_LEN);

    memcpy(me->activeProtocolPath, loaded->pld.msg.text,
           loaded->pld.msg.len);
    me->activeProtocolPath[loaded->pld.msg.len] = '\0';
    ConnectionStatusBar_setProtocol(
        &me->status, me->activeProtocolPath);
    SM_UI_requestFrame_();
}

static void SM_UI_mainBuffer_clear_(SM_UI * const me) {
    DBC_REQUIRE(611, me != (SM_UI *)0);
    TextBufferView_clear(&me->mainBuffer);
    SM_UI_requestFrame_();
}

static void SM_UI_mainBuffer_scrollPageUp_(SM_UI * const me) {
    DBC_REQUIRE(612, me != (SM_UI *)0);
    TextBufferView_scrollPageUp(&me->mainBuffer);
    SM_UI_requestFrame_();
}

static void SM_UI_mainBuffer_scrollPageDown_(SM_UI * const me) {
    DBC_REQUIRE(613, me != (SM_UI *)0);
    TextBufferView_scrollPageDown(&me->mainBuffer);
    SM_UI_requestFrame_();
}

static void SM_UI_menu_show_(SM_UI * const me) {
    DBC_REQUIRE(620, me != (SM_UI *)0);
    struct ncplane * const std = notcurses_stdplane(me->nc);
    Menu_show(&me->menu, std);
    Keybar_showMenuHints(&me->keybar);
    SM_UI_requestFrame_();
}

static void SM_UI_menu_hide_(SM_UI * const me) {
    DBC_REQUIRE(621, me != (SM_UI *)0);
    struct ncplane * const std = notcurses_stdplane(me->nc);
    Menu_hide(&me->menu, std);
    Keybar_showMainHints(&me->keybar);
    SM_UI_requestFrame_();
}

static void SM_UI_menu_sync_(SM_UI * const me) {
    DBC_REQUIRE(622, me != (SM_UI *)0);
    struct ncplane * const std = notcurses_stdplane(me->nc);
    Menu_syncLayout(&me->menu, std);
    if (Menu_isVisible(&me->menu)) {
        Keybar_showMenuHints(&me->keybar);
    } else {
        Keybar_showMainHints(&me->keybar);
    }
}

static void SM_UI_menu_selectNext_(SM_UI * const me) {
    DBC_REQUIRE(623, me != (SM_UI *)0);
    Menu_selectNext(&me->menu);
    SM_UI_requestFrame_();
}

static void SM_UI_menu_selectPrev_(SM_UI * const me) {
    DBC_REQUIRE(624, me != (SM_UI *)0);
    Menu_selectPrev(&me->menu);
    SM_UI_requestFrame_();
}

static void SM_UI_requestFrame_(void) {
    (*SM_UI_hostOps_.requestFrame)(SM_UI_hostOps_.ctx);
}

static void SM_UI_applyCommandOverride_(
    char * const dst,
    SM_InputCmpsMngrArg const * const override)
{
    if (!override->present) {
        return;
    }

    size_t const len = override->len;
    DBC_ASSERT(630, len < SPMNGR_VALUE_LEN);
    memcpy(dst, override->text, len);
    dst[len] = '\0';
}

static void SM_UI_submitCommand_(
    void * const ctx,
    SM_InputCmpsMngrSubmission const * const submission)
{
    DBC_REQUIRE(631, ctx != (void *)0);
    DBC_REQUIRE(632,
        submission != (SM_InputCmpsMngrSubmission const *)0);
    SM_UI * const me = (SM_UI *)ctx;

    if (submission->protocolPath.present) {
        SpMngrProtocolEvt * const load = SST_NEW(SpMngrProtocolEvt);
        load->super.sig = SPMNGR_LOAD_PROTOCOL_SIG;
        SM_UI_applyCommandOverride_(load->relativePath,
                                    &submission->protocolPath);
        SST_Task_post(AO_SpMngr, &load->super);
    }
    if (submission->action == SM_INPUT_ACTION_LOAD_PROTOCOL) {
        DBC_ASSERT(633, submission->protocolPath.present);
        return;
    }

    if ((submission->action == SM_INPUT_ACTION_CONFIG)
        || (submission->action == SM_INPUT_ACTION_CONNECT))
    {
        SM_UI_applyCommandOverride_(me->serialConfig.port,
                                    &submission->port);
        SM_UI_applyCommandOverride_(me->serialConfig.baudrate,
                                    &submission->baudrate);
        SM_UI_applyCommandOverride_(me->serialConfig.dataBits,
                                    &submission->dataBits);
        SM_UI_applyCommandOverride_(me->serialConfig.stopBits,
                                    &submission->stopBits);
        SM_UI_applyCommandOverride_(me->serialConfig.parity,
                                    &submission->parity);
        SM_UI_applyCommandOverride_(me->serialConfig.flowControl,
                                    &submission->flowControl);
        ConnectionStatusBar_setSerial(
            &me->status, me->serialConfig.port,
            me->serialConfig.baudrate, me->serialConfig.dataBits,
            me->serialConfig.stopBits, me->serialConfig.parity,
            me->serialConfig.flowControl);
        SpMngrConfigEvt * const command = SST_NEW(SpMngrConfigEvt);
        command->super.sig =
            (submission->action == SM_INPUT_ACTION_CONNECT)
            ? SPMNGR_PORT_CONNECT_SIG
            : SPMNGR_CONFIG_UPDATE_SIG;
        command->config = me->serialConfig;
        SST_Task_post(AO_SpMngr, &command->super);
    } else if (submission->action == SM_INPUT_ACTION_DISCONNECT) {
        static SST_Evt const disconnectEvt = {
            .sig = SPMNGR_PORT_DISCONNECT_SIG,
        };
        SST_Task_post(AO_SpMngr, &disconnectEvt);
    } else if (submission->action == SM_INPUT_ACTION_REFRESH) {
        static SST_Evt const refreshEvt = {
            .sig = SPMNGR_REFRESH_PORTS_SIG,
        };
        SST_Task_post(AO_SpMngr, &refreshEvt);
    } else if (submission->action
               == SM_INPUT_ACTION_REFRESH_PROTOCOLS)
    {
        static SST_Evt const refreshProtocolsEvt = {
            .sig = SPMNGR_REFRESH_PROTOCOLS_SIG,
        };
        SST_Task_post(AO_SpMngr, &refreshProtocolsEvt);
    } else {
        DBC_ERROR(639);
    }
}

static void SM_UI_rejectCommand_(void * const ctx,
                                 char const * const reason)
{
    DBC_REQUIRE(650, ctx != (void *)0);
    DBC_REQUIRE(651, reason != (char const *)0);

    SM_UI * const me = (SM_UI *)ctx;
    char message[160];
    int const n = snprintf(message, sizeof(message),
                           "[SYS_INFO]> Command rejected: %s.\n", reason);
    DBC_ASSERT(652, (n >= 0) && ((size_t)n < sizeof(message)));
    SM_UI_mainBuffer_pushText_(me, message, (size_t)n);
}

//----------------------------------------------------------------------------
//--- Notcurses callback adapters
//
// These handlers adapt raw notcurses callback context back into SM_UI's
// component composition and shared layout policy.

static int SM_UI_title_cb_(struct ncplane * const n) {
    struct ncplane *parent = ncplane_parent(n);
    SM_UI *ao = ncplane_userptr(n);
    DBC_REQUIRE(640, ao != (SM_UI *)0);
    unsigned cols;
    ncplane_dim_yx(parent, NULL, &cols);
    TitleBar_resize(&ao->title, SM_UI_std_panelCols_(cols));
    return 0;
}

static int SM_UI_status_cb_(struct ncplane * const n) {
    struct ncplane *parent = ncplane_parent(n);
    SM_UI *ao = ncplane_userptr(n);
    DBC_REQUIRE(641, ao != (SM_UI *)0);
    unsigned cols;
    ncplane_dim_yx(parent, NULL, &cols);
    ConnectionStatusBar_resize(&ao->status,
                               SM_UI_std_panelCols_(cols));
    return 0;
}

static int SM_UI_main_cb_(struct ncplane * const n) {
    struct ncplane *parent = ncplane_parent(n); // std
    SM_UI *ao = ncplane_userptr(n);
    DBC_REQUIRE(642, ao != (SM_UI *)0);
    unsigned rows;
    unsigned cols;
    ncplane_dim_yx(parent, &rows, &cols);
    SM_UI_MainBufferMetrics_ const main =
        SM_UI_std_mainBufferMetrics_(rows, cols, ao->inputRows);
    TextBufferView_resizeFrame(&ao->mainBuffer, main.rows, main.cols,
                               SM_UI_std_borderCh_());
    return 0;
}

static int SM_UI_content_cb_(struct ncplane * const n) {
    SM_UI *ao = ncplane_userptr(n);
    DBC_REQUIRE(643, ao != (SM_UI *)0);
    TextBufferView_resizeContent(&ao->mainBuffer);
    SM_UI_requestFrame_();
    return 0;
}

static int SM_UI_input_cb_(struct ncplane * const n) {
    struct ncplane *parent = ncplane_parent(n); // std
    SM_UI *ao = ncplane_userptr(n);
    DBC_REQUIRE(646, ao != (SM_UI *)0);
    unsigned rows;
    unsigned cols;
    ncplane_dim_yx(parent, &rows, &cols);
    unsigned const panelCols = SM_UI_std_panelCols_(cols);
    unsigned const desiredRows = SM_InputCmpsMngr_preferredRows(
        &ao->inputManager, panelCols);
    ao->inputRows = SM_UI_std_inputRows_(rows, desiredRows);
    SM_UI_MainBufferMetrics_ const main =
        SM_UI_std_mainBufferMetrics_(rows, cols, ao->inputRows);
    TextBufferView_resizeFrame(&ao->mainBuffer, main.rows, main.cols,
                               SM_UI_std_borderCh_());
    TextBufferView_resizeContent(&ao->mainBuffer);
    SM_InputCmpsMngr_resize(
        &ao->inputManager,
        SM_UI_std_inputY_(rows, ao->inputRows),
        ao->inputRows, panelCols);
    SM_UI_requestFrame_();
    return 0;
}

static int SM_UI_keybar_cb_(struct ncplane * const n) {
    struct ncplane *parent = ncplane_parent(n); // std
    SM_UI *ao = ncplane_userptr(n);
    DBC_REQUIRE(644, ao != (SM_UI *)0);
    unsigned rows;
    unsigned cols;
    ncplane_dim_yx(parent, &rows, &cols);
    Keybar_resize(&ao->keybar, SM_UI_std_keybarY_(rows),
                  SM_UI_std_panelCols_(cols));
    if (Menu_isVisible(&ao->menu)) {
        Keybar_showMenuHints(&ao->keybar);
    } else {
        Keybar_showMainHints(&ao->keybar);
    }
    return 0;
}

static int SM_UI_menu_cb_(struct ncplane * const n) {
    SM_UI *ao = ncplane_userptr(n);
    DBC_REQUIRE(645, ao != (SM_UI *)0);
    struct ncplane *parent = ncplane_parent(n);
    Menu_syncLayout(&ao->menu, parent);
    return 0;
}
