//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#ifndef SERIAL_PORT_RUNTIME_PRIV_H_
#define SERIAL_PORT_RUNTIME_PRIV_H_

#include <stddef.h>

// Return an owned, double-NUL-terminated sequence of NUL-terminated names.
// The caller must free the result. Return NULL and size zero on failure.
char *SerialPortRuntime_listPorts(size_t *portNamesSize);

#endif // SERIAL_PORT_RUNTIME_PRIV_H_
