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
#include <stdint.h>
#include "platform_port.h"
#include "sp_thread/sp_thread.h"

// Open and configure one serial port. The runtime retains the port handle
// after success and releases every acquired resource after failure.
bool SerialPortRuntime_open(SerialConfig const *config);

// Apply a complete configuration to the retained open port. The caller must
// close the port after failure because its effective configuration is
// unknown.
bool SerialPortRuntime_reconfigure(SerialConfig const *config);

// Best-effort close the retained port, then always release local ownership.
// Return whether the operating-system close operation succeeded.
bool SerialPortRuntime_close(void);

// Return the wait object borrowed from the retained open port, or an invalid
// object while no port is open. The runtime retains native ownership.
PlatformWaitObject SerialPortRuntime_waitObject(void);

// Read at most capacity bytes without blocking. Return a byte count, zero
// when no bytes are available, or a negative runtime error.
int SerialPortRuntime_read(uint8_t *data, size_t capacity);

// Return an owned, double-NUL-terminated sequence of NUL-terminated names.
// The caller must free the result. Return NULL and size zero on failure.
char *SerialPortRuntime_listPorts(size_t *portNamesSize);

#endif // SERIAL_PORT_RUNTIME_PRIV_H_
