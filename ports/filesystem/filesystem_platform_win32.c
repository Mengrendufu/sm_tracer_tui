//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <windows.h>
#include "filesystem_platform.h"

//============================================================================
//=== Component: FilesystemPlatform (Win32)

#define FILESYSTEM_PLATFORM_WIDE_PATH_CAPACITY 32768U

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

static FilesystemPlatformResult FilesystemPlatform_nativeResult_(
    DWORD const error)
{
    if ((error == ERROR_FILE_NOT_FOUND)
        || (error == ERROR_PATH_NOT_FOUND)
        || (error == ERROR_INVALID_NAME))
    {
        return FILESYSTEM_PLATFORM_NOT_FOUND;
    } else if ((error == ERROR_INSUFFICIENT_BUFFER)
               || (error == ERROR_FILENAME_EXCED_RANGE))
    {
        return FILESYSTEM_PLATFORM_PATH_TOO_LONG;
    } else if (error == ERROR_NO_UNICODE_TRANSLATION) {
        return FILESYSTEM_PLATFORM_INVALID_ENCODING;
    } else {
        return FILESYSTEM_PLATFORM_IO_ERROR;
    }
}

static wchar_t *FilesystemPlatform_wideFromUtf8_(
    char const * const source,
    char * const detail,
    size_t const detailCapacity,
    FilesystemPlatformResult * const result)
{
    int const size = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, source, -1,
        (wchar_t *)0, 0);
    if (size <= 0) {
        DWORD const error = GetLastError();
        *result = FilesystemPlatform_fail_(
            FilesystemPlatform_nativeResult_(error), detail, detailCapacity,
            "UTF-8 path conversion failed: %lu", (unsigned long)error);
        return (wchar_t *)0;
    }

    wchar_t * const target = (wchar_t *)malloc(
        (size_t)size * sizeof(wchar_t));
    if (target == (wchar_t *)0) {
        *result = FilesystemPlatform_fail_(
            FILESYSTEM_PLATFORM_IO_ERROR, detail, detailCapacity,
            "cannot allocate a native path buffer");
        return (wchar_t *)0;
    }
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                            source, -1, target, size) == 0)
    {
        DWORD const error = GetLastError();
        free(target);
        *result = FilesystemPlatform_fail_(
            FilesystemPlatform_nativeResult_(error), detail, detailCapacity,
            "UTF-8 path conversion failed: %lu", (unsigned long)error);
        return (wchar_t *)0;
    }

    for (wchar_t *ch = target; *ch != L'\0'; ++ch) {
        if (*ch == L'/') {
            *ch = L'\\';
        }
    }
    *result = FILESYSTEM_PLATFORM_OK;
    return target;
}

static FilesystemPlatformResult FilesystemPlatform_utf8FromWide_(
    wchar_t const * const source,
    char * const target,
    size_t const capacity,
    char * const detail,
    size_t const detailCapacity)
{
    int const size = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, source, -1,
        (char *)0, 0, (char const *)0, (BOOL *)0);
    if (size <= 0) {
        DWORD const error = GetLastError();
        return FilesystemPlatform_fail_(
            FilesystemPlatform_nativeResult_(error), detail, detailCapacity,
            "UTF-16 path conversion failed: %lu", (unsigned long)error);
    }
    if ((size_t)size > capacity) {
        return FilesystemPlatform_fail_(
            FILESYSTEM_PLATFORM_PATH_TOO_LONG, detail, detailCapacity,
            "UTF-8 path exceeds the destination buffer");
    }
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                            source, -1, target, size,
                            (char const *)0, (BOOL *)0) == 0)
    {
        DWORD const error = GetLastError();
        return FilesystemPlatform_fail_(
            FilesystemPlatform_nativeResult_(error), detail, detailCapacity,
            "UTF-16 path conversion failed: %lu", (unsigned long)error);
    }

    for (char *ch = target; *ch != '\0'; ++ch) {
        if (*ch == '\\') {
            *ch = '/';
        }
    }
    return FILESYSTEM_PLATFORM_OK;
}

