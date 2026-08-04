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
//=== Component: RxPacketAssembler
#include <string.h>
#include "dbc_assert.h"
#include "rx_packet_assembler_priv.h"
DBC_MODULE_NAME("rx_packet_assembler")

static void RxPacketAssembler_emit_(RxPacketAssembler * const me) {
    DBC_REQUIRE(100, me != (RxPacketAssembler *)0);
    DBC_ASSERT(101, me->size > 0U);

    (*me->sink)(me->sinkCtx, me->data, me->size);
    me->size = 0U;
}

void RxPacketAssembler_init(RxPacketAssembler * const me,
                            RxPacketAssemblerSink const sink,
                            void * const sinkCtx)
{
    DBC_REQUIRE(200, me != (RxPacketAssembler *)0);
    DBC_REQUIRE(201, sink != (RxPacketAssemblerSink)0);

    me->size = 0U;
    me->latestRxMs = 0U;
    me->sink = sink;
    me->sinkCtx = sinkCtx;
}

void RxPacketAssembler_reset(RxPacketAssembler * const me) {
    DBC_REQUIRE(300, me != (RxPacketAssembler *)0);

    me->size = 0U;
    me->latestRxMs = 0U;
}

void RxPacketAssembler_feed(RxPacketAssembler * const me,
                            uint8_t const *data,
                            size_t size,
                            uint64_t const nowMs)
{
    DBC_REQUIRE(400, me != (RxPacketAssembler *)0);
    DBC_REQUIRE(401, (size == 0U) || (data != (uint8_t const *)0));

    while (size > 0U) {
        size_t const available = sizeof(me->data) - me->size;
        size_t const chunk = size < available ? size : available;
        memcpy(&me->data[me->size], data, chunk);
        me->size += chunk;
        me->latestRxMs = nowMs;
        data += chunk;
        size -= chunk;

        if (me->size == sizeof(me->data)) {
            RxPacketAssembler_emit_(me);
        }
    }
}

int RxPacketAssembler_timeoutMs(RxPacketAssembler const * const me,
                                uint64_t const nowMs)
{
    DBC_REQUIRE(500, me != (RxPacketAssembler const *)0);

    if (me->size == 0U) {
        return -1;
    }

    uint64_t const elapsed = nowMs - me->latestRxMs;
    return elapsed >= RX_PACKET_ASSEMBLER_IDLE_MS
           ? 0
           : (int)(RX_PACKET_ASSEMBLER_IDLE_MS - elapsed);
}

void RxPacketAssembler_onTimeout(RxPacketAssembler * const me,
                                 uint64_t const nowMs)
{
    DBC_REQUIRE(600, me != (RxPacketAssembler *)0);

    if (RxPacketAssembler_timeoutMs(me, nowMs) == 0) {
        RxPacketAssembler_emit_(me);
    }
}
