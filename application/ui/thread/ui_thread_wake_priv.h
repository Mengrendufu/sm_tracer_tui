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
//=== UI Thread Wake contract — private only to application/ui/thread

#ifndef UI_THREAD_WAKE_PRIV_H_
#define UI_THREAD_WAKE_PRIV_H_

enum {
    UI_THREAD_WAKE_TIMEOUT  = 0U,
    UI_THREAD_WAKE_TERMINAL = 1U << 0U,
    UI_THREAD_WAKE_EVENT    = 1U << 1U,
};

// Bind the terminal-input and event-wake required inputs. Their providers
// retain descriptor ownership and lifetime.
void UI_ThreadWake_init(int terminalFd, int eventFd);

// Accept the Frame Clock deadline and return ready-source bits, zero on
// timeout, or -1 with errno on poll failure.
int  UI_ThreadWake_wait(int timeout);

#endif // UI_THREAD_WAKE_PRIV_H_
