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

#include "ui_evt.h"

//============================================================================
//=== UI event runtime — private to the UI package

// Initialize the event subsystem; return 0 on success.
int UI_evtInit(void);

// Return the borrowed eventfd for poll registration (call after UI_evtInit).
int UI_evtWakeFd(void);

// Consume a pending eventfd notification; return 0 on success.
int UI_evtConsumeWake(void);

// Dequeue one event (returns NULL if empty).
UI_Evt *UI_evtDequeue(void);

// Free an event (mirrors the internal allocator).
void UI_evtFree(UI_Evt *e);

#endif // UI_EVT_PRIV_H_
