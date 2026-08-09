//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "dbc_assert.h"
#include "filesystem_platform.h"
#include "protocol_catalog_location.h"
#include "protocol_catalog_priv.h"

//============================================================================
//=== Component: ProtocolCatalog

DBC_MODULE_NAME("protocol_catalog")

typedef struct {
    ProtocolCatalog *catalog;
    char const *relativeDir;
    uint8_t catalogIndex;
    unsigned depth;
    bool valid;
} ProtocolCatalogScanContext;

static void ProtocolCatalog_clearError_(ProtocolCatalog *me);
static ProtocolCatalogResult ProtocolCatalog_fail_(
    ProtocolCatalog *me,
    ProtocolCatalogResult result,
    char const *format,
    ...);
static bool ProtocolCatalog_resolveRoot_(ProtocolCatalog *me);
static bool ProtocolCatalog_rootValid_(ProtocolCatalog *me);
static bool ProtocolCatalog_scanDir_(ProtocolCatalog *me,
                                     uint8_t catalogIndex,
                                     char const *relativeDir,
                                     unsigned depth);
static bool ProtocolCatalog_visitEntry_(
    void *ctx,
    FilesystemPlatformEntry const *entry);
static bool ProtocolCatalog_add_(ProtocolCatalog *me,
                                 uint8_t catalogIndex,
                                 char const *relativePath);
static bool ProtocolCatalog_isJson_(char const *name);
static bool ProtocolCatalog_join_(char *path,
                                  size_t capacity,
                                  char const *left,
                                  char const *right);
static void ProtocolCatalog_sort_(ProtocolCatalog *me,
                                  uint8_t catalogIndex);

void ProtocolCatalog_ctor(ProtocolCatalog * const me) {
    DBC_REQUIRE(100, me != (ProtocolCatalog *)0);

    memset(me, 0, sizeof(*me));
    ProtocolCatalog_clearError_(me);
}

ProtocolCatalogResult ProtocolCatalog_scan(ProtocolCatalog * const me) {
    DBC_REQUIRE(200, me != (ProtocolCatalog *)0);

    ProtocolCatalog_clearError_(me);
    if (!ProtocolCatalog_resolveRoot_(me)
        || !ProtocolCatalog_rootValid_(me))
    {
        return me->error.result;
    }

    uint8_t const candidate = (uint8_t)(me->activeCatalog ^ 1U);
    me->counts[candidate] = 0U;
    memset(me->entries[candidate], 0, sizeof(me->entries[candidate]));

    if (!ProtocolCatalog_scanDir_(me, candidate, "", 0U)) {
        return me->error.result;
    }

    ProtocolCatalog_sort_(me, candidate);
    (void)snprintf(me->root, sizeof(me->root), "%s", me->scanRoot);
    me->activeCatalog = candidate;
    return PROTOCOL_CATALOG_OK;
}

size_t ProtocolCatalog_count(ProtocolCatalog const * const me) {
    DBC_REQUIRE(300, me != (ProtocolCatalog const *)0);
    return me->counts[me->activeCatalog];
}

char const *ProtocolCatalog_relativePath(
    ProtocolCatalog const * const me,
    size_t const index)
{
    DBC_REQUIRE(400, me != (ProtocolCatalog const *)0);
    DBC_REQUIRE(401, index < me->counts[me->activeCatalog]);
    return me->entries[me->activeCatalog][index].relativePath;
}

char const *ProtocolCatalog_root(ProtocolCatalog const * const me) {
    DBC_REQUIRE(500, me != (ProtocolCatalog const *)0);
    return me->root;
}

bool ProtocolCatalog_makePath(ProtocolCatalog const * const me,
                              size_t const index,
                              char * const path,
                              size_t const capacity)
{
    DBC_REQUIRE(600, me != (ProtocolCatalog const *)0);
    DBC_REQUIRE(601, index < me->counts[me->activeCatalog]);
    DBC_REQUIRE(602, path != (char *)0);
    DBC_REQUIRE(603, capacity > 0U);

    return ProtocolCatalog_join_(
        path, capacity, me->root,
        me->entries[me->activeCatalog][index].relativePath);
}

