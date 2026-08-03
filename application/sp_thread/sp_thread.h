//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#ifndef SP_THREAD_H_
#define SP_THREAD_H_

//============================================================================
//=== SpThread lifecycle

// Start the serial-port thread in its disconnected blocking state.
// Returns zero on success and nonzero on an operating-system failure.
int SpThread_start(void);

// Post a serial-port-list refresh request to the thread event inbox.
void SpThread_postRefreshPorts(void);

#endif // SP_THREAD_H_
