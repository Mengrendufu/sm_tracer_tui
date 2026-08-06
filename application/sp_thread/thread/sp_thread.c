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
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "sst.h"
#include "dbc_assert.h"
#include "platform_port.h"
#include "app_sig.h"
#include "sp_mngr/sp_mngr.h"
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
    PlatformThread thread;
    SM_SpThread hsm;
    RxPacketAssembler rxAssembler;
    bool started;
} SpThread;

static SpThread SpThread_inst_;

//============================================================================
//=== Thread entry

static uint64_t SpThread_monotonicMs_(void) {
    uint64_t now;
    int const status = Platform_monotonicMs(&now);
    DBC_ASSERT(200, status == 0);
    (void)status;
    return now;
}

static void SpThread_postPacket_(void * const ctx,
                                 uint8_t const * const data,
                                 size_t const size)
{
    (void)ctx;
    DBC_REQUIRE(210, data != (uint8_t const *)0);
    DBC_REQUIRE(211, (size > 0U)
                     && (size <= RX_PACKET_ASSEMBLER_PACKET_SIZE));

    uint8_t * const dataCopy = (uint8_t *)malloc(size);
    DBC_ENSURE(212, dataCopy != (uint8_t *)0);
    memcpy(dataCopy, data, size);

    SpMngrRxPacketEvt * const packet = SST_NEW(SpMngrRxPacketEvt);
    packet->super.sig = SPMNGR_RX_PACKET_SIG;
    packet->data = dataCopy;
    packet->size = size;
    SST_Task_post(AO_SpMngr, &packet->super);
}

static void SpThread_run_(void * const arg) {
    SpThread * const me = (SpThread *)arg;
    static SpThreadEvt const lostEvt = {
        .sig = SPTHRD_PORT_LOST_SIG,
    };
    DBC_REQUIRE(220, me != (SpThread *)0);

    SM_SpThread_ctor(&me->hsm);
    SM_SpThread_init(&me->hsm);
    RxPacketAssembler_init(&me->rxAssembler,
                           &SpThread_postPacket_, me);
    SpThreadWake_init(SpThread_evtWakeObject());

    PlatformWaitObject serialObject = PLATFORM_WAIT_OBJECT_INVALID;
    for (;;) {
        PlatformWaitObject currentObject =
            SerialPortRuntime_waitObject();
        if (!PlatformWaitObject_equal(currentObject, serialObject)) {
            RxPacketAssembler_reset(&me->rxAssembler);
            SpThreadWake_setSerialObject(currentObject);
            serialObject = currentObject;
        }

        uint64_t nowMs = SpThread_monotonicMs_();
        int const timeoutMs = RxPacketAssembler_timeoutMs(
            &me->rxAssembler, nowMs);
        int const ready = SpThreadWake_wait(timeoutMs);
        if (ready == SP_THREAD_WAKE_INTERRUPTED) {
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
                if (e.sig == SPTHRD_APPLY_CONFIG_SIG) {
                    RxPacketAssembler_reset(&me->rxAssembler);
                }
            }
        }

        if (((ready & SP_THREAD_WAKE_SERIAL_LOST) != 0)
            && PlatformWaitObject_isValid(
                   SerialPortRuntime_waitObject()))
        {
            SM_SpThread_dispatchEvt(&me->hsm, &lostEvt);
        }

        currentObject = SerialPortRuntime_waitObject();
        if (!PlatformWaitObject_equal(currentObject, serialObject)) {
            RxPacketAssembler_reset(&me->rxAssembler);
            SpThreadWake_setSerialObject(currentObject);
            serialObject = currentObject;
        }

        if (((ready & SP_THREAD_WAKE_SERIAL) != 0)
            && PlatformWaitObject_isValid(serialObject))
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
                SM_SpThread_dispatchEvt(&me->hsm, &lostEvt);
                RxPacketAssembler_reset(&me->rxAssembler);
                SpThreadWake_setSerialObject(
                    PLATFORM_WAIT_OBJECT_INVALID);
                serialObject = PLATFORM_WAIT_OBJECT_INVALID;
            }
        }

        nowMs = SpThread_monotonicMs_();
        RxPacketAssembler_onTimeout(&me->rxAssembler, nowMs);
    }

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

    status = PlatformThread_start(&me->thread, &SpThread_run_, me);
    if (status != 0) {
        SpThread_evtDeinit();
        return status;
    }

    me->started = true;
    return 0;
}
