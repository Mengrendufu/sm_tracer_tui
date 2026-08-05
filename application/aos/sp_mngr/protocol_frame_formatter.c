//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "dbc_assert.h"
#include "protocol_frame_formatter_priv.h"
DBC_MODULE_NAME("protocol_frame_formatter")

#define PROTOCOL_VALUE_TEXT_CAPACITY 256U

static size_t ProtocolFrameFormatter_argSize_(ProtocolArgType type);
static uint64_t ProtocolFrameFormatter_readBigEndian_(
    uint8_t const *input,
    size_t size);
static bool ProtocolFrameFormatter_valueText_(
    ProtocolArgType type,
    uint8_t const *input,
    size_t inputSize,
    char *text,
    size_t textCapacity,
    size_t *consumed);
static bool ProtocolFrameFormatter_append_(
    ProtocolFrameFormatter *me,
    size_t *length,
    char const *text,
    size_t textSize);
static ProtocolFrameFormatResult ProtocolFrameFormatter_invalidPayload_(
    ProtocolFrameFormatter *me,
    uint8_t const *frame);
static ProtocolFrameFormatResult ProtocolFrameFormatter_overflow_(
    ProtocolFrameFormatter *me);

void ProtocolFrameFormatter_ctor(ProtocolFrameFormatter * const me) {
    DBC_REQUIRE(100, me != (ProtocolFrameFormatter *)0);
    me->text[0] = '\0';
}

ProtocolFrameFormatResult ProtocolFrameFormatter_format(
    ProtocolFrameFormatter * const me,
    ProtocolDecoder const * const decoder,
    uint8_t const * const frame,
    size_t const frameSize)
{
    DBC_REQUIRE(200, me != (ProtocolFrameFormatter *)0);
    DBC_REQUIRE(201, decoder != (ProtocolDecoder const *)0);
    DBC_REQUIRE(202, frame != (uint8_t const *)0);

    me->text[0] = '\0';
    if (frameSize < 3U) {
        (void)snprintf(me->text, sizeof(me->text),
                       "Protocol frame invalid: frame is too short.\n");
        return PROTOCOL_FRAME_FORMAT_INVALID_FRAME;
    }
    if ((size_t)frame[2] != (frameSize - 3U)) {
        (void)snprintf(
            me->text, sizeof(me->text),
            "Protocol frame invalid: payload length mismatch.\n");
        return PROTOCOL_FRAME_FORMAT_INVALID_FRAME;
    }

    ProtocolRecord const * const record =
        ProtocolDecoder_record(decoder, frame[1]);
    if (!record->valid) {
        (void)snprintf(me->text, sizeof(me->text),
                       "[%03u][%03u][%03u][????]\n",
                       (unsigned)frame[0], (unsigned)frame[1],
                       (unsigned)frame[2]);
        return PROTOCOL_FRAME_FORMAT_UNKNOWN_REC_ID;
    }

    size_t outputLength = 0U;
    int const prefixLength = snprintf(
        me->text, sizeof(me->text), "[%03u]", (unsigned)frame[0]);
    DBC_ASSERT(300, prefixLength > 0);
    if ((prefixLength <= 0)
        || ((size_t)prefixLength >= sizeof(me->text)))
    {
        return ProtocolFrameFormatter_overflow_(me);
    }
    outputLength = (size_t)prefixLength;

    size_t payloadOffset = 0U;
    size_t argIndex = 0U;
    size_t const payloadSize = frame[2];
    uint8_t const * const payload = &frame[3];
    for (size_t i = 0U; record->format[i] != '\0'; ++i) {
        if (record->format[i] != '%') {
            if (!ProtocolFrameFormatter_append_(
                    me, &outputLength, &record->format[i], 1U))
            {
                return ProtocolFrameFormatter_overflow_(me);
            }
            continue;
        }

        ++i;
        if (record->format[i] == '%') {
            if (!ProtocolFrameFormatter_append_(
                    me, &outputLength, "%", 1U))
            {
                return ProtocolFrameFormatter_overflow_(me);
            }
            continue;
        }
        if ((record->format[i] != 's')
            || (argIndex >= record->argCount))
        {
            DBC_ASSERT(301, false);
            return ProtocolFrameFormatter_invalidPayload_(me, frame);
        }

        char valueText[PROTOCOL_VALUE_TEXT_CAPACITY];
        size_t consumed = 0U;
        if (!ProtocolFrameFormatter_valueText_(
                record->args[argIndex], &payload[payloadOffset],
                payloadSize - payloadOffset,
                valueText, sizeof(valueText), &consumed))
        {
            return ProtocolFrameFormatter_invalidPayload_(me, frame);
        }
        if (!ProtocolFrameFormatter_append_(
                me, &outputLength, valueText, strlen(valueText)))
        {
            return ProtocolFrameFormatter_overflow_(me);
        }
        payloadOffset += consumed;
        ++argIndex;
    }

    if ((argIndex != record->argCount)
        || (payloadOffset != payloadSize))
    {
        return ProtocolFrameFormatter_invalidPayload_(me, frame);
    }

    DBC_ENSURE(400, me->text[outputLength] == '\0');
    return PROTOCOL_FRAME_FORMAT_OK;
}

