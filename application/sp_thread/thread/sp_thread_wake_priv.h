//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#ifndef SP_THREAD_WAKE_PRIV_H_
#define SP_THREAD_WAKE_PRIV_H_

enum SpThreadWakeResult {
    SP_THREAD_WAKE_ERROR = -1,
    SP_THREAD_WAKE_EVENT = 1
};

void SpThreadWake_init(int eventFd);
int SpThreadWake_wait(void);

#endif // SP_THREAD_WAKE_PRIV_H_
