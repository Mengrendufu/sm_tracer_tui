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
#include <limits.h>
#include <locale.h>
#include <process.h>
#include <stdlib.h>
#include "platform_port.h"

static unsigned __stdcall Platform_threadMain_(void * const arg) {
    PlatformThread * const thread = (PlatformThread *)arg;
    (*thread->handler)(thread->ctx);
    return 0U;
}

int PlatformThread_start(PlatformThread * const thread,
                         PlatformThreadHandler const handler,
                         void * const ctx)
{
    if ((thread == (PlatformThread *)0)
        || (handler == (PlatformThreadHandler)0))
    {
        return 1;
    }

    thread->handler = handler;
    thread->ctx = ctx;
    uintptr_t const native = _beginthreadex(
        (void *)0, 0U, &Platform_threadMain_, thread, 0U, (unsigned *)0);
    if (native == 0U) {
        return 1;
    }
    thread->native = (HANDLE)native;
    return 0;
}

int PlatformThread_join(PlatformThread * const thread) {
    if ((thread == (PlatformThread *)0) || (thread->native == NULL)) {
        return 1;
    }

    DWORD const status = WaitForSingleObject(thread->native, INFINITE);
    if (status != WAIT_OBJECT_0) {
        return 1;
    }

    int const result = CloseHandle(thread->native) != FALSE ? 0 : 1;
    thread->native = NULL;
    thread->handler = (PlatformThreadHandler)0;
    thread->ctx = (void *)0;
    return result;
}

int PlatformMutex_lock(PlatformMutex * const mutex) {
    AcquireSRWLockExclusive(&mutex->native);
    return 0;
}

int PlatformMutex_unlock(PlatformMutex * const mutex) {
    ReleaseSRWLockExclusive(&mutex->native);
    return 0;
}

int PlatformSemaphore_init(PlatformSemaphore * const semaphore,
                           unsigned const initialCount)
{
    semaphore->native = CreateSemaphoreW(
        (LPSECURITY_ATTRIBUTES)0, (LONG)initialCount, LONG_MAX,
        (LPCWSTR)0);
    return semaphore->native != NULL ? 0 : 1;
}

void PlatformSemaphore_deinit(PlatformSemaphore * const semaphore) {
    if ((semaphore != (PlatformSemaphore *)0)
        && (semaphore->native != NULL))
    {
        (void)CloseHandle(semaphore->native);
        semaphore->native = NULL;
    }
}

int PlatformSemaphore_post(PlatformSemaphore * const semaphore) {
    return ReleaseSemaphore(semaphore->native, 1L, (LPLONG)0) != FALSE
               ? 0 : 1;
}

int PlatformSemaphore_wait(PlatformSemaphore * const semaphore) {
    DWORD const status = WaitForSingleObject(semaphore->native, INFINITE);
    return status == WAIT_OBJECT_0 ? 0 : 1;
}

int PlatformWake_init(PlatformWake * const wake) {
    if ((wake == (PlatformWake *)0) || (wake->native != NULL)) {
        return 1;
    }
    wake->native = CreateEventW((LPSECURITY_ATTRIBUTES)0, FALSE, FALSE,
                                (LPCWSTR)0);
    return wake->native != NULL ? 0 : 1;
}

void PlatformWake_deinit(PlatformWake * const wake) {
    if ((wake != (PlatformWake *)0) && (wake->native != NULL)) {
        (void)CloseHandle(wake->native);
        wake->native = NULL;
    }
}

int PlatformWake_signal(PlatformWake * const wake) {
    return SetEvent(wake->native) != FALSE ? 0 : 1;
}

int PlatformWake_consume(PlatformWake * const wake) {
    (void)wake;
    // Auto-reset events are consumed by WaitForMultipleObjects().
    return 0;
}

PlatformWaitObject PlatformWake_waitObject(
    PlatformWake const * const wake)
{
    return PlatformWaitObject_fromHandle(wake->native);
}

int PlatformBarrier_init(PlatformBarrier * const barrier) {
    if ((barrier == (PlatformBarrier *)0) || (barrier->native != NULL)) {
        return 1;
    }
    barrier->native = CreateEventW((LPSECURITY_ATTRIBUTES)0, TRUE, FALSE,
                                   (LPCWSTR)0);
    return barrier->native != NULL ? 0 : 1;
}

void PlatformBarrier_deinit(PlatformBarrier * const barrier) {
    if ((barrier != (PlatformBarrier *)0) && (barrier->native != NULL)) {
        (void)CloseHandle(barrier->native);
        barrier->native = NULL;
    }
}

int PlatformBarrier_signal(PlatformBarrier * const barrier) {
    return SetEvent(barrier->native) != FALSE ? 0 : 1;
}

int PlatformBarrier_wait(PlatformBarrier * const barrier) {
    DWORD const status = WaitForSingleObject(barrier->native, INFINITE);
    return status == WAIT_OBJECT_0 ? 0 : 1;
}

void PlatformWaitSet_init(PlatformWaitSet * const set,
                          size_t const count)
{
    set->count = count;
    for (size_t i = 0U; i < PLATFORM_WAIT_CAPACITY; ++i) {
        set->objects[i] = PLATFORM_WAIT_OBJECT_INVALID;
    }
}