char const *ProtocolFrameFormatter_text(
    ProtocolFrameFormatter const * const me)
{
    DBC_REQUIRE(500, me != (ProtocolFrameFormatter const *)0);
    return me->text;
}

static size_t ProtocolFrameFormatter_argSize_(
    ProtocolArgType const type)
{
    switch (type) {
        case PROTOCOL_ARG_INT8:
        case PROTOCOL_ARG_UINT8:
        case PROTOCOL_ARG_HEX8:
        case PROTOCOL_ARG_BIN8:
            return 1U;

        case PROTOCOL_ARG_INT16:
        case PROTOCOL_ARG_UINT16:
        case PROTOCOL_ARG_HEX16:
        case PROTOCOL_ARG_BIN16:
            return 2U;

        case PROTOCOL_ARG_INT32:
        case PROTOCOL_ARG_UINT32:
        case PROTOCOL_ARG_HEX32:
        case PROTOCOL_ARG_BIN32:
        case PROTOCOL_ARG_F32:
            return 4U;

        case PROTOCOL_ARG_INT64:
        case PROTOCOL_ARG_UINT64:
        case PROTOCOL_ARG_HEX64:
        case PROTOCOL_ARG_BIN64:
        case PROTOCOL_ARG_F64:
            return 8U;

        case PROTOCOL_ARG_STRING:
            return 0U;

        default:
            return SIZE_MAX;
    }
}

static uint64_t ProtocolFrameFormatter_readBigEndian_(
    uint8_t const * const input,
    size_t const size)
{
    uint64_t value = 0U;
    for (size_t i = 0U; i < size; ++i) {
        value = (value << 8U) | input[i];
    }
    return value;
}

