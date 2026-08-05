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
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "dbc_assert.h"
#include "yyjson.h"
#include "protocol_decoder_priv.h"
DBC_MODULE_NAME("protocol_decoder")

typedef struct {
    char const *name;
    ProtocolArgType type;
} ProtocolArgName_;

static ProtocolArgName_ const l_argNames_[] = {
    {"INT8",   PROTOCOL_ARG_INT8},
    {"INT16",  PROTOCOL_ARG_INT16},
    {"INT32",  PROTOCOL_ARG_INT32},
    {"INT64",  PROTOCOL_ARG_INT64},
    {"UINT8",  PROTOCOL_ARG_UINT8},
    {"UINT16", PROTOCOL_ARG_UINT16},
    {"UINT32", PROTOCOL_ARG_UINT32},
    {"UINT64", PROTOCOL_ARG_UINT64},
    {"HEX8",   PROTOCOL_ARG_HEX8},
    {"HEX16",  PROTOCOL_ARG_HEX16},
    {"HEX32",  PROTOCOL_ARG_HEX32},
    {"HEX64",  PROTOCOL_ARG_HEX64},
    {"BIN8",   PROTOCOL_ARG_BIN8},
    {"BIN16",  PROTOCOL_ARG_BIN16},
    {"BIN32",  PROTOCOL_ARG_BIN32},
    {"BIN64",  PROTOCOL_ARG_BIN64},
    {"F32",    PROTOCOL_ARG_F32},
    {"F64",    PROTOCOL_ARG_F64},
    {"STRING", PROTOCOL_ARG_STRING},
};

static void ProtocolDecoder_clearError_(ProtocolDecoder *me);
static ProtocolLoadResult ProtocolDecoder_fail_(
    ProtocolDecoder *me,
    ProtocolLoadResult result,
    size_t tokenIndex,
    int recId,
    char const *field,
    char const *format,
    ...);
static bool ProtocolDecoder_readFile_(ProtocolDecoder *me,
                                      char const *path,
                                      size_t *fileSize);
static bool ProtocolDecoder_buildTable_(ProtocolDecoder *me,
                                        ProtocolTable *table,
                                        yyjson_val *root);
static bool ProtocolDecoder_buildRecord_(ProtocolDecoder *me,
                                         ProtocolRecord *record,
                                         yyjson_val *token,
                                         size_t tokenIndex,
                                         int recId);
static bool ProtocolDecoder_valueKeysUnique_(ProtocolDecoder *me,
                                             yyjson_val *value,
                                             size_t tokenIndex,
                                             int recId);
static bool ProtocolDecoder_objectKeysUnique_(ProtocolDecoder *me,
                                              yyjson_val *object,
                                              size_t tokenIndex,
                                              int recId);
static bool ProtocolDecoder_argType_(char const *name,
                                     size_t nameSize,
                                     ProtocolArgType *type);
static bool ProtocolDecoder_formatValid_(char const *format,
                                         size_t formatSize,
                                         size_t *placeholderCount);

void ProtocolDecoder_ctor(ProtocolDecoder * const me) {
    DBC_REQUIRE(100, me != (ProtocolDecoder *)0);
    DBC_ASSERT(101,
               PROTOCOL_DECODER_INPUT_PADDING
               >= YYJSON_PADDING_SIZE);
    DBC_ASSERT(102,
        yyjson_read_max_memory_usage(
            PROTOCOL_DECODER_FILE_MAX_SIZE,
            YYJSON_READ_INSITU | YYJSON_READ_ALLOW_COMMENTS)
        <= sizeof(me->parseArena.bytes));

    memset(me, 0, sizeof(*me));
    ProtocolDecoder_clearError_(me);
}

