//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#include <stdint.h>
#include "libserialport.h"
#include "serial_port_platform.h"

int main(void) {
    int failed = 0;
    struct sp_event_set eventSet = {0};

    failed += !PlatformWaitObject_isValid(
        SerialPortPlatform_eventWaitObject(&eventSet)) ? 0 : 1;

#if defined(_WIN32)
    void *handles[] = {(void *)(uintptr_t)42U};
    eventSet.handles = handles;
#else
    int handles[] = {42};
    eventSet.handles = handles;
#endif
    eventSet.count = 1U;

    failed += PlatformWaitObject_isValid(
        SerialPortPlatform_eventWaitObject(&eventSet)) ? 0 : 1;

    return failed == 0 ? 0 : 1;
}
