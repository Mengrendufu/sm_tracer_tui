//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "app_sig.h"
#include "sp_mngr/sp_mngr.h"
#include "sp_thread/sp_thread.h"

static pthread_mutex_t l_mutex_ = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t l_cond_ = PTHREAD_COND_INITIALIZER;
static bool l_refreshDispatched_;
static char l_resultText_[128];

static SST_Task l_spMngr_;
SST_Task * const AO_SpMngr = &l_spMngr_;

void UI_postText(char const * const text) {
    (void)text;
}

char *SerialPortRuntime_listPortsText(void) {
    char const text[] = "Serial ports:\n/dev/ttyTEST0\n";
    char * const copy = (char *)malloc(sizeof(text));
    if (copy != (char *)0) {
        memcpy(copy, text, sizeof(text));
    }
    return copy;
}

void *SST_Evt_new(PoolCtr const blockSize) {
    return calloc(1U, blockSize);
}

void SST_Task_post(SST_Task * const me, SST_Evt const * const e) {
    if ((me == AO_SpMngr) && (e->sig == SPMNGR_REFRESHED_PORTS_SIG)) {
        SpMngrPortsEvt const * const result =
            SST_EVT_DOWNCAST(SpMngrPortsEvt, e);

        pthread_mutex_lock(&l_mutex_);
        (void)snprintf(l_resultText_, sizeof(l_resultText_),
                       "%s", result->text);
        l_refreshDispatched_ = true;
        pthread_cond_signal(&l_cond_);
        pthread_mutex_unlock(&l_mutex_);

        free(result->text);
        free((void *)e);
    }
}

int main(void) {
    int failed = SpThread_start() == 0 ? 0 : 1;

    if (failed == 0) {
        SpThread_postRefreshPorts();

        struct timespec deadline;
        (void)clock_gettime(CLOCK_REALTIME, &deadline);
        ++deadline.tv_sec;

        pthread_mutex_lock(&l_mutex_);
        while (!l_refreshDispatched_) {
            int const status = pthread_cond_timedwait(
                &l_cond_, &l_mutex_, &deadline);
            if (status != 0) {
                failed = 1;
                break;
            }
        }
        pthread_mutex_unlock(&l_mutex_);
    }

    failed += strcmp(l_resultText_,
                     "Serial ports:\n/dev/ttyTEST0\n") == 0
              ? 0 : 1;

    return failed;
}
