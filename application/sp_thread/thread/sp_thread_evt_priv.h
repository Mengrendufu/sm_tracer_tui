//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#ifndef SP_THREAD_EVT_PRIV_H_
#define SP_THREAD_EVT_PRIV_H_

#include <stdbool.h>
#include "platform_port.h"
#include "sp_thread/hsm/sm_sp_thread.h"

int SpThread_evtInit(void);
void SpThread_evtDeinit(void);
PlatformWaitObject SpThread_evtWakeObject(void);
int SpThread_evtConsumeWake(void);
bool SpThread_evtDequeue(SpThreadEvt *e);

#endif // SP_THREAD_EVT_PRIV_H_
