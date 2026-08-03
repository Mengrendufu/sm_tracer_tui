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

// Return an owned, display-ready port-list string. The caller must free the
// result. Return NULL when libserialport cannot enumerate the system ports.
char *SerialPortRuntime_listPortsText(void);

#endif // SERIAL_PORT_RUNTIME_PRIV_H_
