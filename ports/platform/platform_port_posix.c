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
#include <fcntl.h>
#include <locale.h>
#include <poll.h>
#include <string.h>
#include <sys/eventfd.h>
#include <time.h>
#include <unistd.h>
#include "platform_port.h"

static void *Platform_threadMain_(void * const arg) {
    PlatformThread * const thread = (PlatformThread *)arg;
    (*thread->handler)(thread->ctx);
    return (void *)0;
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
    return pthread_create(&thread->native, (pthread_attr_t const *)0,
                          &Platform_threadMain_, thread);
}

int PlatformThread_join(PlatformThread * const thread) {
    if ((thread == (PlatformThread *)0)
        || (thread->handler == (PlatformThreadHandler)0))
    {
        return 1;
    }

    int const result = pthread_join(thread->native, (void **)0);
    if (result == 0) {
        thread->handler = (PlatformThreadHandler)0;
        thread->ctx = (void *)0;
    }
    return result;
}

int PlatformMutex_lock(PlatformMutex * const mutex) {
    return pthread_mutex_lock(&mutex->native);
}

int PlatformMutex_unlock(PlatformMutex * const mutex) {
    return pthread_mutex_unlock(&mutex->native);
}

int PlatformSemaphore_init(PlatformSemaphore * const semaphore,
                           unsigned const initialCount)
{
    int const status = sem_init(&semaphore->native, 0, initialCount);
    semaphore->initialized = status == 0;
    return status;
}

void PlatformSemaphore_deinit(PlatformSemaphore * const semaphore) {
    if (semaphore->initialized) {
        (void)sem_destroy(&semaphore->native);
        semaphore->initialized = false;
    }
}

int PlatformSemaphore_post(PlatformSemaphore * const semaphore) {
    return sem_post(&semaphore->native);
}

int PlatformSemaphore_wait(PlatformSemaphore * const semaphore) {
    int status;
    do {
        status = sem_wait(&semaphore->native);
    } while ((status != 0) && (errno == EINTR));
    return status;
}

int PlatformWake_init(PlatformWake * const wake) {
    if ((wake == (PlatformWake *)0) || (wake->descriptor >= 0)) {
        return 1;
    }

    wake->descriptor = eventfd(0U, EFD_NONBLOCK | EFD_CLOEXEC);
    return wake->descriptor >= 0 ? 0 : 1;
}

void PlatformWake_deinit(PlatformWake * const wake) {
    if ((wake != (PlatformWake *)0) && (wake->descriptor >= 0)) {
        (void)close(wake->descriptor);
        wake->descriptor = -1;
    }
}

int PlatformWake_signal(PlatformWake * const wake) {
    uint64_t const one = 1ULL;
    ssize_t const written = write(wake->descriptor, &one, sizeof(one));
    return written == (ssize_t)sizeof(one) ? 0 : 1;
}

int PlatformWake_consume(PlatformWake * const wake) {
    uint64_t count;
    ssize_t const size = read(wake->descriptor, &count, sizeof(count));
    return size == (ssize_t)sizeof(count) ? 0 : 1;
}

PlatformWaitObject PlatformWake_waitObject(
    PlatformWake const * const wake)
{
    return PlatformWaitObject_fromDescriptor(wake->descriptor);
}

int PlatformBarrier_init(PlatformBarrier * const barrier) {
    if ((barrier == (PlatformBarrier *)0) || barrier->initialized) {
        return 1;
    }

    int status = pthread_mutex_init(&barrier->mutex,
                                    (pthread_mutexattr_t const *)0);
    if (status != 0) {
        return status;
    }
    status = pthread_cond_init(&barrier->condition,
                               (pthread_condattr_t const *)0);
    if (status != 0) {
        (void)pthread_mutex_destroy(&barrier->mutex);
        return status;
    }

    barrier->signaled = false;
    barrier->initialized = true;
    return 0;
}

void PlatformBarrier_deinit(PlatformBarrier * const barrier) {
    if ((barrier != (PlatformBarrier *)0) && barrier->initialized) {
        (void)pthread_cond_destroy(&barrier->condition);
        (void)pthread_mutex_destroy(&barrier->mutex);
        barrier->initialized = false;
    }
}

