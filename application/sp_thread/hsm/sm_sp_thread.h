//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#ifndef SM_SP_THREAD_H_
#define SM_SP_THREAD_H_

#include <stdint.h>
#include "sm_hsm.h"
#include "sp_thread/sp_thread.h"

//============================================================================
//=== SM_SpThread component

enum SpThreadSignals {
    SPTHRD_NULL_SIG = 0U,
    SPTHRD_REFRESH_PORTS_SIG,
    SPTHRD_OPEN_PORT_SIG,
    SPTHRD_CLOSE_PORT_SIG,
    SPTHRD_PORT_LOST_SIG
};

typedef struct {
    uint16_t sig;
    SerialConfig config;
} SpThreadEvt;

typedef struct {
    SM_Hsm super;
} SM_SpThread;

void SM_SpThread_ctor(SM_SpThread *me);
void SM_SpThread_init(SM_SpThread *me);
void SM_SpThread_dispatchEvt(SM_SpThread *me, SpThreadEvt const *e);

#endif // SM_SP_THREAD_H_
