//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#ifndef UI_CMD_HSM_H_
#define UI_CMD_HSM_H_

#include "sm_hsm.h"

//============================================================================
//=== Command HSM — event-driven command line parser

typedef struct {
    SM_Hsm super;
    char   buf[128];
    uint8_t len;
    bool   active;
} UI_CmdHsm;

void UI_CmdHsm_ctor(UI_CmdHsm *me);
void UI_CmdHsm_init(UI_CmdHsm *me);

// query
bool          UI_CmdHsm_isActive(UI_CmdHsm const *me);
char const   *UI_CmdHsm_buf(UI_CmdHsm const *me);
uint8_t       UI_CmdHsm_len(UI_CmdHsm const *me);

// exposed for state comparison
extern SM_HsmState SM_HSM_ROM Cmd_idle;

#endif // UI_CMD_HSM_H_