ProtocolCatalogResult ProtocolCatalog_resolvePath(
    ProtocolCatalog * const me,
    char const * const relativePath,
    char * const filePath,
    size_t const capacity)
{
    DBC_REQUIRE(650, me != (ProtocolCatalog *)0);
    DBC_REQUIRE(651, relativePath != (char const *)0);
    DBC_REQUIRE(652, filePath != (char *)0);
    DBC_REQUIRE(653, capacity > 0U);

    ProtocolCatalog_clearError_(me);
    size_t const count = me->counts[me->activeCatalog];
    for (size_t i = 0U; i < count; ++i) {
        if (strcmp(me->entries[me->activeCatalog][i].relativePath,
                   relativePath) == 0)
        {
            if (!ProtocolCatalog_join_(filePath, capacity,
                                       me->root, relativePath))
            {
                return ProtocolCatalog_fail_(
                    me, PROTOCOL_CATALOG_PATH_TOO_LONG,
                    "resolved protocol file path is too long");
            }
            return PROTOCOL_CATALOG_OK;
        }
    }

    return ProtocolCatalog_fail_(
        me, PROTOCOL_CATALOG_ENTRY_NOT_FOUND,
        "protocol catalog entry was not found: %s", relativePath);
}

ProtocolCatalogError const *ProtocolCatalog_error(
    ProtocolCatalog const * const me)
{
    DBC_REQUIRE(700, me != (ProtocolCatalog const *)0);
    return &me->error;
}

//============================================================================
//=== Common catalog operations

static void ProtocolCatalog_clearError_(ProtocolCatalog * const me) {
    memset(&me->error, 0, sizeof(me->error));
    me->error.result = PROTOCOL_CATALOG_OK;
}

static ProtocolCatalogResult ProtocolCatalog_fail_(
    ProtocolCatalog * const me,
    ProtocolCatalogResult const result,
    char const * const format,
    ...)
{
    me->error.result = result;

    va_list args;
    va_start(args, format);
    (void)vsnprintf(me->error.detail, sizeof(me->error.detail),
                    format, args);
    va_end(args);
    return result;
}

static bool ProtocolCatalog_add_(ProtocolCatalog * const me,
                                 uint8_t const catalogIndex,
                                 char const * const relativePath)
{
    size_t * const count = &me->counts[catalogIndex];
    if (*count >= PROTOCOL_CATALOG_MAX_FILES) {
        (void)ProtocolCatalog_fail_(
            me, PROTOCOL_CATALOG_TOO_MANY_FILES,
            "protocol catalog exceeds %u files",
            PROTOCOL_CATALOG_MAX_FILES);
        return false;
    }

    size_t const size = strlen(relativePath);
    if (size >= PROTOCOL_CATALOG_PATH_CAPACITY) {
        (void)ProtocolCatalog_fail_(
            me, PROTOCOL_CATALOG_PATH_TOO_LONG,
            "protocol relative path exceeds %u bytes",
            PROTOCOL_CATALOG_PATH_CAPACITY - 1U);
        return false;
    }

    memcpy(me->entries[catalogIndex][*count].relativePath,
           relativePath, size + 1U);
    ++(*count);
    return true;
}

static bool ProtocolCatalog_isJson_(char const * const name) {
    size_t const size = strlen(name);
    if (size < 5U) {
        return false;
    }

    char const * const extension = &name[size - 5U];
    return (extension[0] == '.')
        && (tolower((unsigned char)extension[1]) == 'j')
        && (tolower((unsigned char)extension[2]) == 's')
        && (tolower((unsigned char)extension[3]) == 'o')
        && (tolower((unsigned char)extension[4]) == 'n');
}

static bool ProtocolCatalog_join_(char * const path,
                                  size_t const capacity,
                                  char const * const left,
                                  char const * const right)
{
    int written;
    if (right[0] == '\0') {
        written = snprintf(path, capacity, "%s", left);
    } else if (left[0] == '\0') {
        written = snprintf(path, capacity, "%s", right);
    } else {
        written = snprintf(path, capacity, "%s/%s", left, right);
    }
    return (written >= 0) && ((size_t)written < capacity);
}

static void ProtocolCatalog_sort_(ProtocolCatalog * const me,
                                  uint8_t const catalogIndex)
{
    ProtocolCatalogEntry * const entries = me->entries[catalogIndex];
    size_t const count = me->counts[catalogIndex];

    for (size_t i = 1U; i < count; ++i) {
        ProtocolCatalogEntry const entry = entries[i];
        size_t j = i;
        while ((j > 0U)
               && (strcmp(entries[j - 1U].relativePath,
                          entry.relativePath) > 0))
        {
            entries[j] = entries[j - 1U];
            --j;
        }
        entries[j] = entry;
    }
}

//============================================================================
//=== Catalog location and directory-access boundaries

