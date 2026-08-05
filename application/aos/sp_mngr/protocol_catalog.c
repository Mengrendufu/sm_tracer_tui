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
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "dbc_assert.h"
#include "protocol_catalog_priv.h"

#if defined(_WIN32)
#include <wchar.h>
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

DBC_MODULE_NAME("protocol_catalog")

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
//=== Platform file-system bridge

#if defined(_WIN32)

#define PROTOCOL_CATALOG_WIDE_PATH_CAPACITY 32768U

static bool ProtocolCatalog_utf8FromWide_(ProtocolCatalog * const me,
                                          wchar_t const * const source,
                                          char * const target,
                                          size_t const capacity,
                                          ProtocolCatalogResult const result)
{
    int const written = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, source, -1,
        target, (int)capacity, (char const *)0, (BOOL *)0);
    if (written == 0) {
        (void)ProtocolCatalog_fail_(
            me, result,
            "UTF-16 path conversion failed: %lu",
            (unsigned long)GetLastError());
        return false;
    }
    for (char *ch = target; *ch != '\0'; ++ch) {
        if (*ch == '\\') {
            *ch = '/';
        }
    }
    return true;
}

static bool ProtocolCatalog_wideFromUtf8_(ProtocolCatalog * const me,
                                          char const * const source,
                                          wchar_t * const target,
                                          size_t const capacity)
{
    int const written = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, source, -1,
        target, (int)capacity);
    if (written == 0) {
        (void)ProtocolCatalog_fail_(
            me, PROTOCOL_CATALOG_SCAN_FAILED,
            "UTF-8 path conversion failed: %lu",
            (unsigned long)GetLastError());
        return false;
    }
    for (wchar_t *ch = target; *ch != L'\0'; ++ch) {
        if (*ch == L'/') {
            *ch = L'\\';
        }
    }
    return true;
}

static bool ProtocolCatalog_resolveRoot_(ProtocolCatalog * const me) {
    wchar_t executable[PROTOCOL_CATALOG_WIDE_PATH_CAPACITY];
    DWORD const size = GetModuleFileNameW(
        (HMODULE)0, executable,
        (DWORD)PROTOCOL_CATALOG_WIDE_PATH_CAPACITY);
    if ((size == 0U)
        || (size >= PROTOCOL_CATALOG_WIDE_PATH_CAPACITY))
    {
        (void)ProtocolCatalog_fail_(
            me, PROTOCOL_CATALOG_EXECUTABLE_PATH_FAILED,
            "cannot resolve executable path: %lu",
            (unsigned long)GetLastError());
        return false;
    }

    wchar_t *separator = wcsrchr(executable, L'\\');
    wchar_t * const slash = wcsrchr(executable, L'/');
    if ((slash != (wchar_t *)0)
        && ((separator == (wchar_t *)0) || (slash > separator)))
    {
        separator = slash;
    }
    if (separator == (wchar_t *)0) {
        (void)ProtocolCatalog_fail_(
            me, PROTOCOL_CATALOG_EXECUTABLE_PATH_FAILED,
            "executable path has no parent directory");
        return false;
    }

    wchar_t const suffix[] = L"\\protocols";
    size_t const parentSize = (size_t)(separator - executable);
    if ((parentSize + (sizeof(suffix) / sizeof(suffix[0])))
        > PROTOCOL_CATALOG_WIDE_PATH_CAPACITY)
    {
        (void)ProtocolCatalog_fail_(
            me, PROTOCOL_CATALOG_PATH_TOO_LONG,
            "protocol root path is too long");
        return false;
    }
    memcpy(&executable[parentSize], suffix, sizeof(suffix));
    return ProtocolCatalog_utf8FromWide_(
        me, executable, me->scanRoot, sizeof(me->scanRoot),
        PROTOCOL_CATALOG_EXECUTABLE_PATH_FAILED);
}

