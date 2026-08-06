//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#ifndef SST_PORT_H_
#define SST_PORT_H_

//============================================================================
#include <stdint.h>
#include "platform_port.h"

//============================================================================
#define SST_EVT_POOL_NUM 3U
#include "sst_evt_pool.h"

//============================================================================
#define SST_MAX_TASK 8U

//============================================================================
//=== Task priority type.
typedef uint8_t SST_TaskPrio;

//............................................................................
#ifndef SST_LOG2
static inline uint_fast8_t SST_log2_(uint32_t const bitmask) {
    static uint8_t const log2LUT[16] = {
        0U, 1U, 2U, 2U, 3U, 3U, 3U, 3U,
        4U, 4U, 4U, 4U, 4U, 4U, 4U, 4U
    };

    uint_fast8_t n = 0U;
    uint32_t x = bitmask;
    uint32_t tmp;

#if (SST_MAX_TASK > 16U)
    tmp = (x >> 16U);
    if (tmp != 0U) {
        n += 16U;
        x = tmp;
    }
#endif // (SST_MAX_TASK > 16U)

#if (SST_MAX_TASK > 8U)
    tmp = (x >> 8U);
    if (tmp != 0U) {
        n += 8U;
        x = tmp;
    }
#endif // (SST_MAX_TASK > 8U)

    tmp = (x >> 4U);
    if (tmp != 0U) {
        n += 4U;
        x = tmp;
    }

    return (uint_fast8_t)(n + log2LUT[x]);
}

#define SST_LOG2(x_) SST_log2_((uint32_t)(x_))
#endif // ndef SST_LOG2

//============================================================================
//=== Lock key type.
typedef int SST_LockKey;

//============================================================================
//=== Task attributes: thread handle + event semaphore.
#define SST_PORT_TASK_ATTR \
    PlatformThread thread; \
    PlatformSemaphore sem;

//============================================================================
//=== Additional operations (placed after type definitions in sst.h).
#define SST_PORT_TASK_OPER \
    void SST_Task_setPrio(SST_Task * const me, SST_TaskPrio prio); \
    void SST_onIdle(void);

//============================================================================
//=== Critical section: non-recursive mutex with thread-local nesting guard.
#define SST_PORT_CRIT_STAT
#define SST_PORT_CRIT_ENTRY()  enterCriticalSection_()
#define SST_PORT_CRIT_EXIT()   leaveCriticalSection_()

void enterCriticalSection_(void);
void leaveCriticalSection_(void);

//============================================================================
//=== Task pend: wake the receiving task's thread.
#define SST_PORT_TASK_PEND()  PlatformSemaphore_post(&(me)->sem)

//============================================================================
//=== Task wait: suspend until an event is posted.
#define SST_PORT_TASK_WAIT(me_)  PlatformSemaphore_wait(&(me_)->sem)

//============================================================================
//=== Tick rate control.
void SST_setTickRate(uint32_t ticksPerSec);

#endif // SST_PORT_H_