ProtocolLoadResult ProtocolDecoder_loadFile(
    ProtocolDecoder * const me,
    char const * const path)
{
    DBC_REQUIRE(200, me != (ProtocolDecoder *)0);
    DBC_REQUIRE(201, path != (char const *)0);

    ProtocolDecoder_clearError_(me);

    size_t fileSize;
    if (!ProtocolDecoder_readFile_(me, path, &fileSize)) {
        return me->error.result;
    }

    memset(&me->fileBuffer[fileSize], 0, YYJSON_PADDING_SIZE);

    yyjson_alc allocator;
    if (!yyjson_alc_pool_init(&allocator,
                              me->parseArena.bytes,
                              sizeof(me->parseArena.bytes)))
    {
        return ProtocolDecoder_fail_(
            me, PROTOCOL_LOAD_PARSE_FAILED, SIZE_MAX, -1,
            "allocator", "yyjson static allocator initialization failed");
    }

    yyjson_read_err readError;
    yyjson_doc * const document = yyjson_read_opts(
        me->fileBuffer,
        fileSize,
        YYJSON_READ_INSITU | YYJSON_READ_ALLOW_COMMENTS,
        &allocator,
        &readError);
    if (document == (yyjson_doc *)0) {
        me->error.byteOffset = readError.pos;
        (void)yyjson_locate_pos(me->fileBuffer, fileSize,
                                readError.pos,
                                &me->error.line,
                                &me->error.column,
                                (size_t *)0);
        return ProtocolDecoder_fail_(
            me, PROTOCOL_LOAD_PARSE_FAILED, SIZE_MAX, -1,
            "JSON", "%s at byte %zu, line %zu, column %zu",
            readError.msg != (char const *)0
                ? readError.msg : "JSON parse failed",
            readError.pos, me->error.line, me->error.column);
    }

    uint8_t const candidateIndex = (uint8_t)(me->activeTable ^ 1U);
    ProtocolTable * const candidate = &me->tables[candidateIndex];
    memset(candidate, 0, sizeof(*candidate));

    bool const valid = ProtocolDecoder_buildTable_(
        me, candidate, yyjson_doc_get_root(document));
    yyjson_doc_free(document);
    if (!valid) {
        return me->error.result;
    }

    me->activeTable = candidateIndex;
    return PROTOCOL_LOAD_OK;
}

ProtocolRecord const *ProtocolDecoder_record(
    ProtocolDecoder const * const me,
    uint8_t const recId)
{
    DBC_REQUIRE(300, me != (ProtocolDecoder const *)0);
    return &me->tables[me->activeTable].records[recId];
}

ProtocolLoadError const *ProtocolDecoder_error(
    ProtocolDecoder const * const me)
{
    DBC_REQUIRE(400, me != (ProtocolDecoder const *)0);
    return &me->error;
}

//============================================================================
//=== Error context

static void ProtocolDecoder_clearError_(ProtocolDecoder * const me) {
    memset(&me->error, 0, sizeof(me->error));
    me->error.result = PROTOCOL_LOAD_OK;
    me->error.tokenIndex = SIZE_MAX;
    me->error.recId = -1;
}

static ProtocolLoadResult ProtocolDecoder_fail_(
    ProtocolDecoder * const me,
    ProtocolLoadResult const result,
    size_t const tokenIndex,
    int const recId,
    char const * const field,
    char const * const format,
    ...)
{
    me->error.result = result;
    me->error.tokenIndex = tokenIndex;
    me->error.recId = recId;

    if (field != (char const *)0) {
        (void)snprintf(me->error.field, sizeof(me->error.field),
                       "%s", field);
    }

    va_list args;
    va_start(args, format);
    (void)vsnprintf(me->error.detail, sizeof(me->error.detail),
                    format, args);
    va_end(args);
    return result;
}

//============================================================================
//=== File input

static bool ProtocolDecoder_readFile_(
    ProtocolDecoder * const me,
    char const * const path,
    size_t * const fileSize)
{
    FILE * const file = fopen(path, "rb");
    if (file == (FILE *)0) {
        int const errorNumber = errno;
        (void)ProtocolDecoder_fail_(
            me, PROTOCOL_LOAD_OPEN_FAILED, SIZE_MAX, -1,
            "file", "cannot open '%s': %s", path,
            strerror(errorNumber));
        return false;
    }

    bool success = false;
    if (fseek(file, 0L, SEEK_END) != 0) {
        (void)ProtocolDecoder_fail_(
            me, PROTOCOL_LOAD_READ_FAILED, SIZE_MAX, -1,
            "file", "cannot seek to end of '%s'", path);
    } else {
        long const length = ftell(file);
        if ((length <= 0L)
            || ((unsigned long)length
                > (unsigned long)PROTOCOL_DECODER_FILE_MAX_SIZE))
        {
            (void)ProtocolDecoder_fail_(
                me, PROTOCOL_LOAD_FILE_SIZE_INVALID, SIZE_MAX, -1,
                "file", "file size %ld is outside 1..%u bytes",
                length, PROTOCOL_DECODER_FILE_MAX_SIZE);
        } else if (fseek(file, 0L, SEEK_SET) != 0) {
            (void)ProtocolDecoder_fail_(
                me, PROTOCOL_LOAD_READ_FAILED, SIZE_MAX, -1,
                "file", "cannot seek to start of '%s'", path);
        } else {
            *fileSize = (size_t)length;
            if (fread(me->fileBuffer, 1U, *fileSize, file) != *fileSize) {
                (void)ProtocolDecoder_fail_(
                    me, PROTOCOL_LOAD_READ_FAILED, SIZE_MAX, -1,
                    "file", "cannot read %zu bytes from '%s'",
                    *fileSize, path);
            } else {
                success = true;
            }
        }
    }

    if ((fclose(file) != 0) && success) {
        (void)ProtocolDecoder_fail_(
            me, PROTOCOL_LOAD_READ_FAILED, SIZE_MAX, -1,
            "file", "cannot close '%s' after reading", path);
        success = false;
    }
    return success;
}