static bool ProtocolCatalog_rootValid_(ProtocolCatalog * const me) {
    wchar_t root[PROTOCOL_CATALOG_WIDE_PATH_CAPACITY];
    if (!ProtocolCatalog_wideFromUtf8_(
            me, me->scanRoot, root,
            PROTOCOL_CATALOG_WIDE_PATH_CAPACITY))
    {
        return false;
    }

    DWORD const attributes = GetFileAttributesW(root);
    if ((attributes == INVALID_FILE_ATTRIBUTES)
        || ((attributes & FILE_ATTRIBUTE_DIRECTORY) == 0U)
        || ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0U))
    {
        (void)ProtocolCatalog_fail_(
            me, PROTOCOL_CATALOG_ROOT_NOT_FOUND,
            "protocol root is missing or is not a physical directory");
        return false;
    }
    return true;
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

    wchar_t pattern[PROTOCOL_CATALOG_WIDE_PATH_CAPACITY];
    if (!ProtocolCatalog_wideFromUtf8_(
            me, directory, pattern,
            PROTOCOL_CATALOG_WIDE_PATH_CAPACITY))
    {
        return false;
    }
    size_t const patternSize = wcslen(pattern);
    if ((patternSize + 3U) > PROTOCOL_CATALOG_WIDE_PATH_CAPACITY) {
        (void)ProtocolCatalog_fail_(
            me, PROTOCOL_CATALOG_PATH_TOO_LONG,
            "protocol search pattern is too long");
        return false;
    }
    memcpy(&pattern[patternSize], L"\\*", 3U * sizeof(wchar_t));

    WIN32_FIND_DATAW data;
    HANDLE const search = FindFirstFileW(pattern, &data);
    if (search == INVALID_HANDLE_VALUE) {
        if (GetLastError() == ERROR_FILE_NOT_FOUND) {
            return true;
        }
        (void)ProtocolCatalog_fail_(
            me, PROTOCOL_CATALOG_SCAN_FAILED,
            "cannot scan protocol directory: %lu",
            (unsigned long)GetLastError());
        return false;
    }

    bool valid = true;
    for (;;) {
        if ((wcscmp(data.cFileName, L".") != 0)
            && (wcscmp(data.cFileName, L"..") != 0)
            && ((data.dwFileAttributes
                 & FILE_ATTRIBUTE_REPARSE_POINT) == 0U))
        {
            char name[PROTOCOL_CATALOG_PATH_CAPACITY];
            if (!ProtocolCatalog_utf8FromWide_(
                    me, data.cFileName, name, sizeof(name),
                    PROTOCOL_CATALOG_PATH_TOO_LONG))
            {
                valid = false;
                break;
            }

            char relativePath[PROTOCOL_CATALOG_PATH_CAPACITY];
            if (!ProtocolCatalog_join_(relativePath,
                                       sizeof(relativePath),
                                       relativeDir, name))
            {
                (void)ProtocolCatalog_fail_(
                    me, PROTOCOL_CATALOG_PATH_TOO_LONG,
                    "protocol relative path is too long");
                valid = false;
                break;
            }

            if ((data.dwFileAttributes
                 & FILE_ATTRIBUTE_DIRECTORY) != 0U)
            {
                if ((depth < PROTOCOL_CATALOG_MAX_DEPTH)
                    && !ProtocolCatalog_scanDir_(
                        me, catalogIndex, relativePath, depth + 1U))
                {
                    valid = false;
                    break;
                }
            } else if (ProtocolCatalog_isJson_(name)
                       && !ProtocolCatalog_add_(
                           me, catalogIndex, relativePath))
            {
                valid = false;
                break;
            }
        }

        if (!FindNextFileW(search, &data)) {
            DWORD const error = GetLastError();
            if (error != ERROR_NO_MORE_FILES) {
                (void)ProtocolCatalog_fail_(
                    me, PROTOCOL_CATALOG_SCAN_FAILED,
                    "protocol directory iteration failed: %lu",
                    (unsigned long)error);
                valid = false;
            }
            break;
        }
    }

    (void)FindClose(search);
    return valid;
}

#else