void PlatformWaitSet_bind(PlatformWaitSet * const set,
                          size_t const index,
                          PlatformWaitObject const object)
{
    set->objects[index] = object;
}

PlatformWaitStatus PlatformWaitSet_wait(
    PlatformWaitSet * const set,
    int const timeoutMs,
    PlatformWaitResult * const result)
{
    HANDLE handles[PLATFORM_WAIT_CAPACITY];
    size_t sourceIndex[PLATFORM_WAIT_CAPACITY];
    DWORD handleCount = 0U;
    result->readyMask = 0U;
    result->closedMask = 0U;

    for (size_t i = 0U; i < set->count; ++i) {
        if (PlatformWaitObject_isValid(set->objects[i])) {
            handles[handleCount] = (HANDLE)set->objects[i].raw;
            sourceIndex[handleCount] = i;
            ++handleCount;
        }
    }
    if (handleCount == 0U) {
        return PLATFORM_WAIT_ERROR;
    }

    DWORD const timeout = timeoutMs < 0 ? INFINITE : (DWORD)timeoutMs;
    DWORD const status = WaitForMultipleObjects(
        handleCount, handles, FALSE, timeout);
    if (status == WAIT_TIMEOUT) {
        return PLATFORM_WAIT_TIMEOUT;
    }
    if (status >= (WAIT_OBJECT_0 + handleCount)) {
        return PLATFORM_WAIT_ERROR;
    }

    DWORD const first = status - WAIT_OBJECT_0;
    result->readyMask |= (uint32_t)1U << sourceIndex[first];

    for (DWORD i = 0U; i < handleCount; ++i) {
        if (i == first) {
            continue;
        }
        DWORD const extra = WaitForSingleObject(handles[i], 0U);
        if (extra == WAIT_OBJECT_0) {
            result->readyMask |= (uint32_t)1U << sourceIndex[i];
        } else if (extra == WAIT_FAILED) {
            return PLATFORM_WAIT_ERROR;
        }
    }
    return PLATFORM_WAIT_READY;
}

bool PlatformWaitObject_isValid(PlatformWaitObject const object) {
    return object.raw != 0U;
}

bool PlatformWaitObject_equal(PlatformWaitObject const lhs,
                              PlatformWaitObject const rhs)
{
    return lhs.raw == rhs.raw;
}

PlatformWaitObject PlatformWaitObject_fromHandle(void * const handle) {
    PlatformWaitObject object = PLATFORM_WAIT_OBJECT_INVALID;
    if ((handle != (void *)0) && (handle != INVALID_HANDLE_VALUE)) {
        object.raw = (uintptr_t)handle;
    }
    return object;
}

int Platform_monotonicMs(uint64_t * const result) {
    LARGE_INTEGER frequency;
    LARGE_INTEGER counter;
    if ((result == (uint64_t *)0)
        || (QueryPerformanceFrequency(&frequency) == FALSE)
        || (QueryPerformanceCounter(&counter) == FALSE)
        || (frequency.QuadPart <= 0))
    {
        return 1;
    }

    uint64_t const ticks = (uint64_t)counter.QuadPart;
    uint64_t const ticksPerSecond = (uint64_t)frequency.QuadPart;
    *result = (ticks / ticksPerSecond) * 1000U
              + ((ticks % ticksPerSecond) * 1000U) / ticksPerSecond;
    return 0;
}

void Platform_delayMs(uint32_t const delayMs) {
    Sleep((DWORD)delayMs);
}

int Platform_consoleInit(void) {
    return (setlocale(LC_ALL, ".UTF8") != (char *)0)
           && (SetConsoleCP(CP_UTF8) != FALSE)
           && (SetConsoleOutputCP(CP_UTF8) != FALSE) ? 0 : 1;
}

FILE *PlatformFile_openRead(char const * const utf8Path) {
    if (utf8Path == (char const *)0) {
        errno = EINVAL;
        return (FILE *)0;
    }

    int const count = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, utf8Path, -1,
        (wchar_t *)0, 0);
    if (count <= 0) {
        errno = EINVAL;
        return (FILE *)0;
    }

    wchar_t * const widePath = (wchar_t *)malloc(
        (size_t)count * sizeof(wchar_t));
    if (widePath == (wchar_t *)0) {
        errno = ENOMEM;
        return (FILE *)0;
    }
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                            utf8Path, -1, widePath, count) <= 0)
    {
        free(widePath);
        errno = EINVAL;
        return (FILE *)0;
    }

    FILE * const file = _wfopen(widePath, L"rb");
    int const errorNumber = errno;
    free(widePath);
    errno = errorNumber;
    return file;
}

void Platform_faultWrite(char const * const text, size_t const size) {
    HANDLE const errorHandle = GetStdHandle(STD_ERROR_HANDLE);
    if ((errorHandle != NULL) && (errorHandle != INVALID_HANDLE_VALUE)) {
        DWORD written;
        (void)WriteFile(errorHandle, text, (DWORD)size, &written,
                        (LPOVERLAPPED)0);
    }
}
