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
#include "sst_pubsub.h"
#include "app_sig.h"
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

    // extension tick --------------------------------------------------------
    BSP_onTick();
}

//============================================================================
//=== Tick handler extension (for UI module etc.)

static BSP_TickHandler l_tickHandlers_[BSP_MAX_TICK_HANDLERS_];
static uint8_t l_tickHandlerNum_;

void BSP_registerTickHandler(BSP_TickHandler handler) {
    DBC_REQUIRE(400, handler != (BSP_TickHandler)0);
    DBC_REQUIRE(401, l_tickHandlerNum_ < BSP_MAX_TICK_HANDLERS_);
    l_tickHandlers_[l_tickHandlerNum_++] = handler;
}

void BSP_onTick(void) {
    for (uint8_t i = 0U; i < l_tickHandlerNum_; ++i) {
        (*l_tickHandlers_[i])();
    }
}

//============================================================================
//=== SST lifecycle.
void SST_init(void) {
    static SST_SubscrList subscrSto[MAX_PUB_SIG];
    SST_PubSub_init(subscrSto, ARRAY_NELEM(subscrSto));

#if (SST_EVT_POOL_NUM > 0U)
    static uint8_t smallPoolSto[16U * sizeof(SST_Evt)];
    SST_EvtPool_init(smallPoolSto, sizeof(smallPoolSto), sizeof(SST_Evt));
#endif
}

void SST_onStart(void) {
    SST_setTickRate(BSP_TICKS_PER_SEC);
}

//============================================================================
//=== DBC fault handler (SST assertion framework).
#ifndef DBC_DISABLE
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
void DBC_fault_handler(char const *module, int label) {
    char buf[128];
    int len = snprintf(buf, sizeof(buf), "\nDBC ASSERT [%s:%d]\n",
                       module, label);
    // write directly to controlling tty (bypasses notcurses alternate screen)
    int tty = open("/dev/tty", O_WRONLY);
    if (tty >= 0) {
        ssize_t wr = write(tty, buf, (size_t)len);
        (void)wr;
        close(tty);
    }
    abort();
}
#endif // DBC_DISABLE

//============================================================================
//=== SM_onAssert (sm_hsm assertion framework).
#ifndef SM_DBC_DISABLE
SM_NORETURN SM_onAssert(char const *module, int label) {
    DBC_fault_handler(module, label);
}
#endif // SM_DBC_DISABLE
