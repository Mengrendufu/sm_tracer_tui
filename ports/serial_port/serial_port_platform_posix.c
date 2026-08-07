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
//=== Component: SerialPortPlatform
#include "libserialport.h"
#include "serial_port_platform.h"

PlatformWaitObject SerialPortPlatform_eventWaitObject(
    struct sp_event_set const * const eventSet)
{
    if ((eventSet == (struct sp_event_set const *)0)
        || (eventSet->count != 1U)
        || (eventSet->handles == (void *)0))
    {
        return PLATFORM_WAIT_OBJECT_INVALID;
    }

    int const descriptor = ((int const *)eventSet->handles)[0];
    return PlatformWaitObject_fromDescriptor(descriptor);
}

bool SerialPortPlatform_applyOneAndHalfStopBits(
    struct sp_port * const port)
{
    (void)port;
    return false;
}
