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
#include "sp_mngr/protocol_frame_formatter_priv.h"

static void setRecord_(ProtocolDecoder * const decoder,
                       uint8_t const recId,
                       char const * const format,
                       ProtocolArgType const * const args,
                       uint8_t const argCount)
{
    ProtocolRecord * const record =
        &decoder->tables[decoder->activeTable].records[recId];
    record->valid = true;
    record->argCount = argCount;
    memcpy(record->args, args,
           (size_t)argCount * sizeof(record->args[0]));
    (void)strcpy(record->format, format);
}

static int expectFrame_(ProtocolFrameFormatter * const formatter,
                        ProtocolDecoder const * const decoder,
                        uint8_t const * const frame,
                        size_t const frameSize,
                        ProtocolFrameFormatResult const expectedResult,
                        char const * const expectedText)
{
    ProtocolFrameFormatResult const result =
        ProtocolFrameFormatter_format(
            formatter, decoder, frame, frameSize);
    return (result == expectedResult)
           && (strcmp(ProtocolFrameFormatter_text(formatter),
                      expectedText) == 0)
           ? 0 : 1;
}

int main(void) {
    int failed = 0;
    ProtocolDecoder decoder = {0};
    ProtocolFrameFormatter formatter;
    ProtocolFrameFormatter_ctor(&formatter);

    setRecord_(&decoder, 1U, "==ledOn==\n",
               (ProtocolArgType const *)0, 0U);
    uint8_t const noArgs[] = {7U, 1U, 0U};
    failed += expectFrame_(
        &formatter, &decoder, noArgs, sizeof(noArgs),
        PROTOCOL_FRAME_FORMAT_OK, "[007]==ledOn==\n");

    ProtocolArgType const integerArgs[] = {
        PROTOCOL_ARG_INT8, PROTOCOL_ARG_INT16,
        PROTOCOL_ARG_INT32, PROTOCOL_ARG_INT64,
        PROTOCOL_ARG_UINT8, PROTOCOL_ARG_UINT16,
        PROTOCOL_ARG_UINT32, PROTOCOL_ARG_UINT64,
    };
    setRecord_(
        &decoder, 2U, "%s|%s|%s|%s|%s|%s|%s|%s\n",
        integerArgs,
        (uint8_t)(sizeof(integerArgs) / sizeof(integerArgs[0])));
    uint8_t const integers[] = {
        8U, 2U, 30U,
        0xFFU,
        0xFFU, 0xFEU,
        0xFFU, 0xFFU, 0xFFU, 0xFEU,
        0xFFU, 0xFFU, 0xFFU, 0xFFU,
        0xFFU, 0xFFU, 0xFFU, 0xFEU,
        0x07U,
        0x00U, 0x07U,
        0x00U, 0x00U, 0x00U, 0x07U,
        0x00U, 0x00U, 0x00U, 0x00U,
        0x00U, 0x00U, 0x00U, 0x07U,
    };
    failed += expectFrame_(
        &formatter, &decoder, integers, sizeof(integers),
        PROTOCOL_FRAME_FORMAT_OK,
        "[008]-001|-00002|-0000000002|-0000000000000000002|"
        "007|00007|0000000007|00000000000000000007\n");

    ProtocolArgType const radixArgs[] = {
        PROTOCOL_ARG_HEX16, PROTOCOL_ARG_BIN16,
    };
    setRecord_(&decoder, 3U, "%s|%s\n", radixArgs, 2U);
    uint8_t const radix[] = {
        9U, 3U, 4U, 0xABU, 0xCDU, 0xABU, 0xCDU,
    };
    failed += expectFrame_(
        &formatter, &decoder, radix, sizeof(radix),
        PROTOCOL_FRAME_FORMAT_OK,
        "[009]AB CD|10101011'11001101\n");

    ProtocolArgType const mixedArgs[] = {
        PROTOCOL_ARG_F32, PROTOCOL_ARG_F64, PROTOCOL_ARG_STRING,
    };
    setRecord_(&decoder, 4U, "%s|%s|%s\n", mixedArgs, 3U);
    uint8_t const mixed[] = {
        10U, 4U, 14U,
        0x3FU, 0xC0U, 0x00U, 0x00U,
        0x3FU, 0xF8U, 0x00U, 0x00U,
        0x00U, 0x00U, 0x00U, 0x00U,
        'O', 'K',
    };
    failed += expectFrame_(
        &formatter, &decoder, mixed, sizeof(mixed),
        PROTOCOL_FRAME_FORMAT_OK, "[010]1.5|1.5|OK\n");

    uint8_t const unknown[] = {9U, 200U, 0U};
    failed += expectFrame_(
        &formatter, &decoder, unknown, sizeof(unknown),
        PROTOCOL_FRAME_FORMAT_UNKNOWN_REC_ID,
        "[009][200][000][????]\n");

    uint8_t const invalidFrame[] = {1U, 2U, 2U, 0xAAU};
    failed += expectFrame_(
        &formatter, &decoder, invalidFrame, sizeof(invalidFrame),
        PROTOCOL_FRAME_FORMAT_INVALID_FRAME,
        "Protocol frame invalid: payload length mismatch.\n");

    ProtocolArgType const shortArg[] = {PROTOCOL_ARG_UINT16};
    setRecord_(&decoder, 6U, "%s\n", shortArg, 1U);
    uint8_t const shortPayload[] = {1U, 6U, 1U, 0xAAU};
    failed += expectFrame_(
        &formatter, &decoder, shortPayload, sizeof(shortPayload),
        PROTOCOL_FRAME_FORMAT_INVALID_PAYLOAD,
        "[001][006][001][INVALID]\n");

    return failed == 0 ? 0 : 1;
}
