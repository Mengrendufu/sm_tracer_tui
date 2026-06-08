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
#include "dbc_assert.h"
DBC_MODULE_NAME("main")

//============================================================================
//=== SST kernel thread

static void *SST_thread(void *arg) {
    (void)arg;
    SST_Task_run();
    return NULL;
}
static pthread_t SST_tid;

//============================================================================
//=== notcurses

static struct notcurses_options opts = {
    .flags = NCOPTION_SUPPRESS_BANNERS
};
static struct notcurses *nc;

//============================================================================
//=== Main entry

int main(int argc, char **argv) {
    (void)argc; (void)argv;

    setlocale(LC_ALL, "");

    nc = notcurses_core_init(&opts, NULL);
    if (!nc) { fprintf(stderr, "notcurses init failed\n"); return 1; }

    UI_prepare(nc);

    pthread_create(&SST_tid, NULL, SST_thread, NULL);

    UI_loop();

    notcurses_stop(nc);
    printf("Goodbye!\n");
    return 0;
}
