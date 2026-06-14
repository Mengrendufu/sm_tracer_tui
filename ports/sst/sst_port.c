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
#include "dbc_assert.h"
#include <time.h>

DBC_MODULE_NAME("sst_port")

//============================================================================
//=== Critical section: ref-counted non-recursive mutex.
static pthread_mutex_t l_portLock = PTHREAD_MUTEX_INITIALIZER;

static __thread int l_critSectNest;

void enterCriticalSection_(void) {
    DBC_REQUIRE(100, l_critSectNest == 0);
    pthread_mutex_lock(&l_portLock);
    ++l_critSectNest;
}

void leaveCriticalSection_(void) {
    DBC_REQUIRE(200, l_critSectNest == 1);
    --l_critSectNest;
    pthread_mutex_unlock(&l_portLock);
}

//============================================================================
//=== AO thread: sem_wait -> dequeue -> dispatch.
static void *ao_thread(void *arg) {
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
    return NULL;
}

//============================================================================
//=== Task launch: sem_init + pthread_create, called from SST_Task_start.
void SST_Task_portStart_(SST_Task * const me) {
    sem_init(&me->sem, 0, 0);
    pthread_create(&me->thread, NULL, ao_thread, me);
}

//============================================================================
//=== SST kernel callbacks.

//============================================================================
//=== Scheduler locking (no-op, critical section handled by SST_PORT_CRIT).
SST_LockKey SST_Task_lock(SST_TaskPrio ceiling) {
    (void)ceiling;
    return 0;
}

void SST_Task_unlock(SST_LockKey lock_key) {
    (void)lock_key;
}
