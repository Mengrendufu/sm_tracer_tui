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
//=== Command HSM — event-driven line parser
#include <string.h>
#include "sm_port.h"
#include "sm_hsm.h"
#include "dbc_assert.h"
#include "ui.h"
#include "ui_cmd_hsm.h"
DBC_MODULE_NAME("ui_cmd_hsm")

//============================================================================
//=== States

static SM_StatePtr Cmd_TOP_initial(SM_Hsm *me) SM_HSM_RETT;

static void        Cmd_idle_entry_(SM_Hsm *me) SM_HSM_RETT;
static SM_RetState Cmd_idle_(SM_Hsm *me, void const *e) SM_HSM_RETT;
SM_HsmState SM_HSM_ROM Cmd_idle = {
    (SM_StatePtr)0,                     // super (top)
    (SM_InitHandler)0,                  // init_ (leaf)
    (SM_ActionHandler)&Cmd_idle_entry_, // entry_
    (SM_ActionHandler)0,                // exit_
    (SM_StateHandler)&Cmd_idle_         // handler_
};

static void        Cmd_gather_entry_(SM_Hsm *me) SM_HSM_RETT;
static SM_RetState Cmd_gather_(SM_Hsm *me, void const *e) SM_HSM_RETT;
SM_HsmState SM_HSM_ROM Cmd_gather = {
    (SM_StatePtr)0,                      // super (top)
    (SM_InitHandler)0,                   // init_ (leaf)
    (SM_ActionHandler)&Cmd_gather_entry_,// entry_
    (SM_ActionHandler)0,                 // exit_
    (SM_StateHandler)&Cmd_gather_        // handler_
};

//============================================================================
//=== Helper — post command result to UI queue

static void Cmd_postResult_(UI_Signal sig) {
    UI_postSignal(sig);
}

//============================================================================
//=== HSM implementations

static SM_StatePtr Cmd_TOP_initial(SM_Hsm * const me) SM_HSM_RETT {
    (void)me;
    return _SM_INIT(&Cmd_idle);
}

// idle — waiting for : to activate
static void Cmd_idle_entry_(SM_Hsm * const me) SM_HSM_RETT {
    (void)me;
    UI_postSignal(UI_CMD_IDLE_SIG);
}

static SM_RetState Cmd_idle_(SM_Hsm * const me, void const * const e) {
    (void)me;
    UI_Evt const *ue = (UI_Evt const *)e;
    if (ue->sig == UI_KEY_SIG && ue->pld.key == (uint32_t)':') {
        return _SM_TRAN(&Cmd_gather);
    }
    return _SM_SUPER();
}

// gathering — collect characters until Enter
static void Cmd_gather_entry_(SM_Hsm * const me) SM_HSM_RETT {
    UI_CmdHsm *cmd = containerof(me, UI_CmdHsm, super);
    cmd->len    = 0U;
    cmd->buf[0] = '\0';
    cmd->active = true;
    UI_postSignal(UI_CMD_ACTIVE_SIG);
}

static void Cmd_parseAndPost_(UI_CmdHsm *cmd) {
    char const *s = cmd->buf;
    if (cmd->buf[0] == '/') {
        ++s;
    }
    if (strcmp(s, "quit") == 0 || strcmp(s, "q") == 0) {
        Cmd_postResult_(UI_CMD_QUIT_SIG);
    } else if (strcmp(s, "menu") == 0) {
        Cmd_postResult_(UI_CMD_MENU_SIG);
    } else if (strcmp(s, "connect") == 0) {
        Cmd_postResult_(UI_CMD_CONNECT_SIG);
    }
}

static SM_RetState Cmd_gather_(SM_Hsm * const me, void const * const e) {
    UI_CmdHsm *cmd = containerof(me, UI_CmdHsm, super);
    UI_Evt const *ue = (UI_Evt const *)e;

    if (ue->sig != UI_KEY_SIG) {
        return _SM_SUPER();
    }

    switch (ue->pld.key) {
    case NCKEY_ENTER:
        Cmd_parseAndPost_(cmd);
        return _SM_TRAN(&Cmd_idle);

    case NCKEY_ESC:
        return _SM_TRAN(&Cmd_idle);

    case NCKEY_BACKSPACE:
        if (cmd->len > 0U) {
            --cmd->len;
            cmd->buf[cmd->len] = '\0';
        }
        return _SM_HANDLED();

    default:
        if (ue->pld.key >= 0x20U && ue->pld.key <= 0x7EU
            && cmd->len + 1U < (uint8_t)sizeof(cmd->buf)) {
            cmd->buf[cmd->len] = (char)ue->pld.key;
            ++cmd->len;
            cmd->buf[cmd->len] = '\0';
        }
        return _SM_HANDLED();
    }
}

//============================================================================
//=== Query

bool UI_CmdHsm_isActive(UI_CmdHsm const * const me) {
    return me->active;
}

char const *UI_CmdHsm_buf(UI_CmdHsm const * const me) {
    return me->buf;
}

uint8_t UI_CmdHsm_len(UI_CmdHsm const * const me) {
    return me->len;
}

//============================================================================
//=== Constructor

void UI_CmdHsm_ctor(UI_CmdHsm * const me) {
    DBC_REQUIRE(100, me != (UI_CmdHsm *)0);
    me->len     = 0U;
    me->active  = false;
    me->buf[0]  = '\0';
}

void UI_CmdHsm_init(UI_CmdHsm * const me) {
    DBC_REQUIRE(200, me != (UI_CmdHsm *)0);
    SM_Hsm_init_(&me->super, (SM_InitHandler)Cmd_TOP_initial);
}