int PlatformBarrier_signal(PlatformBarrier * const barrier) {
    int status = pthread_mutex_lock(&barrier->mutex);
    if (status != 0) {
        return status;
    }
    barrier->signaled = true;
    status = pthread_cond_broadcast(&barrier->condition);
    int const unlockStatus = pthread_mutex_unlock(&barrier->mutex);
    return status != 0 ? status : unlockStatus;
}

int PlatformBarrier_wait(PlatformBarrier * const barrier) {
    int status = pthread_mutex_lock(&barrier->mutex);
    if (status != 0) {
        return status;
    }
    while (!barrier->signaled) {
        status = pthread_cond_wait(&barrier->condition,
                                   &barrier->mutex);
        if (status != 0) {
            break;
        }
    }
    int const unlockStatus = pthread_mutex_unlock(&barrier->mutex);
    return status != 0 ? status : unlockStatus;
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
    struct pollfd entries[PLATFORM_WAIT_CAPACITY];
    memset(entries, 0, sizeof(entries));
    result->readyMask = 0U;
    result->closedMask = 0U;

    for (size_t i = 0U; i < set->count; ++i) {
        entries[i].fd = PlatformWaitObject_isValid(set->objects[i])
                            ? (int)(set->objects[i].raw - 1U) : -1;
        entries[i].events = POLLIN;
    }

    int const ready = poll(entries, (nfds_t)set->count, timeoutMs);
    if (ready < 0) {
        return errno == EINTR ? PLATFORM_WAIT_INTERRUPTED
                             : PLATFORM_WAIT_ERROR;
    }
    if (ready == 0) {
        return PLATFORM_WAIT_TIMEOUT;
    }

    for (size_t i = 0U; i < set->count; ++i) {
        uint32_t const bit = (uint32_t)1U << i;
        if ((entries[i].revents & (POLLIN | POLLPRI)) != 0) {
            result->readyMask |= bit;
        }
        if ((entries[i].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
            result->closedMask |= bit;
        }
    }

    return (result->readyMask | result->closedMask) != 0U
               ? PLATFORM_WAIT_READY : PLATFORM_WAIT_ERROR;
}

bool PlatformWaitObject_isValid(PlatformWaitObject const object) {
    return object.raw != 0U;
}

bool PlatformWaitObject_equal(PlatformWaitObject const lhs,
                              PlatformWaitObject const rhs)
{
    return lhs.raw == rhs.raw;
}

PlatformWaitObject PlatformWaitObject_fromDescriptor(
    int const descriptor)
{
    PlatformWaitObject object = PLATFORM_WAIT_OBJECT_INVALID;
    if (descriptor >= 0) {
        object.raw = (uintptr_t)(unsigned)descriptor + 1U;
    }
    return object;
}

int Platform_monotonicMs(uint64_t * const result) {
    struct timespec now;
    if ((result == (uint64_t *)0)
        || (clock_gettime(CLOCK_MONOTONIC, &now) != 0))
    {
        return 1;
    }
    *result = ((uint64_t)now.tv_sec * 1000U)
              + ((uint64_t)now.tv_nsec / 1000000U);
    return 0;
}

void Platform_delayMs(uint32_t const delayMs) {
    struct timespec remaining = {
        .tv_sec = (time_t)(delayMs / 1000U),
        .tv_nsec = (long)(delayMs % 1000U) * 1000000L,
    };
    while ((nanosleep(&remaining, &remaining) != 0) && (errno == EINTR)) {
    }
}

int Platform_consoleInit(void) {
    return setlocale(LC_ALL, "") != (char *)0 ? 0 : 1;
}

FILE *PlatformFile_openRead(char const * const utf8Path) {
    return fopen(utf8Path, "rb");
}

void Platform_faultWrite(char const * const text, size_t const size) {
    int descriptor = open("/dev/tty", O_WRONLY);
    if (descriptor < 0) {
        descriptor = STDERR_FILENO;
    }
    ssize_t const written = write(descriptor, text, size);
    (void)written;
    if (descriptor != STDERR_FILENO) {
        (void)close(descriptor);
    }
}
