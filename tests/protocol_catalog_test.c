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
#include <stdio.h>
#include <string.h>
#if defined(_WIN32)
#include <direct.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif
#include "sp_mngr/protocol_catalog_priv.h"

static int makeDirectory_(char const * const path) {
#if defined(_WIN32)
    int const result = _mkdir(path);
#else
    int const result = mkdir(path, 0700);
#endif
    return (result == 0) || (errno == EEXIST) ? 0 : 1;
}

static int removeDirectory_(char const * const path) {
#if defined(_WIN32)
    return _rmdir(path);
#else
    return rmdir(path);
#endif
}

static int writeEmptyFile_(char const * const path) {
    FILE * const file = fopen(path, "wb");
    if (file == (FILE *)0) {
        return 1;
    }
    return fclose(file) == 0 ? 0 : 1;
}

static int findEntry_(ProtocolCatalog const * const catalog,
                      char const * const relativePath)
{
    for (size_t i = 0U; i < ProtocolCatalog_count(catalog); ++i) {
        if (strcmp(ProtocolCatalog_relativePath(catalog, i),
                   relativePath) == 0)
        {
            return 0;
        }
    }
    return 1;
}

int main(int const argc, char const ** const argv) {
    if (argc != 2) {
        return 1;
    }

    int failed = 0;
    static ProtocolCatalog catalog;
    ProtocolCatalog_ctor(&catalog);

    ProtocolCatalogResult const result = ProtocolCatalog_scan(&catalog);
    if (result != PROTOCOL_CATALOG_OK) {
        fprintf(stderr, "scan failed: %s\n",
                ProtocolCatalog_error(&catalog)->detail);
        ++failed;
    }
    if (strcmp(ProtocolCatalog_root(&catalog), argv[1]) != 0) {
        fprintf(stderr, "root: expected '%s', got '%s'\n",
                argv[1], ProtocolCatalog_root(&catalog));
        ++failed;
    }
    if (ProtocolCatalog_count(&catalog) != 1U) {
        fprintf(stderr, "count: expected 1, got %zu\n",
                ProtocolCatalog_count(&catalog));
        ++failed;
    }
    if ((ProtocolCatalog_count(&catalog) > 0U)
        && (strcmp(ProtocolCatalog_relativePath(&catalog, 0U),
                   "blinky_c51.json") != 0))
    {
        fprintf(stderr, "entry: expected 'blinky_c51.json', got '%s'\n",
                ProtocolCatalog_relativePath(&catalog, 0U));
        ++failed;
    }

    char path[PROTOCOL_CATALOG_FILE_PATH_CAPACITY];
    failed += ProtocolCatalog_makePath(
        &catalog, 0U, path, sizeof(path)) ? 0 : 1;
    char expected[PROTOCOL_CATALOG_FILE_PATH_CAPACITY];
    int const written = snprintf(expected, sizeof(expected),
                                 "%s/blinky_c51.json", argv[1]);
    failed += (written > 0)
              && ((size_t)written < sizeof(expected)) ? 0 : 1;
    failed += strcmp(path, expected) == 0 ? 0 : 1;
    failed += ProtocolCatalog_resolvePath(
        &catalog, "blinky_c51.json", path, sizeof(path))
        == PROTOCOL_CATALOG_OK ? 0 : 1;
    failed += strcmp(path, expected) == 0 ? 0 : 1;
    failed += ProtocolCatalog_resolvePath(
        &catalog, "missing.json", path, sizeof(path))
        == PROTOCOL_CATALOG_ENTRY_NOT_FOUND ? 0 : 1;

    char directories[9][PROTOCOL_CATALOG_ROOT_CAPACITY];
    char relative[PROTOCOL_CATALOG_PATH_CAPACITY] = "catalog_depth_1";
    for (size_t i = 0U; i < 9U; ++i) {
        if (i > 0U) {
            size_t const used = strlen(relative);
            int const length = snprintf(
                &relative[used], sizeof(relative) - used,
                "/catalog_depth_%zu", i + 1U);
            failed += (length > 0)
                      && ((size_t)length < (sizeof(relative) - used))
                      ? 0 : 1;
        }
        int const length = snprintf(
            directories[i], sizeof(directories[i]),
            "%s/%s", argv[1], relative);
        failed += (length > 0)
                  && ((size_t)length < sizeof(directories[i]))
                  ? 0 : 1;
        failed += makeDirectory_(directories[i]);
    }

    char depth8File[PROTOCOL_CATALOG_ROOT_CAPACITY
                    + PROTOCOL_CATALOG_PATH_CAPACITY];
    char depth9File[PROTOCOL_CATALOG_ROOT_CAPACITY
                    + PROTOCOL_CATALOG_PATH_CAPACITY];
    (void)snprintf(depth8File, sizeof(depth8File),
                   "%s/depth8.json", directories[7]);
    (void)snprintf(depth9File, sizeof(depth9File),
                   "%s/depth9.json", directories[8]);
    failed += writeEmptyFile_(depth8File);
    failed += writeEmptyFile_(depth9File);

    if (ProtocolCatalog_scan(&catalog) != PROTOCOL_CATALOG_OK) {
        fprintf(stderr, "depth scan failed: %s\n",
                ProtocolCatalog_error(&catalog)->detail);
        ++failed;
    }
    failed += ProtocolCatalog_count(&catalog) == 2U ? 0 : 1;
    failed += findEntry_(
        &catalog,
        "catalog_depth_1/catalog_depth_2/catalog_depth_3/"
        "catalog_depth_4/catalog_depth_5/catalog_depth_6/"
        "catalog_depth_7/catalog_depth_8/depth8.json");
    failed += findEntry_(
        &catalog,
        "catalog_depth_1/catalog_depth_2/catalog_depth_3/"
        "catalog_depth_4/catalog_depth_5/catalog_depth_6/"
        "catalog_depth_7/catalog_depth_8/catalog_depth_9/depth9.json")
        == 1 ? 0 : 1;

    (void)remove(depth8File);
    (void)remove(depth9File);
    for (size_t i = 9U; i > 0U; --i) {
        (void)removeDirectory_(directories[i - 1U]);
    }

    return failed == 0 ? 0 : 1;
}
