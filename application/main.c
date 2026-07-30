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
//=== Application entry: notcurses init, SST thread, UI loop
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <pthread.h>
#include "sst.h"
#include "ui.h"
#include "sp_thread/sp_thread.h"
#include "dbc_assert.h"

//============================================================================
// DBC_MODULE_NAME("main")

//============================================================================
//=== SST kernel thread

static void *SST_thread(void *arg) {
    (void)arg;
    SST_Task_run();
    return NULL;
}
static pthread_t SST_tid;

//============================================================================
//=== Main entry

int main(int const argc, char const ** const argv) {
    (void)argc; (void)argv;

    //------------------------------------------------------------------------
    setlocale(LC_ALL, "");

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
    pthread_create(&SST_tid, NULL, SST_thread, NULL);

    // main thread -----------------------------------------------------------
    if (UI_run() != 0) {
        return 1;
    }
    printf("Goodbye!\n");

    return 0;
}
