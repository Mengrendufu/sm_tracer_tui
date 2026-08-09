//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "filesystem_platform.h"

typedef struct {
    bool foundProtocol;
} VisitContext;

static bool visitEntry_(void * const ctx,
                        FilesystemPlatformEntry const * const entry)
{
    VisitContext * const visit = (VisitContext *)ctx;
    if ((strcmp(entry->name, "blinky_c51.json") == 0)
        && (entry->kind == FILESYSTEM_PLATFORM_ENTRY_FILE))
    {
        visit->foundProtocol = true;
    }
    return true;
}

static void normalizeSeparators_(char * const path) {
    for (char *ch = path; *ch != '\0'; ++ch) {
        if (*ch == '\\') {
            *ch = '/';
        }
    }
}

int main(int const argc, char const ** const argv) {
    if (argc != 3) {
        return 1;
    }

    int failed = 0;
    char detail[192];
    char executableDirectory[4096];
    FilesystemPlatformResult result =
        FilesystemPlatform_executableDirectory(
            executableDirectory, sizeof(executableDirectory),
            detail, sizeof(detail));
    if (result != FILESYSTEM_PLATFORM_OK) {
        fprintf(stderr, "executable directory: %s\n", detail);
        ++failed;
    }

    char expectedDirectory[4096];
    int const expectedLength = snprintf(
        expectedDirectory, sizeof(expectedDirectory), "%s", argv[1]);
    if ((expectedLength <= 0)
        || ((size_t)expectedLength >= sizeof(expectedDirectory)))
    {
        return 1;
    }
    normalizeSeparators_(expectedDirectory);
    if (strcmp(executableDirectory, expectedDirectory) != 0) {
        fprintf(stderr, "executable directory: expected '%s', got '%s'\n",
                expectedDirectory, executableDirectory);
        ++failed;
    }

    FilesystemPlatformEntryKind kind = FILESYSTEM_PLATFORM_ENTRY_OTHER;
    result = FilesystemPlatform_pathKind(
        argv[2], &kind, detail, sizeof(detail));
    if ((result != FILESYSTEM_PLATFORM_OK)
        || (kind != FILESYSTEM_PLATFORM_ENTRY_DIRECTORY))
    {
        fprintf(stderr, "directory kind: %s\n", detail);
        ++failed;
    }

    VisitContext visit = {false};
    result = FilesystemPlatform_enumerateDirectory(
        argv[2], &visitEntry_, &visit, detail, sizeof(detail));
    if (result != FILESYSTEM_PLATFORM_OK) {
        fprintf(stderr, "directory enumeration: %s\n", detail);
        ++failed;
    }
    if (!visit.foundProtocol) {
        fprintf(stderr, "directory enumeration missed blinky_c51.json\n");
        ++failed;
    }

    return failed == 0 ? 0 : 1;
}
