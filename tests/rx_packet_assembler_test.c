//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#include <stdint.h>
#include <string.h>
#include "sp_thread/thread/rx_packet_assembler_priv.h"

typedef struct {
    uint8_t packet[RX_PACKET_ASSEMBLER_PACKET_SIZE];
    size_t size;
    unsigned count;
} PacketSink;

static void capturePacket_(void * const ctx,
                           uint8_t const * const data,
                           size_t const size)
{
    PacketSink * const sink = (PacketSink *)ctx;
    memcpy(sink->packet, data, size);
    sink->size = size;
    ++sink->count;
}

int main(void) {
    int failed = 0;
    PacketSink sink = {0};
    RxPacketAssembler assembler;
    RxPacketAssembler_init(&assembler, &capturePacket_, &sink);

    uint8_t const shortPacket[] = {0x11U, 0x22U, 0x33U};
    RxPacketAssembler_feed(&assembler, shortPacket,
                           sizeof(shortPacket), 100U);
    failed += sink.count == 0U ? 0 : 1;
    failed += RxPacketAssembler_timeoutMs(&assembler, 103U) == 2
              ? 0 : 1;
    RxPacketAssembler_onTimeout(&assembler, 104U);
    failed += sink.count == 0U ? 0 : 1;
    RxPacketAssembler_onTimeout(&assembler, 105U);
    failed += (sink.count == 1U)
              && (sink.size == sizeof(shortPacket))
              && (memcmp(sink.packet, shortPacket,
                         sizeof(shortPacket)) == 0)
              ? 0 : 1;
    failed += RxPacketAssembler_timeoutMs(&assembler, 105U) == -1
              ? 0 : 1;

    uint8_t fullPacket[RX_PACKET_ASSEMBLER_PACKET_SIZE];
    memset(fullPacket, 0xA5, sizeof(fullPacket));
    RxPacketAssembler_feed(&assembler, fullPacket,
                           sizeof(fullPacket), 200U);
    failed += (sink.count == 2U)
              && (sink.size == sizeof(fullPacket))
              && (memcmp(sink.packet, fullPacket,
                         sizeof(fullPacket)) == 0)
              ? 0 : 1;

    uint8_t splitPacket[RX_PACKET_ASSEMBLER_PACKET_SIZE + 1U];
    memset(splitPacket, 0x5A, sizeof(splitPacket));
    RxPacketAssembler_feed(&assembler, splitPacket,
                           sizeof(splitPacket), 300U);
    failed += (sink.count == 3U)
              && (sink.size == RX_PACKET_ASSEMBLER_PACKET_SIZE)
              ? 0 : 1;
    RxPacketAssembler_onTimeout(&assembler, 305U);
    failed += (sink.count == 4U)
              && (sink.size == 1U)
              && (sink.packet[0] == 0x5AU)
              ? 0 : 1;

    RxPacketAssembler_reset(&assembler);
    failed += RxPacketAssembler_timeoutMs(&assembler, 400U) == -1
              ? 0 : 1;

    return failed == 0 ? 0 : 1;
}
