//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "app_sig.h"
#include "sp_mngr/sp_mngr.h"
#include "sp_thread/sp_thread.h"

static pthread_mutex_t l_mutex_ = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t l_cond_ = PTHREAD_COND_INITIALIZER;
static bool l_refreshDispatched_;
static bool l_openDispatched_;
static bool l_runtimeCloseResult_;
static unsigned l_runtimeCloseCalls_;
static unsigned l_closeDispatchCount_;
static SST_Signal l_openResultSig_;
static SerialConfig l_openConfig_;
static char l_portNames_[128];
static size_t l_portNamesSize_;
static int l_serialPipe_[2];
static int l_runtimeFd_ = -1;
static bool l_rxDispatched_;
static bool l_forceReadFailure_;
static bool l_lossDispatched_;
static uint8_t l_rxData_[128];
static size_t l_rxDataSize_;

static SST_Task l_spMngr_;
SST_Task * const AO_SpMngr = &l_spMngr_;

void UI_postText(char const * const text) {
    (void)text;
}

char *SerialPortRuntime_listPorts(size_t * const size) {
    char const portNames[] = "/dev/ttyTEST0\0";
    char * const copy = (char *)malloc(sizeof(portNames));
    if (copy != (char *)0) {
        memcpy(copy, portNames, sizeof(portNames));
        *size = sizeof(portNames);
    }
    return copy;
}

bool SerialPortRuntime_open(SerialConfig const * const config) {
    l_openConfig_ = *config;
    l_runtimeFd_ = l_serialPipe_[0];
    return true;
}

bool SerialPortRuntime_close(void) {
    ++l_runtimeCloseCalls_;
    l_runtimeFd_ = -1;
    return l_runtimeCloseResult_;
}

int SerialPortRuntime_fd(void) {
    return l_runtimeFd_;
}

int SerialPortRuntime_read(uint8_t * const data,
                           size_t const capacity)
{
    if (l_forceReadFailure_) {
        return -1;
    }
    int const size = (int)read(l_serialPipe_[0], data, capacity);
    return (size < 0) && (errno == EAGAIN) ? 0 : size;
}

void *SST_Evt_new(PoolCtr const blockSize) {
    return calloc(1U, blockSize);
}

void SST_Task_post(SST_Task * const me, SST_Evt const * const e) {
    if ((me == AO_SpMngr) && (e->sig == SPMNGR_REFRESHED_PORTS_SIG)) {
        SpMngrPortsEvt const * const result =
            SST_EVT_DOWNCAST(SpMngrPortsEvt, e);

        pthread_mutex_lock(&l_mutex_);
        l_portNamesSize_ = result->portNamesSize;
        memcpy(l_portNames_, result->portNames,
               result->portNamesSize);
        l_refreshDispatched_ = true;
        pthread_cond_signal(&l_cond_);
        pthread_mutex_unlock(&l_mutex_);

        free(result->portNames);
        free((void *)e);
    } else if ((me == AO_SpMngr)
               && (e->sig == SPMNGR_PORT_OPENED_SIG))
    {
        pthread_mutex_lock(&l_mutex_);
        l_openResultSig_ = e->sig;
        l_openDispatched_ = true;
        pthread_cond_signal(&l_cond_);
        pthread_mutex_unlock(&l_mutex_);
    } else if ((me == AO_SpMngr)
               && ((e->sig == SPMNGR_PORT_CLOSED_SIG)
                   || (e->sig == SPMNGR_PORT_CLOSE_FAILED_SIG)))
    {
        pthread_mutex_lock(&l_mutex_);
        l_openResultSig_ = e->sig;
        ++l_closeDispatchCount_;
        pthread_cond_signal(&l_cond_);
        pthread_mutex_unlock(&l_mutex_);
    } else if ((me == AO_SpMngr)
               && (e->sig == SPMNGR_PORT_CONNECTION_LOST_SIG))
    {
        pthread_mutex_lock(&l_mutex_);
        l_lossDispatched_ = true;
        pthread_cond_signal(&l_cond_);
        pthread_mutex_unlock(&l_mutex_);
    } else if ((me == AO_SpMngr)
               && (e->sig == SPMNGR_RX_PACKET_SIG))
    {
        SpMngrRxPacketEvt const * const packet =
            SST_EVT_DOWNCAST(SpMngrRxPacketEvt, e);
        pthread_mutex_lock(&l_mutex_);
        l_rxDataSize_ = packet->size;
        memcpy(l_rxData_, packet->data, packet->size);
        l_rxDispatched_ = true;
        pthread_cond_signal(&l_cond_);
        pthread_mutex_unlock(&l_mutex_);

        free(packet->data);
        free((void *)e);
    }
}

