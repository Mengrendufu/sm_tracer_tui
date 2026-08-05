//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#include <stdio.h>
#include <string.h>
#include "sp_mngr/protocol_decoder_priv.h"

static int writeFile_(char const * const path,
                      char const * const content)
{
    FILE * const file = fopen(path, "wb");
    if (file == (FILE *)0) {
        return 1;
    }

    size_t const size = strlen(content);
    int const failed = fwrite(content, 1U, size, file) != size;
    return fclose(file) == 0 ? failed : 1;
}

int main(int const argc, char const ** const argv) {
    if (argc != 2) {
        return 1;
    }

    int failed = 0;
    static ProtocolDecoder decoder;
    ProtocolDecoder_ctor(&decoder);

    failed += ProtocolDecoder_loadFile(&decoder, argv[1])
              == PROTOCOL_LOAD_OK ? 0 : 1;

    ProtocolRecord const *record =
        ProtocolDecoder_record(&decoder, 0U);
    failed += record->valid ? 0 : 1;
    failed += strcmp(record->format,
                     "==[ASSERTION]==[Label][%s]==[Module][%s]==\n")
              == 0 ? 0 : 1;
    failed += record->argCount == 2U ? 0 : 1;
    failed += record->args[0] == PROTOCOL_ARG_INT32 ? 0 : 1;
    failed += record->args[1] == PROTOCOL_ARG_STRING ? 0 : 1;

    record = ProtocolDecoder_record(&decoder, 11U);
    failed += record->valid ? 0 : 1;
    failed += strcmp(record->format, "==ledOn ==\n") == 0 ? 0 : 1;
    failed += record->argCount == 0U ? 0 : 1;
    failed += !ProtocolDecoder_record(&decoder, 2U)->valid ? 0 : 1;

    char const invalidPath[] = "protocol_decoder_invalid.json";
    char const invalidJson[] =
        "{\"RX_tokens\":["
        "{\"RecID\":1,\"format\":\"one\",\"args\":[]},"
        "{\"RecID\":1,\"format\":\"two\",\"args\":[]}]}";
    failed += writeFile_(invalidPath, invalidJson);
    failed += ProtocolDecoder_loadFile(&decoder, invalidPath)
              == PROTOCOL_LOAD_SCHEMA_INVALID ? 0 : 1;
    (void)remove(invalidPath);

    record = ProtocolDecoder_record(&decoder, 11U);
    failed += record->valid ? 0 : 1;
    failed += strcmp(record->format, "==ledOn ==\n") == 0 ? 0 : 1;

    char const duplicateFieldPath[] =
        "protocol_decoder_duplicate_field.json";
    char const duplicateFieldJson[] =
        "{\"RX_tokens\":[{\"RecID\":1,"
        "\"format\":\"one\",\"format\":\"two\",\"args\":[]}]}";
    failed += writeFile_(duplicateFieldPath, duplicateFieldJson);
    failed += ProtocolDecoder_loadFile(&decoder, duplicateFieldPath)
              == PROTOCOL_LOAD_SCHEMA_INVALID ? 0 : 1;
    ProtocolLoadError const *error = ProtocolDecoder_error(&decoder);
    failed += error->result == PROTOCOL_LOAD_SCHEMA_INVALID ? 0 : 1;
    failed += error->tokenIndex == 0U ? 0 : 1;
    failed += error->recId == 1 ? 0 : 1;
    failed += strcmp(error->field, "format") == 0 ? 0 : 1;
    failed += strstr(error->detail, "duplicate field")
              != (char const *)0 ? 0 : 1;
    (void)remove(duplicateFieldPath);

    char const syntaxPath[] = "protocol_decoder_syntax.json";
    char const syntaxJson[] =
        "{\n"
        "  \"RX_tokens\": [\n"
        "    {\"RecID\": 1,}\n"
        "  ]\n"
        "}\n";
    failed += writeFile_(syntaxPath, syntaxJson);
    failed += ProtocolDecoder_loadFile(&decoder, syntaxPath)
              == PROTOCOL_LOAD_PARSE_FAILED ? 0 : 1;
    error = ProtocolDecoder_error(&decoder);
    failed += error->result == PROTOCOL_LOAD_PARSE_FAILED ? 0 : 1;
    failed += error->line == 3U ? 0 : 1;
    failed += error->column > 0U ? 0 : 1;
    failed += error->detail[0] != '\0' ? 0 : 1;
    (void)remove(syntaxPath);

    char const oversizedPath[] = "protocol_decoder_oversized.json";
    FILE * const oversized = fopen(oversizedPath, "wb");
    if (oversized == (FILE *)0) {
        return 1;
    }
    failed += fseek(oversized,
                    (long)PROTOCOL_DECODER_FILE_MAX_SIZE,
                    SEEK_SET) == 0 ? 0 : 1;
    failed += fputc('x', oversized) == 'x' ? 0 : 1;
    failed += fclose(oversized) == 0 ? 0 : 1;
    failed += ProtocolDecoder_loadFile(&decoder, oversizedPath)
              == PROTOCOL_LOAD_FILE_SIZE_INVALID ? 0 : 1;
    (void)remove(oversizedPath);

    record = ProtocolDecoder_record(&decoder, 11U);
    failed += record->valid ? 0 : 1;
    failed += strcmp(record->format, "==ledOn ==\n") == 0 ? 0 : 1;

    return failed == 0 ? 0 : 1;
}