//============================================================================
//=== Schema conversion

static bool ProtocolDecoder_buildTable_(
    ProtocolDecoder * const me,
    ProtocolTable * const table,
    yyjson_val * const root)
{
    if (!yyjson_is_obj(root)) {
        (void)ProtocolDecoder_fail_(
            me, PROTOCOL_LOAD_SCHEMA_INVALID, SIZE_MAX, -1,
            "root", "root value must be an object");
        return false;
    }
    if (!ProtocolDecoder_objectKeysUnique_(
            me, root, SIZE_MAX, -1))
    {
        return false;
    }

    size_t rootIndex;
    size_t rootMax;
    yyjson_val *rootKey;
    yyjson_val *rootValue;
    yyjson_obj_foreach(root, rootIndex, rootMax, rootKey, rootValue) {
        if (!yyjson_equals_str(rootKey, "RX_tokens")
            && !ProtocolDecoder_valueKeysUnique_(
                me, rootValue, SIZE_MAX, -1))
        {
            return false;
        }
    }

    yyjson_val * const tokens = yyjson_obj_get(root, "RX_tokens");
    if (!yyjson_is_arr(tokens)) {
        (void)ProtocolDecoder_fail_(
            me, PROTOCOL_LOAD_SCHEMA_INVALID, SIZE_MAX, -1,
            "RX_tokens", "root field 'RX_tokens' must be an array");
        return false;
    }

    size_t index;
    size_t max;
    yyjson_val *token;
    yyjson_arr_foreach(tokens, index, max, token) {
        if (!yyjson_is_obj(token)) {
            (void)ProtocolDecoder_fail_(
                me, PROTOCOL_LOAD_SCHEMA_INVALID, index, -1,
                "RX_tokens", "RX_tokens[%zu] must be an object", index);
            return false;
        }

        yyjson_val * const recIdValue = yyjson_obj_get(token, "RecID");
        int const tentativeRecId = yyjson_is_uint(recIdValue)
            && (yyjson_get_uint(recIdValue) <= (uint64_t)INT_MAX)
            ? (int)yyjson_get_uint(recIdValue) : -1;
        if (!ProtocolDecoder_valueKeysUnique_(
                me, token, index, tentativeRecId))
        {
            return false;
        }

        if (!yyjson_is_uint(recIdValue)) {
            (void)ProtocolDecoder_fail_(
                me, PROTOCOL_LOAD_SCHEMA_INVALID, index, -1,
                "RecID", "RX_tokens[%zu].RecID must be an unsigned integer",
                index);
            return false;
        }

        uint64_t const recId = yyjson_get_uint(recIdValue);
        if (recId >= PROTOCOL_DECODER_RECORD_NUM) {
            (void)ProtocolDecoder_fail_(
                me, PROTOCOL_LOAD_SCHEMA_INVALID, index, tentativeRecId,
                "RecID", "RX_tokens[%zu].RecID %llu is outside 0..255",
                index, (unsigned long long)recId);
            return false;
        }
        if (table->records[recId].valid) {
            (void)ProtocolDecoder_fail_(
                me, PROTOCOL_LOAD_SCHEMA_INVALID, index, (int)recId,
                "RecID", "RX_tokens[%zu] duplicates RecID %llu",
                index, (unsigned long long)recId);
            return false;
        }

        ProtocolRecord * const record = &table->records[recId];
        if (!ProtocolDecoder_buildRecord_(
                me, record, token, index, (int)recId))
        {
            return false;
        }
        record->valid = true;
    }
    return true;
}

