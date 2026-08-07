//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#ifndef SERIAL_PORT_PLATFORM_H_
#define SERIAL_PORT_PLATFORM_H_

#include "platform_port.h"

//============================================================================
//=== Component: SerialPortPlatform

struct sp_event_set;
struct sp_port;

// Convert libserialport's single RX event into the common wait-object type.
PlatformWaitObject SerialPortPlatform_eventWaitObject(
    struct sp_event_set const *eventSet);

// Preserve the Win32-only 1.5 stop-bit capability. POSIX reports unsupported.
bool SerialPortPlatform_applyOneAndHalfStopBits(struct sp_port *port);

#endif // SERIAL_PORT_PLATFORM_H_
