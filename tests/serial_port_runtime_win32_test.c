//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "sp_thread/thread/serial_port_runtime_priv.h"

static bool portNameValid_(char const * const name) {
    if ((strlen(name) < 4U)
        || (name[0] != 'C') || (name[1] != 'O') || (name[2] != 'M')
        || !isdigit((unsigned char)name[3]))
    {
        return false;
    }
    for (size_t i = 4U; name[i] != '\0'; ++i) {
        if (!isdigit((unsigned char)name[i])) {
            return false;
        }
    }
    return true;
}

int main(void) {
    int failed = 0;

    failed += !PlatformWaitObject_isValid(
        SerialPortRuntime_waitObject()) ? 0 : 1;
    failed += !SerialPortRuntime_close() ? 0 : 1;

    size_t namesSize = 0U;
    char * const names = SerialPortRuntime_listPorts(&namesSize);
    failed += names != (char *)0 ? 0 : 1;
    if (names != (char *)0) {
        failed += namesSize >= 2U ? 0 : 1;
        failed += names[namesSize - 1U] == '\0' ? 0 : 1;
        failed += names[namesSize - 2U] == '\0' ? 0 : 1;
        for (char const *name = names; *name != '\0';
             name += strlen(name) + 1U)
        {
            failed += portNameValid_(name) ? 0 : 1;
        }
        free(names);
    }

    SerialConfig const invalid = {
        .portName = "COM9999",
        .baudRate = 115200,
        .dataBits = 8U,
        .stopBits = SERIAL_STOP_BITS_1,
        .parity = SERIAL_PARITY_NONE,
        .flowControl = SERIAL_FLOW_NONE,
    };
    failed += !SerialPortRuntime_open(&invalid) ? 0 : 1;
    failed += !PlatformWaitObject_isValid(
        SerialPortRuntime_waitObject()) ? 0 : 1;

    return failed == 0 ? 0 : 1;
}