static bool ProtocolCatalog_resolveRoot_(ProtocolCatalog * const me) {
    char detail[PROTOCOL_CATALOG_ERROR_CAPACITY];
    ProtocolCatalogLocationResult const result =
        ProtocolCatalogLocation_resolveRoot(
            me->scanRoot, sizeof(me->scanRoot),
            detail, sizeof(detail));
    if (result == PROTOCOL_CATALOG_LOCATION_OK) {
        return true;
    }

    ProtocolCatalogResult const catalogResult =
        result == PROTOCOL_CATALOG_LOCATION_PATH_TOO_LONG
        ? PROTOCOL_CATALOG_PATH_TOO_LONG
        : PROTOCOL_CATALOG_EXECUTABLE_PATH_FAILED;
    (void)ProtocolCatalog_fail_(me, catalogResult, "%s", detail);
    return false;
}

static bool ProtocolCatalog_rootValid_(ProtocolCatalog * const me) {
    char detail[PROTOCOL_CATALOG_ERROR_CAPACITY];
    FilesystemPlatformEntryKind kind = FILESYSTEM_PLATFORM_ENTRY_OTHER;
    FilesystemPlatformResult const result = FilesystemPlatform_pathKind(
        me->scanRoot, &kind, detail, sizeof(detail));
    if ((result == FILESYSTEM_PLATFORM_OK)
        && (kind == FILESYSTEM_PLATFORM_ENTRY_DIRECTORY))
    {
        return true;
    }

    if (detail[0] != '\0') {
        (void)ProtocolCatalog_fail_(
            me, PROTOCOL_CATALOG_ROOT_NOT_FOUND,
            "protocol root is missing or is not a physical directory: %s",
            detail);
    } else {
        (void)ProtocolCatalog_fail_(
            me, PROTOCOL_CATALOG_ROOT_NOT_FOUND,
            "protocol root is missing or is not a physical directory");
    }
    return false;
}

static bool ProtocolCatalog_scanDir_(ProtocolCatalog * const me,
                                     uint8_t const catalogIndex,
                                     char const * const relativeDir,
                                     unsigned const depth)
{
    char directory[PROTOCOL_CATALOG_FILE_PATH_CAPACITY];
    if (!ProtocolCatalog_join_(directory, sizeof(directory),
                               me->scanRoot, relativeDir))
    {
        (void)ProtocolCatalog_fail_(
            me, PROTOCOL_CATALOG_PATH_TOO_LONG,
            "protocol directory path is too long");
        return false;
    }

    ProtocolCatalogScanContext context = {
        .catalog = me,
        .relativeDir = relativeDir,
        .catalogIndex = catalogIndex,
        .depth = depth,
        .valid = true
    };
    char detail[PROTOCOL_CATALOG_ERROR_CAPACITY];
    FilesystemPlatformResult const result =
        FilesystemPlatform_enumerateDirectory(
            directory, &ProtocolCatalog_visitEntry_, &context,
            detail, sizeof(detail));
    if (!context.valid) {
        return false;
    }
    if (result != FILESYSTEM_PLATFORM_OK) {
        (void)ProtocolCatalog_fail_(
            me, PROTOCOL_CATALOG_SCAN_FAILED,
            "cannot scan protocol directory: %s", detail);
        return false;
    }
    return true;
}

static bool ProtocolCatalog_visitEntry_(
    void * const ctx,
    FilesystemPlatformEntry const * const entry)
{
    ProtocolCatalogScanContext * const context =
        (ProtocolCatalogScanContext *)ctx;
    if ((entry->kind == FILESYSTEM_PLATFORM_ENTRY_LINK)
        || (entry->kind == FILESYSTEM_PLATFORM_ENTRY_OTHER))
    {
        return true;
    }

    char relativePath[PROTOCOL_CATALOG_PATH_CAPACITY];
    if (!ProtocolCatalog_join_(relativePath, sizeof(relativePath),
                               context->relativeDir, entry->name))
    {
        (void)ProtocolCatalog_fail_(
            context->catalog, PROTOCOL_CATALOG_PATH_TOO_LONG,
            "protocol relative path is too long");
        context->valid = false;
        return false;
    }

    if (entry->kind == FILESYSTEM_PLATFORM_ENTRY_DIRECTORY) {
        if ((context->depth < PROTOCOL_CATALOG_MAX_DEPTH)
            && !ProtocolCatalog_scanDir_(
                context->catalog, context->catalogIndex,
                relativePath, context->depth + 1U))
        {
            context->valid = false;
        }
    } else if (ProtocolCatalog_isJson_(entry->name)
               && !ProtocolCatalog_add_(
                   context->catalog, context->catalogIndex, relativePath))
    {
        context->valid = false;
    }
    return context->valid;
}
