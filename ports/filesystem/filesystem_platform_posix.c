//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "filesystem_platform.h"

//============================================================================
//=== Component: FilesystemPlatform (POSIX)

static FilesystemPlatformResult FilesystemPlatform_fail_(
    FilesystemPlatformResult const result,
    char * const detail,
    size_t const detailCapacity,
    char const * const format,
    ...)
{
    if (detailCapacity > 0U) {
        va_list args;
        va_start(args, format);
        (void)vsnprintf(detail, detailCapacity, format, args);
        va_end(args);
    }
    return result;
}

static void FilesystemPlatform_clearDetail_(char * const detail,
                                             size_t const capacity)
{
    if (capacity > 0U) {
        detail[0] = '\0';
    }
}

static FilesystemPlatformEntryKind FilesystemPlatform_kind_(
    mode_t const mode)
{
    if (S_ISLNK(mode)) {
        return FILESYSTEM_PLATFORM_ENTRY_LINK;
    } else if (S_ISDIR(mode)) {
        return FILESYSTEM_PLATFORM_ENTRY_DIRECTORY;
    } else if (S_ISREG(mode)) {
        return FILESYSTEM_PLATFORM_ENTRY_FILE;
    } else {
        return FILESYSTEM_PLATFORM_ENTRY_OTHER;
    }
}

FilesystemPlatformResult FilesystemPlatform_executableDirectory(
    char * const path,
    size_t const capacity,
    char * const detail,
    size_t const detailCapacity)
{
    FilesystemPlatform_clearDetail_(detail, detailCapacity);
    if (capacity < 2U) {
        return FilesystemPlatform_fail_(
            FILESYSTEM_PLATFORM_PATH_TOO_LONG, detail, detailCapacity,
            "executable directory buffer is too small");
    }

    ssize_t const size = readlink(
        "/proc/self/exe", path, capacity - 1U);
    if (size < 0) {
        return FilesystemPlatform_fail_(
            FILESYSTEM_PLATFORM_IO_ERROR, detail, detailCapacity,
            "cannot resolve /proc/self/exe: %s", strerror(errno));
    }
    if ((size_t)size >= (capacity - 1U)) {
        return FilesystemPlatform_fail_(
            FILESYSTEM_PLATFORM_PATH_TOO_LONG, detail, detailCapacity,
            "executable path exceeds the destination buffer");
    }
    path[size] = '\0';

    char * const separator = strrchr(path, '/');
    if (separator == (char *)0) {
        return FilesystemPlatform_fail_(
            FILESYSTEM_PLATFORM_IO_ERROR, detail, detailCapacity,
            "executable path has no parent directory");
    }
    if (separator == path) {
        path[1] = '\0';
    } else {
        *separator = '\0';
    }
    return FILESYSTEM_PLATFORM_OK;
}

FilesystemPlatformResult FilesystemPlatform_pathKind(
    char const * const path,
    FilesystemPlatformEntryKind * const kind,
    char * const detail,
    size_t const detailCapacity)
{
    FilesystemPlatform_clearDetail_(detail, detailCapacity);

    struct stat status;
    if (lstat(path, &status) != 0) {
        int const error = errno;
        FilesystemPlatformResult const result =
            ((error == ENOENT) || (error == ENOTDIR))
            ? FILESYSTEM_PLATFORM_NOT_FOUND
            : FILESYSTEM_PLATFORM_IO_ERROR;
        return FilesystemPlatform_fail_(
            result, detail, detailCapacity,
            "cannot inspect path: %s", strerror(error));
    }

    *kind = FilesystemPlatform_kind_(status.st_mode);
    return FILESYSTEM_PLATFORM_OK;
}

FilesystemPlatformResult FilesystemPlatform_enumerateDirectory(
    char const * const path,
    FilesystemPlatformVisitor const visitor,
    void * const ctx,
    char * const detail,
    size_t const detailCapacity)
{
    FilesystemPlatform_clearDetail_(detail, detailCapacity);

    DIR * const stream = opendir(path);
    if (stream == (DIR *)0) {
        int const error = errno;
        FilesystemPlatformResult const result =
            ((error == ENOENT) || (error == ENOTDIR))
            ? FILESYSTEM_PLATFORM_NOT_FOUND
            : FILESYSTEM_PLATFORM_IO_ERROR;
        return FilesystemPlatform_fail_(
            result, detail, detailCapacity,
            "cannot open directory: %s", strerror(error));
    }

    FilesystemPlatformResult result = FILESYSTEM_PLATFORM_OK;
    int const descriptor = dirfd(stream);
    if (descriptor < 0) {
        result = FilesystemPlatform_fail_(
            FILESYSTEM_PLATFORM_IO_ERROR, detail, detailCapacity,
            "cannot access directory descriptor: %s", strerror(errno));
    }

    while (result == FILESYSTEM_PLATFORM_OK) {
        errno = 0;
        struct dirent const * const nativeEntry = readdir(stream);
        if (nativeEntry == (struct dirent const *)0) {
            if (errno != 0) {
                result = FilesystemPlatform_fail_(
                    FILESYSTEM_PLATFORM_IO_ERROR, detail, detailCapacity,
                    "cannot read directory: %s", strerror(errno));
            }
            break;
        }
        if ((strcmp(nativeEntry->d_name, ".") == 0)
            || (strcmp(nativeEntry->d_name, "..") == 0))
        {
            continue;
        }

        struct stat status;
        if (fstatat(descriptor, nativeEntry->d_name, &status,
                    AT_SYMLINK_NOFOLLOW) != 0)
        {
            result = FilesystemPlatform_fail_(
                FILESYSTEM_PLATFORM_IO_ERROR, detail, detailCapacity,
                "cannot inspect directory entry: %s", strerror(errno));
            break;
        }

        FilesystemPlatformEntry const entry = {
            .name = nativeEntry->d_name,
            .kind = FilesystemPlatform_kind_(status.st_mode)
        };
        if (!(*visitor)(ctx, &entry)) {
            break;
        }
    }

    if (closedir(stream) != 0) {
        if (result == FILESYSTEM_PLATFORM_OK) {
            result = FilesystemPlatform_fail_(
                FILESYSTEM_PLATFORM_IO_ERROR, detail, detailCapacity,
                "cannot close directory: %s", strerror(errno));
        }
    }
    return result;
}
