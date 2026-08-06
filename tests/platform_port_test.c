//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#include <stdint.h>
#include <stdio.h>
#include "platform_port.h"

#if defined(_WIN32)
#include <wchar.h>
#endif

static PlatformWake l_eventWake_ = PLATFORM_WAKE_INITIALIZER;
static PlatformWake l_inputWake_ = PLATFORM_WAKE_INITIALIZER;
static PlatformSemaphore l_workerDone_ = PLATFORM_SEMAPHORE_INITIALIZER;
static PlatformBarrier l_workerStart_ = PLATFORM_BARRIER_INITIALIZER;
static PlatformThread l_worker_ = PLATFORM_THREAD_INITIALIZER;

static char const l_utf8Path_[] =
    "platform_port_utf8_\xC3\xA9.txt";

static int testUtf8FileOpen_(void) {
#if defined(_WIN32)
    wchar_t const * const nativePath =
        L"platform_port_utf8_\x00E9.txt";
    FILE *file = _wfopen(nativePath, L"wb");
#else
    FILE *file = fopen(l_utf8Path_, "wb");
#endif
    if (file == (FILE *)0) {
        return 1;
    }

    int failed = fputs("ok", file) < 0 ? 1 : 0;
    failed += fclose(file) == 0 ? 0 : 1;

    file = PlatformFile_openRead(l_utf8Path_);
    if (file == (FILE *)0) {
        failed += 1;
    } else {
        failed += fgetc(file) == 'o' ? 0 : 1;
        failed += fgetc(file) == 'k' ? 0 : 1;
        failed += fclose(file) == 0 ? 0 : 1;
    }

#if defined(_WIN32)
    failed += _wremove(nativePath) == 0 ? 0 : 1;
#else
    failed += remove(l_utf8Path_) == 0 ? 0 : 1;
#endif
    return failed;
}

static void worker_(void * const ctx) {
    PlatformWake * const wake = (PlatformWake *)ctx;

    (void)PlatformBarrier_wait(&l_workerStart_);
    (void)PlatformWake_signal(wake);
    (void)PlatformSemaphore_post(&l_workerDone_);
}

int main(void) {
    int failed = 0;

    failed += testUtf8FileOpen_();

    failed += PlatformWake_init(&l_eventWake_) == 0 ? 0 : 1;
    failed += PlatformWake_init(&l_inputWake_) == 0 ? 0 : 1;
    failed += PlatformSemaphore_init(&l_workerDone_, 0U) == 0 ? 0 : 1;
    failed += PlatformBarrier_init(&l_workerStart_) == 0 ? 0 : 1;

    PlatformWaitSet waitSet = PLATFORM_WAIT_SET_INITIALIZER;
    PlatformWaitSet_init(&waitSet, 2U);
    PlatformWaitSet_bind(&waitSet, 0U,
                         PlatformWake_waitObject(&l_eventWake_));
    PlatformWaitSet_bind(&waitSet, 1U,
                         PlatformWake_waitObject(&l_inputWake_));

    PlatformWaitResult result = {0U, 0U};
    failed += PlatformWaitSet_wait(&waitSet, 0, &result)
                  == PLATFORM_WAIT_TIMEOUT ? 0 : 1;
    failed += result.readyMask == 0U ? 0 : 1;

    (void)PlatformWake_signal(&l_eventWake_);
    (void)PlatformWake_signal(&l_inputWake_);
    failed += PlatformWaitSet_wait(&waitSet, 100, &result)
                  == PLATFORM_WAIT_READY ? 0 : 1;
    failed += result.readyMask == 0x3U ? 0 : 1;
    failed += PlatformWake_consume(&l_eventWake_) == 0 ? 0 : 1;
    failed += PlatformWake_consume(&l_inputWake_) == 0 ? 0 : 1;

    failed += PlatformThread_start(&l_worker_, &worker_, &l_inputWake_)
                  == 0 ? 0 : 1;
    failed += PlatformBarrier_signal(&l_workerStart_) == 0 ? 0 : 1;
    failed += PlatformWaitSet_wait(&waitSet, 100, &result)
                  == PLATFORM_WAIT_READY ? 0 : 1;
    failed += result.readyMask == 0x2U ? 0 : 1;
    failed += PlatformWake_consume(&l_inputWake_) == 0 ? 0 : 1;
    failed += PlatformSemaphore_wait(&l_workerDone_) == 0 ? 0 : 1;
    failed += PlatformThread_join(&l_worker_) == 0 ? 0 : 1;

    uint64_t before;
    uint64_t after;
    failed += Platform_monotonicMs(&before) == 0 ? 0 : 1;
    Platform_delayMs(2U);
    failed += Platform_monotonicMs(&after) == 0 ? 0 : 1;
    failed += after >= before ? 0 : 1;

    PlatformBarrier_deinit(&l_workerStart_);
    PlatformSemaphore_deinit(&l_workerDone_);
    PlatformWake_deinit(&l_inputWake_);
    PlatformWake_deinit(&l_eventWake_);

    if (failed != 0) {
        fprintf(stderr, "platform_port_test: %d failure(s)\n", failed);
    }
    return failed == 0 ? 0 : 1;
}
