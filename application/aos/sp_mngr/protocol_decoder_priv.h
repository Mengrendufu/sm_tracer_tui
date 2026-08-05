//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#ifndef PROTOCOL_DECODER_PRIV_H_
#define PROTOCOL_DECODER_PRIV_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PROTOCOL_DECODER_RECORD_NUM      256U
#define PROTOCOL_DECODER_ARG_MAX         16U
#define PROTOCOL_DECODER_FORMAT_CAPACITY 256U
#define PROTOCOL_DECODER_FILE_MAX_SIZE   (256U * 1024U)
#define PROTOCOL_DECODER_INPUT_PADDING   4U
#define PROTOCOL_DECODER_ERROR_FIELD_CAPACITY 32U
#define PROTOCOL_DECODER_ERROR_DETAIL_CAPACITY 160U
// yyjson 0.12.0 worst-case bound for in-situ parsing.
#define PROTOCOL_DECODER_PARSE_ARENA_SIZE \
    ((PROTOCOL_DECODER_FILE_MAX_SIZE * 12U) + 256U)

typedef uint8_t ProtocolArgType;

enum ProtocolArgTypes {
    PROTOCOL_ARG_INT8,
    PROTOCOL_ARG_INT16,
    PROTOCOL_ARG_INT32,
    PROTOCOL_ARG_INT64,
    PROTOCOL_ARG_UINT8,
    PROTOCOL_ARG_UINT16,
    PROTOCOL_ARG_UINT32,
    PROTOCOL_ARG_UINT64,
    PROTOCOL_ARG_HEX8,
    PROTOCOL_ARG_HEX16,
    PROTOCOL_ARG_HEX32,
    PROTOCOL_ARG_HEX64,
    PROTOCOL_ARG_BIN8,
    PROTOCOL_ARG_BIN16,
    PROTOCOL_ARG_BIN32,
    PROTOCOL_ARG_BIN64,
    PROTOCOL_ARG_F32,
    PROTOCOL_ARG_F64,
    PROTOCOL_ARG_STRING,
    PROTOCOL_ARG_NUM
};

typedef struct {
    bool valid;
    uint8_t argCount;
    ProtocolArgType args[PROTOCOL_DECODER_ARG_MAX];
    char format[PROTOCOL_DECODER_FORMAT_CAPACITY];
} ProtocolRecord;

typedef struct {
    ProtocolRecord records[PROTOCOL_DECODER_RECORD_NUM];
} ProtocolTable;

typedef enum {
    PROTOCOL_LOAD_OK,
    PROTOCOL_LOAD_OPEN_FAILED,
    PROTOCOL_LOAD_FILE_SIZE_INVALID,
    PROTOCOL_LOAD_READ_FAILED,
    PROTOCOL_LOAD_PARSE_FAILED,
    PROTOCOL_LOAD_SCHEMA_INVALID
} ProtocolLoadResult;

typedef struct {
    ProtocolLoadResult result;
    size_t byteOffset;
    size_t line;
    size_t column;
    size_t tokenIndex;
    int recId;
    char field[PROTOCOL_DECODER_ERROR_FIELD_CAPACITY];
    char detail[PROTOCOL_DECODER_ERROR_DETAIL_CAPACITY];
} ProtocolLoadError;

typedef union {
    uint64_t align;
    uint8_t bytes[PROTOCOL_DECODER_PARSE_ARENA_SIZE];
} ProtocolParseArena;

typedef struct {
    ProtocolTable tables[2];
    char fileBuffer[
        PROTOCOL_DECODER_FILE_MAX_SIZE
        + PROTOCOL_DECODER_INPUT_PADDING];
    ProtocolParseArena parseArena;
    ProtocolLoadError error;
    uint8_t activeTable;
} ProtocolDecoder;

void ProtocolDecoder_ctor(ProtocolDecoder *me);
ProtocolLoadResult ProtocolDecoder_loadFile(ProtocolDecoder *me,
                                             char const *path);
ProtocolRecord const *ProtocolDecoder_record(
    ProtocolDecoder const *me,
    uint8_t recId);
ProtocolLoadError const *ProtocolDecoder_error(
    ProtocolDecoder const *me);

#endif // PROTOCOL_DECODER_PRIV_H_
