//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#ifndef PROTOCOL_CATALOG_PRIV_H_
#define PROTOCOL_CATALOG_PRIV_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PROTOCOL_CATALOG_MAX_FILES       128U
#define PROTOCOL_CATALOG_MAX_DEPTH       8U
#define PROTOCOL_CATALOG_ROOT_CAPACITY   4096U
#define PROTOCOL_CATALOG_PATH_CAPACITY   240U
#define PROTOCOL_CATALOG_FILE_PATH_CAPACITY \
    (PROTOCOL_CATALOG_ROOT_CAPACITY + PROTOCOL_CATALOG_PATH_CAPACITY)
#define PROTOCOL_CATALOG_ERROR_CAPACITY  192U

typedef struct {
    char relativePath[PROTOCOL_CATALOG_PATH_CAPACITY];
} ProtocolCatalogEntry;

typedef enum {
    PROTOCOL_CATALOG_OK,
    PROTOCOL_CATALOG_EXECUTABLE_PATH_FAILED,
    PROTOCOL_CATALOG_ROOT_NOT_FOUND,
    PROTOCOL_CATALOG_SCAN_FAILED,
    PROTOCOL_CATALOG_TOO_MANY_FILES,
    PROTOCOL_CATALOG_PATH_TOO_LONG,
    PROTOCOL_CATALOG_ENTRY_NOT_FOUND
} ProtocolCatalogResult;

typedef struct {
    ProtocolCatalogResult result;
    char detail[PROTOCOL_CATALOG_ERROR_CAPACITY];
} ProtocolCatalogError;

// Transactional snapshot of JSON paths relative to the real executable's
// sibling protocols/ directory. Failed scans preserve the active snapshot.
typedef struct {
    ProtocolCatalogEntry entries[2][PROTOCOL_CATALOG_MAX_FILES];
    size_t counts[2];
    char root[PROTOCOL_CATALOG_ROOT_CAPACITY];
    char scanRoot[PROTOCOL_CATALOG_ROOT_CAPACITY];
    ProtocolCatalogError error;
    uint8_t activeCatalog;
} ProtocolCatalog;

void ProtocolCatalog_ctor(ProtocolCatalog *me);
ProtocolCatalogResult ProtocolCatalog_scan(ProtocolCatalog *me);
size_t ProtocolCatalog_count(ProtocolCatalog const *me);
char const *ProtocolCatalog_relativePath(ProtocolCatalog const *me,
                                         size_t index);
char const *ProtocolCatalog_root(ProtocolCatalog const *me);
bool ProtocolCatalog_makePath(ProtocolCatalog const *me,
                              size_t index,
                              char *path,
                              size_t capacity);
ProtocolCatalogResult ProtocolCatalog_resolvePath(
    ProtocolCatalog *me,
    char const *relativePath,
    char *filePath,
    size_t capacity);
ProtocolCatalogError const *ProtocolCatalog_error(
    ProtocolCatalog const *me);

#endif // PROTOCOL_CATALOG_PRIV_H_
