//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#ifndef UI_INPUT_ROUTER_PRIV_H_
#define UI_INPUT_ROUTER_PRIV_H_

#include "hsm/sm_ui_evt.h"

//============================================================================
//=== notcurses input classification -- private to the UI thread

UI_Signal UI_InputRouter_route(UI_Input const *input);

#endif // UI_INPUT_ROUTER_PRIV_H_
