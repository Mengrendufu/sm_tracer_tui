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
//=== Board Support Package: assertions, tick, pool init
#include <stdio.h>
#include <stdlib.h>
#include "sst.h"
#include "bsp.h"
#include "sm_assert.h"
#include "dbc_assert.h"
DBC_MODULE_NAME("bsp")

//============================================================================
//=== Tick rate + idle hook.
static uint32_t l_tickRateMs = 10U;

void SST_setTickRate(uint32_t ticksPerSec) {
    l_tickRateMs = ticksPerSec > 0U ? 1000U / ticksPerSec : l_tickRateMs;
}

void SST_onIdle(void) {
    struct timespec ts = {
        .tv_sec  = l_tickRateMs / 1000U,
        .tv_nsec = (l_tickRateMs % 1000U) * 1000000UL,
    };
    nanosleep(&ts, (struct timespec *)0);

    // ao tick ---------------------------------------------------------------
    SST_TimeEvt_tick();

    // thread tick -----------------------------------------------------------
}

//============================================================================
//=== SST lifecycle.
void SST_init(void) {
    static uint8_t smallPoolSto[16U * sizeof(SST_Evt)];
    SST_EvtPool_init(smallPoolSto, sizeof(smallPoolSto), sizeof(SST_Evt));
}

void SST_onStart(void) {
    SST_setTickRate(BSP_TICKS_PER_SEC);
}

//============================================================================
//=== DBC fault handler (SST assertion framework).
#ifndef DBC_DISABLE
void DBC_fault_handler(char const *module, int label) {
    fprintf(stderr, "DBC ASSERT: %s:%d\n", module, label);
    exit(EXIT_FAILURE);
}
#endif // DBC_DISABLE

//============================================================================
//=== SM_onAssert (sm_hsm assertion framework).
#ifndef SM_DBC_DISABLE
SM_NORETURN SM_onAssert(char const *module, int label) {
    DBC_fault_handler(module, label);
}
#endif // SM_DBC_DISABLE
