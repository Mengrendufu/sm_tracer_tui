//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#ifndef RX_PACKET_ASSEMBLER_PRIV_H_
#define RX_PACKET_ASSEMBLER_PRIV_H_

#include <stddef.h>
#include <stdint.h>

#define RX_PACKET_ASSEMBLER_PACKET_SIZE 1024U
#define RX_PACKET_ASSEMBLER_IDLE_MS     5U

typedef void (*RxPacketAssemblerSink)(void *ctx,
                                      uint8_t const *data,
                                      size_t size);

typedef struct {
    uint8_t data[RX_PACKET_ASSEMBLER_PACKET_SIZE];
    size_t size;
    uint64_t latestRxMs;
    RxPacketAssemblerSink sink;
    void *sinkCtx;
} RxPacketAssembler;

void RxPacketAssembler_init(RxPacketAssembler *me,
                            RxPacketAssemblerSink sink,
                            void *sinkCtx);
void RxPacketAssembler_reset(RxPacketAssembler *me);
void RxPacketAssembler_feed(RxPacketAssembler *me,
                            uint8_t const *data,
                            size_t size,
                            uint64_t nowMs);
int RxPacketAssembler_timeoutMs(RxPacketAssembler const *me,
                                uint64_t nowMs);
void RxPacketAssembler_onTimeout(RxPacketAssembler *me,
                                 uint64_t nowMs);

#endif // RX_PACKET_ASSEMBLER_PRIV_H_
