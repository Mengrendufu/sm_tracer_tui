//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#ifndef PROTOCOL_CATALOG_LOCATION_H_
#define PROTOCOL_CATALOG_LOCATION_H_

#include <stddef.h>

//============================================================================
//=== Component: ProtocolCatalogLocation
//=== Interface: IProtocolCatalogLocation

typedef enum {
    PROTOCOL_CATALOG_LOCATION_OK,
    PROTOCOL_CATALOG_LOCATION_EXECUTABLE_PATH_FAILED,
    PROTOCOL_CATALOG_LOCATION_PATH_TOO_LONG
} ProtocolCatalogLocationResult;

ProtocolCatalogLocationResult ProtocolCatalogLocation_resolveRoot(
    char *path,
    size_t capacity,
    char *detail,
    size_t detailCapacity);

#endif // PROTOCOL_CATALOG_LOCATION_H_
