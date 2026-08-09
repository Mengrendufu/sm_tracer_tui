//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#ifndef FILESYSTEM_PLATFORM_H_
#define FILESYSTEM_PLATFORM_H_

#include <stdbool.h>
#include <stddef.h>

//============================================================================
//=== Component: FilesystemPlatform
//=== Interfaces: IExecutableLocation, IDirectoryAccess

typedef enum {
    FILESYSTEM_PLATFORM_OK,
    FILESYSTEM_PLATFORM_NOT_FOUND,
    FILESYSTEM_PLATFORM_PATH_TOO_LONG,
    FILESYSTEM_PLATFORM_INVALID_ENCODING,
    FILESYSTEM_PLATFORM_IO_ERROR
} FilesystemPlatformResult;

typedef enum {
    FILESYSTEM_PLATFORM_ENTRY_FILE,
    FILESYSTEM_PLATFORM_ENTRY_DIRECTORY,
    FILESYSTEM_PLATFORM_ENTRY_LINK,
    FILESYSTEM_PLATFORM_ENTRY_OTHER
} FilesystemPlatformEntryKind;

// The entry name is UTF-8 and remains valid only for the visitor call.
typedef struct {
    char const *name;
    FilesystemPlatformEntryKind kind;
} FilesystemPlatformEntry;

// Returning false stops enumeration without turning it into a platform error.
typedef bool (*FilesystemPlatformVisitor)(
    void *ctx,
    FilesystemPlatformEntry const *entry);

FilesystemPlatformResult FilesystemPlatform_executableDirectory(
    char *path,
    size_t capacity,
    char *detail,
    size_t detailCapacity);

FilesystemPlatformResult FilesystemPlatform_pathKind(
    char const *path,
    FilesystemPlatformEntryKind *kind,
    char *detail,
    size_t detailCapacity);

FilesystemPlatformResult FilesystemPlatform_enumerateDirectory(
    char const *path,
    FilesystemPlatformVisitor visitor,
    void *ctx,
    char *detail,
    size_t detailCapacity);

#endif // FILESYSTEM_PLATFORM_H_