static char *FilesystemPlatform_utf8Name_(
    wchar_t const * const source,
    char * const detail,
    size_t const detailCapacity,
    FilesystemPlatformResult * const result)
{
    int const size = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, source, -1,
        (char *)0, 0, (char const *)0, (BOOL *)0);
    if (size <= 0) {
        DWORD const error = GetLastError();
        *result = FilesystemPlatform_fail_(
            FilesystemPlatform_nativeResult_(error), detail, detailCapacity,
            "directory entry conversion failed: %lu",
            (unsigned long)error);
        return (char *)0;
    }

    char * const name = (char *)malloc((size_t)size);
    if (name == (char *)0) {
        *result = FilesystemPlatform_fail_(
            FILESYSTEM_PLATFORM_IO_ERROR, detail, detailCapacity,
            "cannot allocate a directory entry buffer");
        return (char *)0;
    }
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                            source, -1, name, size,
                            (char const *)0, (BOOL *)0) == 0)
    {
        DWORD const error = GetLastError();
        free(name);
        *result = FilesystemPlatform_fail_(
            FilesystemPlatform_nativeResult_(error), detail, detailCapacity,
            "directory entry conversion failed: %lu",
            (unsigned long)error);
        return (char *)0;
    }

    *result = FILESYSTEM_PLATFORM_OK;
    return name;
}

static FilesystemPlatformEntryKind FilesystemPlatform_kind_(
    DWORD const attributes)
{
    if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0U) {
        return FILESYSTEM_PLATFORM_ENTRY_LINK;
    } else if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0U) {
        return FILESYSTEM_PLATFORM_ENTRY_DIRECTORY;
    } else {
        return FILESYSTEM_PLATFORM_ENTRY_FILE;
    }
}

FilesystemPlatformResult FilesystemPlatform_executableDirectory(
    char * const path,
    size_t const capacity,
    char * const detail,
    size_t const detailCapacity)
{
    FilesystemPlatform_clearDetail_(detail, detailCapacity);

    wchar_t * const executable = (wchar_t *)malloc(
        FILESYSTEM_PLATFORM_WIDE_PATH_CAPACITY * sizeof(wchar_t));
    if (executable == (wchar_t *)0) {
        return FilesystemPlatform_fail_(
            FILESYSTEM_PLATFORM_IO_ERROR, detail, detailCapacity,
            "cannot allocate an executable path buffer");
    }

    DWORD const size = GetModuleFileNameW(
        (HMODULE)0, executable,
        (DWORD)FILESYSTEM_PLATFORM_WIDE_PATH_CAPACITY);
    if ((size == 0U)
        || (size >= FILESYSTEM_PLATFORM_WIDE_PATH_CAPACITY))
    {
        DWORD const error = GetLastError();
        free(executable);
        return FilesystemPlatform_fail_(
            FilesystemPlatform_nativeResult_(error), detail, detailCapacity,
            "cannot resolve executable path: %lu", (unsigned long)error);
    }

    wchar_t *separator = wcsrchr(executable, L'\\');
    wchar_t * const slash = wcsrchr(executable, L'/');
    if ((slash != (wchar_t *)0)
        && ((separator == (wchar_t *)0) || (slash > separator)))
    {
        separator = slash;
    }
    if (separator == (wchar_t *)0) {
        free(executable);
        return FilesystemPlatform_fail_(
            FILESYSTEM_PLATFORM_IO_ERROR, detail, detailCapacity,
            "executable path has no parent directory");
    }
    if ((separator == &executable[2]) && (executable[1] == L':')) {
        separator[1] = L'\0';
    } else {
        *separator = L'\0';
    }

    FilesystemPlatformResult const result =
        FilesystemPlatform_utf8FromWide_(
            executable, path, capacity, detail, detailCapacity);
    free(executable);
    return result;
}