static bool ProtocolDecoder_buildRecord_(
    ProtocolDecoder * const me,
    ProtocolRecord * const record,
    yyjson_val * const token,
    size_t const tokenIndex,
    int const recId)
{
    yyjson_val * const formatValue = yyjson_obj_get(token, "format");
    yyjson_val * const argsValue = yyjson_obj_get(token, "args");
    if (!yyjson_is_str(formatValue)) {
        (void)ProtocolDecoder_fail_(
            me, PROTOCOL_LOAD_SCHEMA_INVALID, tokenIndex, recId,
            "format", "RX_tokens[%zu] RecID %d format must be a string",
            tokenIndex, recId);
        return false;
    }
    if (!yyjson_is_arr(argsValue)) {
        (void)ProtocolDecoder_fail_(
            me, PROTOCOL_LOAD_SCHEMA_INVALID, tokenIndex, recId,
            "args", "RX_tokens[%zu] RecID %d args must be an array",
            tokenIndex, recId);
        return false;
    }

    char const * const format = yyjson_get_str(formatValue);
    size_t const formatSize = yyjson_get_len(formatValue);
    size_t placeholderCount;
    if (formatSize >= sizeof(record->format)) {
        (void)ProtocolDecoder_fail_(
            me, PROTOCOL_LOAD_SCHEMA_INVALID, tokenIndex, recId,
            "format", "RX_tokens[%zu] RecID %d format exceeds %u bytes",
            tokenIndex, recId,
            PROTOCOL_DECODER_FORMAT_CAPACITY - 1U);
        return false;
    }
    if (strlen(format) != formatSize) {
        (void)ProtocolDecoder_fail_(
            me, PROTOCOL_LOAD_SCHEMA_INVALID, tokenIndex, recId,
            "format", "RX_tokens[%zu] RecID %d format contains NUL",
            tokenIndex, recId);
        return false;
    }
    if (!ProtocolDecoder_formatValid_(format, formatSize,
                                      &placeholderCount))
    {
        (void)ProtocolDecoder_fail_(
            me, PROTOCOL_LOAD_SCHEMA_INVALID, tokenIndex, recId,
            "format", "RX_tokens[%zu] RecID %d format only allows %%s/%%%%",
            tokenIndex, recId);
        return false;
    }

    size_t const argCount = yyjson_arr_size(argsValue);
    if (argCount > PROTOCOL_DECODER_ARG_MAX) {
        (void)ProtocolDecoder_fail_(
            me, PROTOCOL_LOAD_SCHEMA_INVALID, tokenIndex, recId,
            "args", "RX_tokens[%zu] RecID %d has %zu args; maximum is %u",
            tokenIndex, recId, argCount, PROTOCOL_DECODER_ARG_MAX);
        return false;
    }
    if (argCount != placeholderCount) {
        (void)ProtocolDecoder_fail_(
            me, PROTOCOL_LOAD_SCHEMA_INVALID, tokenIndex, recId,
            "args", "RX_tokens[%zu] RecID %d has %zu args but %zu %%s",
            tokenIndex, recId, argCount, placeholderCount);
        return false;
    }

    size_t index;
    size_t max;
    yyjson_val *argValue;
    yyjson_arr_foreach(argsValue, index, max, argValue) {
        if (!yyjson_is_str(argValue)) {
            (void)ProtocolDecoder_fail_(
                me, PROTOCOL_LOAD_SCHEMA_INVALID, tokenIndex, recId,
                "args", "RX_tokens[%zu] RecID %d args[%zu] must be a string",
                tokenIndex, recId, index);
            return false;
        }
        if (!ProtocolDecoder_argType_(
                yyjson_get_str(argValue), yyjson_get_len(argValue),
                &record->args[index]))
        {
            (void)ProtocolDecoder_fail_(
                me, PROTOCOL_LOAD_SCHEMA_INVALID, tokenIndex, recId,
                "args",
                "RX_tokens[%zu] RecID %d args[%zu] type '%.*s' is unknown",
                tokenIndex, recId, index,
                (int)yyjson_get_len(argValue), yyjson_get_str(argValue));
            return false;
        }
        if ((record->args[index] == PROTOCOL_ARG_STRING)
            && ((index + 1U) != argCount))
        {
            (void)ProtocolDecoder_fail_(
                me, PROTOCOL_LOAD_SCHEMA_INVALID, tokenIndex, recId,
                "args", "RX_tokens[%zu] RecID %d STRING must be last",
                tokenIndex, recId);
            return false;
        }
    }

    memcpy(record->format, format, formatSize + 1U);
    record->argCount = (uint8_t)argCount;
    return true;
}

