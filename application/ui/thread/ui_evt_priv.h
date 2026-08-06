//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#ifndef UI_EVT_PRIV_H_
#define UI_EVT_PRIV_H_

#include "platform_port.h"
#include "hsm/sm_ui_evt.h"

//============================================================================
//=== UI event runtime — private to the UI package

void UI_evtPostSignal(UI_Signal sig);
void UI_evtPostText(UI_Signal sig, char const *text);
// UI-thread-local enqueue; terminal readiness already woke the loop.
void UI_evtEnqueueInput(UI_Signal sig, UI_Input const *input);

// Initialize the event subsystem; return 0 on success.
int UI_evtInit(void);

// Return the borrowed wait object (call after UI_evtInit).
PlatformWaitObject UI_evtWakeObject(void);

// Consume a pending wake notification; return 0 on success.
int UI_evtConsumeWake(void);

// Dequeue one event (returns NULL if empty).
UI_Evt *UI_evtDequeue(void);

// Free an event (mirrors the internal allocator).
void UI_evtFree(UI_Evt *e);

#endif // UI_EVT_PRIV_H_
