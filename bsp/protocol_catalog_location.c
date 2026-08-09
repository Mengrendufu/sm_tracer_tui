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
#include "filesystem_platform.h"
#include "protocol_catalog_location.h"

//============================================================================
//=== Component: ProtocolCatalogLocation

ProtocolCatalogLocationResult ProtocolCatalogLocation_resolveRoot(
    char * const path,
    size_t const capacity,
    char * const detail,
    size_t const detailCapacity)
{
    FilesystemPlatformResult const platformResult =
        FilesystemPlatform_executableDirectory(
            path, capacity, detail, detailCapacity);
    if (platformResult != FILESYSTEM_PLATFORM_OK) {
        return platformResult == FILESYSTEM_PLATFORM_PATH_TOO_LONG
            ? PROTOCOL_CATALOG_LOCATION_PATH_TOO_LONG
            : PROTOCOL_CATALOG_LOCATION_EXECUTABLE_PATH_FAILED;
    }

    size_t const length = strlen(path);
    bool const needsSeparator = (length == 0U)
        || (path[length - 1U] != '/');
    char const suffix[] = "protocols";
    size_t const required = length + (needsSeparator ? 1U : 0U)
        + sizeof(suffix);
    if (required > capacity) {
        if (detailCapacity > 0U) {
            (void)snprintf(detail, detailCapacity,
                           "protocol root path is too long");
        }
        return PROTOCOL_CATALOG_LOCATION_PATH_TOO_LONG;
    }

    size_t offset = length;
    if (needsSeparator) {
        path[offset++] = '/';
    }
    memcpy(&path[offset], suffix, sizeof(suffix));
    if (detailCapacity > 0U) {
        detail[0] = '\0';
    }
    return PROTOCOL_CATALOG_LOCATION_OK;
}