//============================================================================
//=== Duplicate field validation

static bool ProtocolDecoder_valueKeysUnique_(
    ProtocolDecoder * const me,
    yyjson_val * const value,
    size_t const tokenIndex,
    int const recId)
{
    if (yyjson_is_obj(value)) {
        if (!ProtocolDecoder_objectKeysUnique_(
                me, value, tokenIndex, recId))
        {
            return false;
        }

        size_t index;
        size_t max;
        yyjson_val *key;
        yyjson_val *child;
        yyjson_obj_foreach(value, index, max, key, child) {
            if (!ProtocolDecoder_valueKeysUnique_(
                    me, child, tokenIndex, recId))
            {
                return false;
            }
        }
    } else if (yyjson_is_arr(value)) {
        size_t index;
        size_t max;
        yyjson_val *child;
        yyjson_arr_foreach(value, index, max, child) {
            if (!ProtocolDecoder_valueKeysUnique_(
                    me, child, tokenIndex, recId))
            {
                return false;
            }
        }
    }
    return true;
}

static bool ProtocolDecoder_objectKeysUnique_(
    ProtocolDecoder * const me,
    yyjson_val * const object,
    size_t const tokenIndex,
    int const recId)
{
    size_t outerIndex;
    size_t outerMax;
    yyjson_val *outerKey;
    yyjson_val *outerValue;
    yyjson_obj_foreach(object, outerIndex, outerMax,
                       outerKey, outerValue)
    {
        (void)outerValue;
        size_t matches = 0U;
        size_t innerIndex;
        size_t innerMax;
        yyjson_val *innerKey;
        yyjson_val *innerValue;
        yyjson_obj_foreach(object, innerIndex, innerMax,
                           innerKey, innerValue)
        {
            (void)innerValue;
            if ((yyjson_get_len(outerKey) == yyjson_get_len(innerKey))
                && (memcmp(yyjson_get_str(outerKey),
                           yyjson_get_str(innerKey),
                           yyjson_get_len(outerKey)) == 0))
            {
                ++matches;
            }
        }

        if (matches > 1U) {
            size_t const keySize = yyjson_get_len(outerKey);
            char field[PROTOCOL_DECODER_ERROR_FIELD_CAPACITY];
            (void)snprintf(field, sizeof(field),
                           "%.*s", (int)keySize,
                           yyjson_get_str(outerKey));
            if (tokenIndex != SIZE_MAX) {
                (void)ProtocolDecoder_fail_(
                    me, PROTOCOL_LOAD_SCHEMA_INVALID,
                    tokenIndex, recId, field,
                    "RX_tokens[%zu] RecID %d has duplicate field '%s'",
                    tokenIndex, recId, field);
            } else {
                (void)ProtocolDecoder_fail_(
                    me, PROTOCOL_LOAD_SCHEMA_INVALID,
                    tokenIndex, recId, field,
                    "object has duplicate field '%s'", field);
            }
            return false;
        }
    }
    return true;
}

//============================================================================
//=== Record rules

static bool ProtocolDecoder_argType_(
    char const * const name,
    size_t const nameSize,
    ProtocolArgType * const type)
{
    for (size_t i = 0U;
         i < (sizeof(l_argNames_) / sizeof(l_argNames_[0]));
         ++i)
    {
        size_t const candidateSize = strlen(l_argNames_[i].name);
        if ((nameSize == candidateSize)
            && (memcmp(name, l_argNames_[i].name, nameSize) == 0))
        {
            *type = l_argNames_[i].type;
            return true;
        }
    }
    return false;
}

static bool ProtocolDecoder_formatValid_(
    char const * const format,
    size_t const formatSize,
    size_t * const placeholderCount)
{
    *placeholderCount = 0U;
    for (size_t i = 0U; i < formatSize; ++i) {
        if (format[i] == '%') {
            ++i;
            if (i >= formatSize) {
                return false;
            } else if (format[i] == 's') {
                ++(*placeholderCount);
            } else if (format[i] != '%') {
                return false;
            }
        }
    }
    return true;
}