FilesystemPlatformResult FilesystemPlatform_pathKind(
    char const * const path,
    FilesystemPlatformEntryKind * const kind,
    char * const detail,
    size_t const detailCapacity)
{
    FilesystemPlatform_clearDetail_(detail, detailCapacity);

    FilesystemPlatformResult result;
    wchar_t * const nativePath = FilesystemPlatform_wideFromUtf8_(
        path, detail, detailCapacity, &result);
    if (nativePath == (wchar_t *)0) {
        return result;
    }

    DWORD const attributes = GetFileAttributesW(nativePath);
    DWORD const attributeError = attributes == INVALID_FILE_ATTRIBUTES
        ? GetLastError()
        : ERROR_SUCCESS;
    free(nativePath);
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        return FilesystemPlatform_fail_(
            FilesystemPlatform_nativeResult_(attributeError),
            detail, detailCapacity,
            "cannot inspect path: %lu", (unsigned long)attributeError);
    }

    *kind = FilesystemPlatform_kind_(attributes);
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

    FilesystemPlatformResult result;
    wchar_t *nativePath = FilesystemPlatform_wideFromUtf8_(
        path, detail, detailCapacity, &result);
    if (nativePath == (wchar_t *)0) {
        return result;
    }

    size_t const length = wcslen(nativePath);
    bool const needsSeparator = (length == 0U)
        || ((nativePath[length - 1U] != L'\\')
            && (nativePath[length - 1U] != L'/'));
    size_t const suffixSize = needsSeparator ? 3U : 2U;
    wchar_t * const pattern = (wchar_t *)realloc(
        nativePath, (length + suffixSize) * sizeof(wchar_t));
    if (pattern == (wchar_t *)0) {
        free(nativePath);
        return FilesystemPlatform_fail_(
            FILESYSTEM_PLATFORM_IO_ERROR, detail, detailCapacity,
            "cannot allocate a directory search pattern");
    }
    if (needsSeparator) {
        memcpy(&pattern[length], L"\\*", 3U * sizeof(wchar_t));
    } else {
        memcpy(&pattern[length], L"*", 2U * sizeof(wchar_t));
    }

    WIN32_FIND_DATAW data;
    HANDLE const search = FindFirstFileW(pattern, &data);
    DWORD const searchError = search == INVALID_HANDLE_VALUE
        ? GetLastError()
        : ERROR_SUCCESS;
    free(pattern);
    if (search == INVALID_HANDLE_VALUE) {
        if (searchError == ERROR_FILE_NOT_FOUND) {
            return FILESYSTEM_PLATFORM_OK;
        }
        return FilesystemPlatform_fail_(
            FilesystemPlatform_nativeResult_(searchError),
            detail, detailCapacity,
            "cannot open directory: %lu", (unsigned long)searchError);
    }

    result = FILESYSTEM_PLATFORM_OK;
    for (;;) {
        if ((wcscmp(data.cFileName, L".") != 0)
            && (wcscmp(data.cFileName, L"..") != 0))
        {
            char * const name = FilesystemPlatform_utf8Name_(
                data.cFileName, detail, detailCapacity, &result);
            if (name == (char *)0) {
                break;
            }

            FilesystemPlatformEntry const entry = {
                .name = name,
                .kind = FilesystemPlatform_kind_(data.dwFileAttributes)
            };
            bool const keepGoing = (*visitor)(ctx, &entry);
            free(name);
            if (!keepGoing) {
                break;
            }
        }

        if (!FindNextFileW(search, &data)) {
            DWORD const error = GetLastError();
            if (error != ERROR_NO_MORE_FILES) {
                result = FilesystemPlatform_fail_(
                    FilesystemPlatform_nativeResult_(error),
                    detail, detailCapacity,
                    "cannot read directory: %lu", (unsigned long)error);
            }
            break;
        }
    }

    if (!FindClose(search)) {
        if (result == FILESYSTEM_PLATFORM_OK) {
            DWORD const error = GetLastError();
            result = FilesystemPlatform_fail_(
                FilesystemPlatform_nativeResult_(error),
                detail, detailCapacity,
                "cannot close directory: %lu", (unsigned long)error);
        }
    }
    return result;
}
