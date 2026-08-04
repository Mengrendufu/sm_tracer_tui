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
//=== SpThread runtime: lifecycle, mixed wake, and RX assembly
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include "dbc_assert.h"
#include "sp_thread/sp_thread.h"
#include "sp_thread/hsm/sm_sp_thread.h"
#include "rx_packet_assembler_priv.h"
#include "serial_port_runtime_priv.h"
#include "sp_thread_evt_priv.h"
#include "sp_thread_wake_priv.h"
#include "ui_evt.h"
DBC_MODULE_NAME("sp_thread")

//============================================================================
//=== Runtime instance

typedef struct {
    pthread_t thread;
    SM_SpThread hsm;
    RxPacketAssembler rxAssembler;
    bool started;
} SpThread;

static SpThread SpThread_inst_;

//============================================================================
//=== Thread entry

static uint64_t SpThread_monotonicMs_(void) {
    struct timespec now;
    int const status = clock_gettime(CLOCK_MONOTONIC, &now);
    DBC_ASSERT(200, status == 0);
    (void)status;

    return ((uint64_t)now.tv_sec * 1000U)
           + ((uint64_t)now.tv_nsec / 1000000U);
}

static void SpThread_printPacket_(void * const ctx,
                                  uint8_t const * const data,
                                  size_t const size)
{
    (void)ctx;
    DBC_REQUIRE(210, data != (uint8_t const *)0);
    DBC_REQUIRE(211, (size > 0U)
                     && (size <= RX_PACKET_ASSEMBLER_PACKET_SIZE));

    char text[(RX_PACKET_ASSEMBLER_PACKET_SIZE * 3U) + 48U];
    int written = snprintf(text, sizeof(text),
                           "SpThread RX [%zu bytes]:", size);
    DBC_ASSERT(212, (written > 0) && ((size_t)written < sizeof(text)));

    size_t offset = (size_t)written;
    for (size_t i = 0U; i < size; ++i) {
        written = snprintf(&text[offset], sizeof(text) - offset,
                           " %02X", (unsigned)data[i]);
        DBC_ASSERT(213, (written > 0)
                        && ((size_t)written < (sizeof(text) - offset)));
        offset += (size_t)written;
    }
    text[offset++] = '\n';
    text[offset] = '\0';
    UI_postText(text);
}

static void *SpThread_run_(void * const arg) {
    SpThread * const me = (SpThread *)arg;
    DBC_REQUIRE(220, me != (SpThread *)0);

    SM_SpThread_ctor(&me->hsm);
    SM_SpThread_init(&me->hsm);
    RxPacketAssembler_init(&me->rxAssembler,
                           &SpThread_printPacket_, me);
    SpThreadWake_init(SpThread_evtWakeFd());

    int serialFd = -1;
    for (;;) {
        int currentFd = SerialPortRuntime_fd();
        if (currentFd != serialFd) {
            RxPacketAssembler_reset(&me->rxAssembler);
            SpThreadWake_setSerialFd(currentFd);
            serialFd = currentFd;
        }

        uint64_t nowMs = SpThread_monotonicMs_();
        int const timeoutMs = RxPacketAssembler_timeoutMs(
            &me->rxAssembler, nowMs);
        int const ready = SpThreadWake_wait(timeoutMs);
        if ((ready == SP_THREAD_WAKE_ERROR) && (errno == EINTR)) {
            continue;
        }
        if (ready == SP_THREAD_WAKE_ERROR) {
            DBC_ERROR(221);
        }

        if ((ready & SP_THREAD_WAKE_EVENT) != 0) {
            int const consumeStatus = SpThread_evtConsumeWake();
            DBC_ASSERT(222, consumeStatus == 0);
            (void)consumeStatus;

            SpThreadEvt e;
            while (SpThread_evtDequeue(&e)) {
                SM_SpThread_dispatchEvt(&me->hsm, &e);
            }
        }

        currentFd = SerialPortRuntime_fd();
        if (currentFd != serialFd) {
            RxPacketAssembler_reset(&me->rxAssembler);
            SpThreadWake_setSerialFd(currentFd);
            serialFd = currentFd;
        }

        if (((ready & SP_THREAD_WAKE_SERIAL) != 0)
            && (serialFd >= 0))
        {
            uint8_t rxData[256U];
            int readSize;
            do {
                readSize = SerialPortRuntime_read(rxData,
                                                  sizeof(rxData));
                if (readSize > 0) {
                    RxPacketAssembler_feed(&me->rxAssembler, rxData,
                                           (size_t)readSize,
                                           SpThread_monotonicMs_());
                }
            } while (readSize > 0);

            if (readSize < 0) {
                UI_postText("SpThread: serial receive failed.\n");
            }
        }

        nowMs = SpThread_monotonicMs_();
        RxPacketAssembler_onTimeout(&me->rxAssembler, nowMs);
    }

    return (void *)0;
}

//============================================================================
//=== Lifecycle

int SpThread_start(void) {
    SpThread * const me = &SpThread_inst_;
    DBC_REQUIRE(100, !me->started);

    int status = SpThread_evtInit();
    if (status != 0) {
        return status;
    }

    status = pthread_create(&me->thread,
                            (pthread_attr_t const *)0,
                            &SpThread_run_,
                            me);
    if (status != 0) {
        SpThread_evtDeinit();
        return status;
    }

    me->started = true;
    return 0;
}