int main(void) {
    int failed = pipe(l_serialPipe_) == 0 ? 0 : 1;
    failed += fcntl(l_serialPipe_[0], F_SETFL, O_NONBLOCK) == 0
              ? 0 : 1;
    failed += SpThread_start() == 0 ? 0 : 1;

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

    char const expected[] = "/dev/ttyTEST0\0";
    failed += l_portNamesSize_ == sizeof(expected)
              && memcmp(l_portNames_, expected,
                        sizeof(expected)) == 0
              ? 0 : 1;

    SerialConfig const config = {
        .portName = "/dev/ttyTEST0",
        .baudRate = 115200,
        .dataBits = 8U,
        .stopBits = SERIAL_STOP_BITS_1,
        .parity = SERIAL_PARITY_NONE,
        .flowControl = SERIAL_FLOW_NONE,
    };
    SpThread_postOpenPort(&config);

    struct timespec deadline;
    (void)clock_gettime(CLOCK_REALTIME, &deadline);
    ++deadline.tv_sec;

    pthread_mutex_lock(&l_mutex_);
    while (!l_openDispatched_) {
        int const status = pthread_cond_timedwait(
            &l_cond_, &l_mutex_, &deadline);
        if (status != 0) {
            failed = 1;
            break;
        }
    }
    pthread_mutex_unlock(&l_mutex_);

    failed += l_openResultSig_ == SPMNGR_PORT_OPENED_SIG ? 0 : 1;
    failed += strcmp(l_openConfig_.portName, config.portName) == 0
              ? 0 : 1;

    uint8_t const rxData[] = {0x11U, 0x22U, 0x33U};
    failed += write(l_serialPipe_[1], rxData, sizeof(rxData))
              == sizeof(rxData) ? 0 : 1;
    (void)clock_gettime(CLOCK_REALTIME, &deadline);
    ++deadline.tv_sec;

    pthread_mutex_lock(&l_mutex_);
    while (!l_rxDispatched_) {
        int const status = pthread_cond_timedwait(
            &l_cond_, &l_mutex_, &deadline);
        if (status != 0) {
            failed = 1;
            break;
        }
    }
    pthread_mutex_unlock(&l_mutex_);

    failed += (l_rxDataSize_ == sizeof(rxData))
              && (memcmp(l_rxData_, rxData, sizeof(rxData)) == 0)
              ? 0 : 1;

    l_runtimeCloseResult_ = false;
    SpThread_postClosePort();
    (void)clock_gettime(CLOCK_REALTIME, &deadline);
    ++deadline.tv_sec;

    pthread_mutex_lock(&l_mutex_);
    while (l_closeDispatchCount_ < 1U) {
        int const status = pthread_cond_timedwait(
            &l_cond_, &l_mutex_, &deadline);
        if (status != 0) {
            failed = 1;
            break;
        }
    }
    pthread_mutex_unlock(&l_mutex_);

    failed += l_openResultSig_ == SPMNGR_PORT_CLOSE_FAILED_SIG
              ? 0 : 1;

    unsigned const closeCallsAfterFailure = l_runtimeCloseCalls_;
    SpThread_postClosePort();
    (void)clock_gettime(CLOCK_REALTIME, &deadline);
    ++deadline.tv_sec;

    pthread_mutex_lock(&l_mutex_);
    while (l_closeDispatchCount_ < 2U) {
        int const status = pthread_cond_timedwait(
            &l_cond_, &l_mutex_, &deadline);
        if (status != 0) {
            failed = 1;
            break;
        }
    }
    pthread_mutex_unlock(&l_mutex_);

    failed += l_openResultSig_ == SPMNGR_PORT_CLOSED_SIG ? 0 : 1;
    failed += l_runtimeCloseCalls_ == closeCallsAfterFailure ? 0 : 1;

    l_openDispatched_ = false;
    SpThread_postOpenPort(&config);
    (void)clock_gettime(CLOCK_REALTIME, &deadline);
    ++deadline.tv_sec;

    pthread_mutex_lock(&l_mutex_);
    while (!l_openDispatched_) {
        int const status = pthread_cond_timedwait(
            &l_cond_, &l_mutex_, &deadline);
        if (status != 0) {
            failed = 1;
            break;
        }
    }
    pthread_mutex_unlock(&l_mutex_);

    l_runtimeCloseResult_ = true;
    l_forceReadFailure_ = true;
    failed += write(l_serialPipe_[1], rxData, sizeof(rxData))
              == sizeof(rxData) ? 0 : 1;
    (void)clock_gettime(CLOCK_REALTIME, &deadline);
    ++deadline.tv_sec;

    pthread_mutex_lock(&l_mutex_);
    while (!l_lossDispatched_) {
        int const status = pthread_cond_timedwait(
            &l_cond_, &l_mutex_, &deadline);
        if (status != 0) {
            failed = 1;
            break;
        }
    }
    pthread_mutex_unlock(&l_mutex_);

    failed += l_runtimeFd_ == -1 ? 0 : 1;

    (void)close(l_serialPipe_[0]);
    (void)close(l_serialPipe_[1]);

    return failed;
}
