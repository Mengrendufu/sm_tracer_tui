//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
//============================================================================
//=== Application entry: UI init, serial/SST startup, main-thread UI loop
#include <stdio.h>
#include <stdlib.h>
#include "sst.h"
#include "bsp.h"
#include "platform_port.h"
#include "ui.h"
#include "sp_thread/sp_thread.h"
#include "dbc_assert.h"

//============================================================================
// DBC_MODULE_NAME("main")

//============================================================================
//=== SST kernel thread

static void SST_thread_(void *arg) {
    (void)arg;
    SST_Task_run();
}
static PlatformThread SST_tid_ = PLATFORM_THREAD_INITIALIZER;

//============================================================================
//=== Main entry

int main(int const argc, char const ** const argv) {
    (void)argc; (void)argv;

    //------------------------------------------------------------------------
    if (Platform_consoleInit() != 0) {
        fprintf(stderr, "terminal runtime init failed\n");
        return 1;
    }

    // resource init of UI ---------------------------------------------------
    if (UI_init() != 0) {
        fprintf(stderr, "UI init failed\n");
        return 1;
    }

    // init of SerialPort thread ---------------------------------------------
    if (SpThread_start() != 0) {
        fprintf(stderr, "SpThread start failed\n");
        return 1;
    }

    // AOs -------------------------------------------------------------------
    SST_init();
    if (PlatformThread_start(&SST_tid_, &SST_thread_, (void *)0) != 0) {
        fprintf(stderr, "SST thread start failed\n");
        return 1;
    }
    BSP_waitForSSTStart();

    // main thread -----------------------------------------------------------
    if (UI_run() != 0) {
        return 1;
    }
    printf("Goodbye!\n");

    return 0;
}
