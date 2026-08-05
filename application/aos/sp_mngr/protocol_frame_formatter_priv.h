//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#ifndef PROTOCOL_FRAME_FORMATTER_PRIV_H_
#define PROTOCOL_FRAME_FORMATTER_PRIV_H_

#include <stddef.h>
#include <stdint.h>
#include "protocol_decoder_priv.h"

#define PROTOCOL_FRAME_TEXT_CAPACITY 2048U

typedef enum {
    PROTOCOL_FRAME_FORMAT_OK,
    PROTOCOL_FRAME_FORMAT_UNKNOWN_REC_ID,
    PROTOCOL_FRAME_FORMAT_INVALID_FRAME,
    PROTOCOL_FRAME_FORMAT_INVALID_PAYLOAD,
    PROTOCOL_FRAME_FORMAT_OUTPUT_OVERFLOW
} ProtocolFrameFormatResult;

typedef struct {
    char text[PROTOCOL_FRAME_TEXT_CAPACITY];
} ProtocolFrameFormatter;

void ProtocolFrameFormatter_ctor(ProtocolFrameFormatter *me);
ProtocolFrameFormatResult ProtocolFrameFormatter_format(
    ProtocolFrameFormatter *me,
    ProtocolDecoder const *decoder,
    uint8_t const *frame,
    size_t frameSize);
char const *ProtocolFrameFormatter_text(
    ProtocolFrameFormatter const *me);

#endif // PROTOCOL_FRAME_FORMATTER_PRIV_H_