static bool ProtocolCatalog_resolveRoot_(ProtocolCatalog * const me) {
    char executable[PROTOCOL_CATALOG_ROOT_CAPACITY];
    ssize_t const size = readlink(
        "/proc/self/exe", executable, sizeof(executable) - 1U);
    if (size < 0) {
        (void)ProtocolCatalog_fail_(
            me, PROTOCOL_CATALOG_EXECUTABLE_PATH_FAILED,
            "cannot resolve /proc/self/exe: %s", strerror(errno));
        return false;
    }
    executable[size] = '\0';

    char * const separator = strrchr(executable, '/');
    if (separator == (char *)0) {
        (void)ProtocolCatalog_fail_(
            me, PROTOCOL_CATALOG_EXECUTABLE_PATH_FAILED,
            "executable path has no parent directory");
        return false;
    }
    *separator = '\0';

    if (!ProtocolCatalog_join_(me->scanRoot, sizeof(me->scanRoot),
                               executable, "protocols"))
    {
        (void)ProtocolCatalog_fail_(
            me, PROTOCOL_CATALOG_PATH_TOO_LONG,
            "protocol root path is too long");
        return false;
    }
    return true;
}

static bool ProtocolCatalog_rootValid_(ProtocolCatalog * const me) {
    struct stat status;
    if ((lstat(me->scanRoot, &status) != 0)
        || !S_ISDIR(status.st_mode)
        || S_ISLNK(status.st_mode))
    {
        (void)ProtocolCatalog_fail_(
            me, PROTOCOL_CATALOG_ROOT_NOT_FOUND,
            "protocol root is missing or is not a physical directory");
        return false;
    }
    return true;
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

    DIR * const stream = opendir(directory);
    if (stream == (DIR *)0) {
        (void)ProtocolCatalog_fail_(
            me, PROTOCOL_CATALOG_SCAN_FAILED,
            "cannot scan protocol directory: %s", strerror(errno));
        return false;
    }

    bool valid = true;
    errno = 0;
    for (;;) {
        struct dirent const * const entry = readdir(stream);
        if (entry == (struct dirent const *)0) {
            if (errno != 0) {
                (void)ProtocolCatalog_fail_(
                    me, PROTOCOL_CATALOG_SCAN_FAILED,
                    "protocol directory iteration failed: %s",
                    strerror(errno));
                valid = false;
            }
            break;
        }
        if ((strcmp(entry->d_name, ".") == 0)
            || (strcmp(entry->d_name, "..") == 0))
        {
            continue;
        }

        char relativePath[PROTOCOL_CATALOG_PATH_CAPACITY];
        if (!ProtocolCatalog_join_(relativePath,
                                   sizeof(relativePath),
                                   relativeDir, entry->d_name))
        {
            (void)ProtocolCatalog_fail_(
                me, PROTOCOL_CATALOG_PATH_TOO_LONG,
                "protocol relative path is too long");
            valid = false;
            break;
        }

        char path[PROTOCOL_CATALOG_FILE_PATH_CAPACITY];
        if (!ProtocolCatalog_join_(path, sizeof(path),
                                   me->scanRoot, relativePath))
        {
            (void)ProtocolCatalog_fail_(
                me, PROTOCOL_CATALOG_PATH_TOO_LONG,
                "protocol file path is too long");
            valid = false;
            break;
        }

        struct stat status;
        if (lstat(path, &status) != 0) {
            (void)ProtocolCatalog_fail_(
                me, PROTOCOL_CATALOG_SCAN_FAILED,
                "cannot inspect protocol path: %s", strerror(errno));
            valid = false;
            break;
        }
        if (S_ISLNK(status.st_mode)) {
            continue;
        }
        if (S_ISDIR(status.st_mode)) {
            if ((depth < PROTOCOL_CATALOG_MAX_DEPTH)
                && !ProtocolCatalog_scanDir_(
                    me, catalogIndex, relativePath, depth + 1U))
            {
                valid = false;
                break;
            }
        } else if (S_ISREG(status.st_mode)
                   && ProtocolCatalog_isJson_(entry->d_name)
                   && !ProtocolCatalog_add_(
                       me, catalogIndex, relativePath))
        {
            valid = false;
            break;
        }
        errno = 0;
    }

    if (closedir(stream) != 0) {
        if (valid) {
            (void)ProtocolCatalog_fail_(
                me, PROTOCOL_CATALOG_SCAN_FAILED,
                "cannot close protocol directory: %s", strerror(errno));
        }
        valid = false;
    }
    return valid;
}

#endif
