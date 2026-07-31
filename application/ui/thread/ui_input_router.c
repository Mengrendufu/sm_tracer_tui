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
//=== UIInputRouter: notcurses input to UI signal classification
#include <notcurses/notcurses.h>
#include "dbc_assert.h"
#include "ui_input_router_priv.h"
DBC_MODULE_NAME("ui_input_router")

UI_Signal UI_InputRouter_route(UI_Input const * const input) {
    DBC_REQUIRE(100, input != (UI_Input const *)0);

    uint32_t const id = input->id;
    uint32_t const modifiers = input->modifiers;

    if (id == 27U) {
        return UI_KEY_ESC_SIG;
    }
    if (id == 0x1FU
        || (id == '/'
            && (modifiers & NCKEY_MOD_CTRL) != 0U))
    {
        return UI_KEY_CTRL_SLASH_SIG;
    }
    if (id == NCKEY_UP) {
        return UI_KEY_UP_SIG;
    }
    if (id == NCKEY_DOWN) {
        return UI_KEY_DOWN_SIG;
    }
    if (id == NCKEY_LEFT) {
        return UI_KEY_LEFT_SIG;
    }
    if (id == NCKEY_RIGHT) {
        return UI_KEY_RIGHT_SIG;
    }
    if (id == NCKEY_BACKSPACE) {
        return UI_KEY_BACKSPACE_SIG;
    }
    if (id == 0x15U
        || ((id == 'u' || id == 'U')
            && (modifiers & NCKEY_MOD_CTRL) != 0U))
    {
        return UI_KEY_CTRL_U_SIG;
    }
    if (id == 0x17U
        || ((id == 'w' || id == 'W')
            && (modifiers & NCKEY_MOD_CTRL) != 0U))
    {
        return UI_KEY_CTRL_W_SIG;
    }
    if (id == NCKEY_HOME) {
        return UI_KEY_HOME_SIG;
    }
    if (id == NCKEY_END) {
        return UI_KEY_END_SIG;
    }
    if (id == NCKEY_ENTER) {
        return UI_KEY_ENTER_SIG;
    }
    if (id == 'j' && modifiers == 0U) {
        return UI_KEY_J_SIG;
    }
    if (id == 'k' && modifiers == 0U) {
        return UI_KEY_K_SIG;
    }
    if (id == 0x0EU
        || ((id == 'n' || id == 'N')
            && (modifiers & NCKEY_MOD_CTRL) != 0U))
    {
        return UI_KEY_CTRL_N_SIG;
    }
    if (id == 0x10U
        || ((id == 'p' || id == 'P')
            && (modifiers & NCKEY_MOD_CTRL) != 0U))
    {
        return UI_KEY_CTRL_P_SIG;
    }
    if (id == NCKEY_PGUP) {
        return UI_KEY_PGUP_SIG;
    }
    if (id == NCKEY_PGDOWN) {
        return UI_KEY_PGDN_SIG;
    }
    if (id == NCKEY_RESIZE) {
        return UI_RESIZE_SIG;
    }
    return UI_INPUT_SIG;
}
