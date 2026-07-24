//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#ifndef SM_UI_H_
#define SM_UI_H_

#include "sm_ui_evt.h"

struct notcurses;

//============================================================================
//=== Host capability required by SM_UI
//
// SM_UI owns this required-port contract, not its implementation.
// The host runtime supplies requestQuit(), requestFrame(), and ctx to
// SM_UI_setup().
// SM_UI copies the descriptor and invokes it without importing the host.
// The ctx pointee remains host-owned and must outlive SM_UI use.
typedef struct {
    void (*requestQuit)(void *ctx);
    void (*requestFrame)(void *ctx);
    void *ctx;
} SM_UI_HostOps;

void SM_UI_setup(struct notcurses *nc,
                 SM_UI_HostOps const *hostOps);
void SM_UI_flush(void);
void SM_UI_dispatchEvt(UI_Evt const *e);

#endif // SM_UI_H_
