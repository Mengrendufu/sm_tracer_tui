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

#include <stdbool.h>
#include <stddef.h>
#include "sp_thread/sp_thread.h"

// Open and configure one serial port. The runtime retains the port handle
// after success and releases every acquired resource after failure.
bool SerialPortRuntime_open(SerialConfig const *config);

// Close and release the retained serial-port handle. A close failure leaves
// the handle retained so the caller can retry.
bool SerialPortRuntime_close(void);

// Return an owned, double-NUL-terminated sequence of NUL-terminated names.
// The caller must free the result. Return NULL and size zero on failure.
char *SerialPortRuntime_listPorts(size_t *portNamesSize);

#endif // SERIAL_PORT_RUNTIME_PRIV_H_
