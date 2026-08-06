//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#include "sst.h"
#include "sst_priv.h"
#include "dbc_assert.h"

DBC_MODULE_NAME("sst_port")

//============================================================================
//=== Critical section: non-recursive mutex with thread-local nesting guard.
static PlatformMutex l_portLock = PLATFORM_MUTEX_INITIALIZER;

static PLATFORM_THREAD_LOCAL int l_critSectNest;

void enterCriticalSection_(void) {
    DBC_REQUIRE(100, l_critSectNest == 0);
    int const result = PlatformMutex_lock(&l_portLock);
    DBC_ASSERT(101, result == 0);
    (void)result;
    ++l_critSectNest;
}

void leaveCriticalSection_(void) {
    DBC_REQUIRE(200, l_critSectNest == 1);
    --l_critSectNest;
    int const result = PlatformMutex_unlock(&l_portLock);
    DBC_ASSERT(201, result == 0);
    (void)result;
}

//============================================================================
//=== AO thread: wait -> dequeue -> dispatch.
static void ao_thread(void *arg) {
    SST_Task *me = (SST_Task *)arg;

    for (;;) {
        SST_Evt const *e;

        SST_PORT_TASK_WAIT(me);

        SST_PORT_CRIT_ENTRY();

        e = me->qBuf[me->tail];
        DBC_ENSURE(300, e != (SST_Evt const *)0);

        if (me->tail == 0U) {
            me->tail = me->end;
        } else {
            --me->tail;
        }
        --me->nUsed;

        SST_PORT_CRIT_EXIT();

        (*me->dispatch)(me, e);

        SST_GC(e);
    }
}

//============================================================================
//=== Task launch: semaphore + native thread, called from SST_Task_start.
void SST_Task_setPrio(SST_Task * const me, SST_TaskPrio const prio) {
    DBC_REQUIRE(400, (0U < prio) && (prio <= SST_MAX_TASK));
    DBC_REQUIRE(401, SST_tasks_[prio] == (SST_Task *)0);
    me->prio = prio;
    SST_tasks_[prio] = me;

    int result = PlatformSemaphore_init(&me->sem, 0U);
    DBC_ENSURE(402, result == 0);
    result = PlatformThread_start(&me->thread, &ao_thread, me);
    DBC_ENSURE(403, result == 0);
    (void)result;
}

//============================================================================
//=== Scheduler locking (no-op, critical section handled by SST_PORT_CRIT).
SST_LockKey SST_Task_lock(SST_TaskPrio ceiling) {
    (void)ceiling;
    return 0;
}

void SST_Task_unlock(SST_LockKey lock_key) {
    (void)lock_key;
}
