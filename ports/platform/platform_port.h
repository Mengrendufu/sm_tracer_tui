//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#ifndef PLATFORM_PORT_H_
#define PLATFORM_PORT_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>

#if defined(_WIN32)

#include <windows.h>

typedef struct {
    SRWLOCK native;
} PlatformMutex;

typedef struct {
    HANDLE native;
} PlatformSemaphore;

typedef struct {
    HANDLE native;
} PlatformWake;

typedef struct {
    HANDLE native;
} PlatformBarrier;

#define PLATFORM_MUTEX_INITIALIZER     {SRWLOCK_INIT}
#define PLATFORM_SEMAPHORE_INITIALIZER {NULL}
#define PLATFORM_WAKE_INITIALIZER      {NULL}
#define PLATFORM_BARRIER_INITIALIZER   {NULL}

#else // Linux

#include <pthread.h>
#include <semaphore.h>

typedef struct {
    pthread_mutex_t native;
} PlatformMutex;

typedef struct {
    sem_t native;
    bool initialized;
} PlatformSemaphore;

typedef struct {
    int descriptor;
} PlatformWake;

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    bool signaled;
    bool initialized;
} PlatformBarrier;

#define PLATFORM_MUTEX_INITIALIZER     {PTHREAD_MUTEX_INITIALIZER}
#define PLATFORM_SEMAPHORE_INITIALIZER {.initialized = false}
#define PLATFORM_WAKE_INITIALIZER      {-1}
#define PLATFORM_BARRIER_INITIALIZER   \
    {.signaled = false, .initialized = false}

#endif // defined(_WIN32)

#define PLATFORM_THREAD_LOCAL __thread

typedef void (*PlatformThreadHandler)(void *ctx);

typedef struct {
#if defined(_WIN32)
    HANDLE native;
#else
    pthread_t native;
#endif
    PlatformThreadHandler handler;
    void *ctx;
} PlatformThread;

#define PLATFORM_THREAD_INITIALIZER {0}

typedef struct {
    uintptr_t raw;
} PlatformWaitObject;

#define PLATFORM_WAIT_OBJECT_INVALID ((PlatformWaitObject){0U})
#define PLATFORM_WAIT_OBJECT_INITIALIZER {0U}
#define PLATFORM_WAIT_CAPACITY 2U

typedef struct {
    PlatformWaitObject objects[PLATFORM_WAIT_CAPACITY];
    size_t count;
} PlatformWaitSet;

#define PLATFORM_WAIT_SET_INITIALIZER \
    {{PLATFORM_WAIT_OBJECT_INITIALIZER, \
      PLATFORM_WAIT_OBJECT_INITIALIZER}, 0U}

typedef struct {
    uint32_t readyMask;
    uint32_t closedMask;
} PlatformWaitResult;

typedef enum {
    PLATFORM_WAIT_READY,
    PLATFORM_WAIT_TIMEOUT,
    PLATFORM_WAIT_INTERRUPTED,
    PLATFORM_WAIT_ERROR
} PlatformWaitStatus;

int PlatformThread_start(PlatformThread *thread,
                         PlatformThreadHandler handler,
                         void *ctx);

int PlatformMutex_lock(PlatformMutex *mutex);
int PlatformMutex_unlock(PlatformMutex *mutex);

int PlatformSemaphore_init(PlatformSemaphore *semaphore,
                           unsigned initialCount);
void PlatformSemaphore_deinit(PlatformSemaphore *semaphore);
int PlatformSemaphore_post(PlatformSemaphore *semaphore);
int PlatformSemaphore_wait(PlatformSemaphore *semaphore);

int PlatformWake_init(PlatformWake *wake);
void PlatformWake_deinit(PlatformWake *wake);
int PlatformWake_signal(PlatformWake *wake);
int PlatformWake_consume(PlatformWake *wake);
PlatformWaitObject PlatformWake_waitObject(PlatformWake const *wake);

int PlatformBarrier_init(PlatformBarrier *barrier);
void PlatformBarrier_deinit(PlatformBarrier *barrier);
int PlatformBarrier_signal(PlatformBarrier *barrier);
int PlatformBarrier_wait(PlatformBarrier *barrier);

void PlatformWaitSet_init(PlatformWaitSet *set, size_t count);
void PlatformWaitSet_bind(PlatformWaitSet *set, size_t index,
                          PlatformWaitObject object);
PlatformWaitStatus PlatformWaitSet_wait(PlatformWaitSet *set,
                                        int timeoutMs,
                                        PlatformWaitResult *result);
bool PlatformWaitObject_isValid(PlatformWaitObject object);
bool PlatformWaitObject_equal(PlatformWaitObject lhs,
                              PlatformWaitObject rhs);

#if defined(_WIN32)

PlatformWaitObject PlatformWaitObject_fromHandle(void *handle);

#else // Linux

PlatformWaitObject PlatformWaitObject_fromDescriptor(int descriptor);

#endif // defined(_WIN32)

int Platform_monotonicMs(uint64_t *result);
void Platform_delayMs(uint32_t delayMs);
int Platform_consoleInit(void);
FILE *PlatformFile_openRead(char const *utf8Path);
void Platform_faultWrite(char const *text, size_t size);

#endif // PLATFORM_PORT_H_