static bool ProtocolFrameFormatter_valueText_(
    ProtocolArgType const type,
    uint8_t const * const input,
    size_t const inputSize,
    char * const text,
    size_t const textCapacity,
    size_t * const consumed)
{
    size_t const valueSize = ProtocolFrameFormatter_argSize_(type);
    if (valueSize == SIZE_MAX) {
        return false;
    }
    if (type == PROTOCOL_ARG_STRING) {
        size_t textSize = 0U;
        while ((textSize < inputSize) && (input[textSize] != '\0')) {
            ++textSize;
        }
        if (textSize >= textCapacity) {
            return false;
        }
        memcpy(text, input, textSize);
        text[textSize] = '\0';
        *consumed = inputSize;
        return true;
    }
    if (valueSize > inputSize) {
        return false;
    }

    uint64_t const value =
        ProtocolFrameFormatter_readBigEndian_(input, valueSize);
    int length = -1;
    if (type <= PROTOCOL_ARG_INT64) {
        static int const digits[] = {3, 5, 10, 19};
        size_t const index = valueSize == 1U ? 0U
                           : valueSize == 2U ? 1U
                           : valueSize == 4U ? 2U : 3U;
        unsigned const bits = (unsigned)(valueSize * 8U);
        uint64_t const signBit = UINT64_C(1) << (bits - 1U);
        bool const negative = (value & signBit) != 0U;
        uint64_t mask = UINT64_MAX;
        if (valueSize < sizeof(uint64_t)) {
            mask = (UINT64_C(1) << bits) - 1U;
        }
        uint64_t const magnitude = negative
            ? ((~value + 1U) & mask) : value;
        length = snprintf(text, textCapacity, "%c%0*" PRIu64,
                          negative ? '-' : '+', digits[index], magnitude);
    } else if ((type >= PROTOCOL_ARG_UINT8)
               && (type <= PROTOCOL_ARG_UINT64))
    {
        static int const digits[] = {3, 5, 10, 20};
        size_t const index = valueSize == 1U ? 0U
                           : valueSize == 2U ? 1U
                           : valueSize == 4U ? 2U : 3U;
        length = snprintf(text, textCapacity, "%0*" PRIu64,
                          digits[index], value);
    } else if ((type >= PROTOCOL_ARG_HEX8)
               && (type <= PROTOCOL_ARG_HEX64))
    {
        size_t offset = 0U;
        for (size_t i = 0U; i < valueSize; ++i) {
            length = snprintf(&text[offset], textCapacity - offset,
                              i == 0U ? "%02X" : " %02X",
                              (unsigned)input[i]);
            if ((length <= 0)
                || ((size_t)length >= (textCapacity - offset)))
            {
                return false;
            }
            offset += (size_t)length;
        }
        length = (int)offset;
    } else if ((type >= PROTOCOL_ARG_BIN8)
               && (type <= PROTOCOL_ARG_BIN64))
    {
        size_t offset = 0U;
        for (size_t i = 0U; i < valueSize; ++i) {
            if ((i > 0U) && (offset + 1U < textCapacity)) {
                text[offset++] = '\'';
            }
            for (unsigned bit = 0U; bit < 8U; ++bit) {
                if (offset + 1U >= textCapacity) {
                    return false;
                }
                text[offset++] =
                    (input[i] & (uint8_t)(0x80U >> bit)) != 0U
                    ? '1' : '0';
            }
        }
        text[offset] = '\0';
        length = (int)offset;
    } else if (type == PROTOCOL_ARG_F32) {
        uint32_t const bits = (uint32_t)value;
        float number;
        memcpy(&number, &bits, sizeof(number));
        length = snprintf(text, textCapacity, "%.7g", (double)number);
    } else if (type == PROTOCOL_ARG_F64) {
        double number;
        memcpy(&number, &value, sizeof(number));
        length = snprintf(text, textCapacity, "%.16g", number);
    }

    if ((length < 0) || ((size_t)length >= textCapacity)) {
        return false;
    }
    *consumed = valueSize;
    return true;
}

static bool ProtocolFrameFormatter_append_(
    ProtocolFrameFormatter * const me,
    size_t * const length,
    char const * const text,
    size_t const textSize)
{
    if (textSize >= (sizeof(me->text) - *length)) {
        return false;
    }
    memcpy(&me->text[*length], text, textSize);
    *length += textSize;
    me->text[*length] = '\0';
    return true;
}

static ProtocolFrameFormatResult ProtocolFrameFormatter_invalidPayload_(
    ProtocolFrameFormatter * const me,
    uint8_t const * const frame)
{
    (void)snprintf(me->text, sizeof(me->text),
                   "[%03u][%03u][%03u][INVALID]\n",
                   (unsigned)frame[0], (unsigned)frame[1],
                   (unsigned)frame[2]);
    return PROTOCOL_FRAME_FORMAT_INVALID_PAYLOAD;
}

static ProtocolFrameFormatResult ProtocolFrameFormatter_overflow_(
    ProtocolFrameFormatter * const me)
{
    (void)snprintf(me->text, sizeof(me->text),
                   "Protocol frame formatting overflow.\n");
    return PROTOCOL_FRAME_FORMAT_OUTPUT_OVERFLOW;
}
