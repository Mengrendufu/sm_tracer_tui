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
//=== Application AO registry
#include "sst.h"
#include "dbc_assert.h"
#include "aos.h"
#include "blinky/blinky.h"
#include "sp_mngr/sp_mngr.h"

//============================================================================
// DBC_MODULE_NAME("aos")

//============================================================================
//=== AO startup — construct + start all Active Objects

void SST_start(void) {
    Blinky_ctor();
    static SST_Evt const * Blinky_qSto[16];
    SST_Task_start(AO_Blinky,
                   1U,                    // priority
                   Blinky_qSto, ARRAY_NELEM(Blinky_qSto),
                   (SST_Evt const *)0);  // init event

    SpMngr_ctor();
    static SST_Evt const * SpMngr_qSto[128];
    SST_Task_start(AO_SpMngr,
                   2U,                    // priority
                   SpMngr_qSto, ARRAY_NELEM(SpMngr_qSto),
                   (SST_Evt const *)0);  // init event
}
