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

#include <pthread.h>
#include <semaphore.h>
#include "static_pool.h"

//============================================================================
//=== Lock key type.
typedef int SST_LockKey;

//============================================================================
//=== Task attributes: thread handle + event semaphore.
#define SST_PORT_TASK_ATTR \
    pthread_t thread;         \
    sem_t sem;                \
    SST_TaskPrio prio;

//============================================================================
//=== Additional operations (placed after type definitions in sst.h).
#define SST_PORT_TASK_OPER \
    void SST_Task_setPrio(SST_Task * const me, SST_TaskPrio prio); \
    void SST_onIdle(void); \

//============================================================================
//=== Critical section: ref-counted non-recursive mutex.
#define SST_PORT_CRIT_STAT
#define SST_PORT_CRIT_ENTRY()  enterCriticalSection_()
#define SST_PORT_CRIT_EXIT()   leaveCriticalSection_()

void enterCriticalSection_(void);
void leaveCriticalSection_(void);

//============================================================================
//=== Task pend: wake the receiving task's thread.
#define SST_PORT_TASK_PEND()  sem_post(&(me)->sem)

//============================================================================
//=== Task wait: suspend until an event is posted.
#define SST_PORT_TASK_WAIT(me_)  sem_wait(&(me_)->sem)

//============================================================================
enum SST_Signals {
    SST_TIMEOUT_SIG = 0,
    SST_TIMEOUT1_SIG = 1,
    SST_TIMEOUT2_SIG = 2,
    SST_TIMEOUT3_SIG = 3,

    SST_USER_SIG // can not be used
};

//============================================================================
//=== Tick rate control.
void SST_setTickRate(uint32_t ticksPerSec);

//============================================================================
//=== AO event pools: multi-size pools for AO-to-AO communication.
#define SST_EVT_POOL_NUM 3U

extern StaticPool SST_evtPools_[SST_EVT_POOL_NUM];
extern uint8_t SST_evtPoolsNum;

void SST_EvtPool_init(void *sto, PoolCtr poolSize, PoolCtr blockSize);
void *SST_Evt_new(PoolCtr blockSize);
void SST_Evt_gc(void *evt);

#define SST_NEW(evtType_) ((evtType_ *)SST_Evt_new(sizeof(evtType_)))
#define SST_GC(evt_)       SST_Evt_gc((void *)(evt_))

#endif // SST_PORT_H_
