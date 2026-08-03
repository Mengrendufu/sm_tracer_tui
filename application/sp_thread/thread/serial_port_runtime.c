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
//=== Component: SerialPortRuntime
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "libserialport.h"
#include "serial_port_runtime_priv.h"

char *SerialPortRuntime_listPorts(size_t * const portNamesSize) {
    if (portNamesSize == (size_t *)0) {
        return (char *)0;
    }
    *portNamesSize = 0U;

    struct sp_port **ports = (struct sp_port **)0;
    if (sp_list_ports(&ports) != SP_OK) {
        return (char *)0;
    }

    size_t size = 1U; // final terminator after the last name
    size_t count = 0U;
    for (size_t i = 0U; ports[i] != (struct sp_port *)0; ++i) {
        char const * const name = sp_get_port_name(ports[i]);
        if (name == (char const *)0) {
            sp_free_port_list(ports);
            return (char *)0;
        }
        size_t const nameSize = strlen(name) + 1U;
        if (nameSize > (SIZE_MAX - size)) {
            sp_free_port_list(ports);
            return (char *)0;
        }
        size += nameSize;
        ++count;
    }
    if (count == 0U) {
        size = 2U;
    }

    char * const portNames = (char *)malloc(size);
    if (portNames == (char *)0) {
        sp_free_port_list(ports);
        return (char *)0;
    }

    size_t offset = 0U;
    for (size_t i = 0U; ports[i] != (struct sp_port *)0; ++i) {
        char const * const name = sp_get_port_name(ports[i]);
        size_t const nameSize = strlen(name) + 1U;
        memcpy(&portNames[offset], name, nameSize);
        offset += nameSize;
    }
    portNames[offset] = '\0';
    if (count == 0U) {
        portNames[1] = '\0';
    }

    sp_free_port_list(ports);
    *portNamesSize = size;
    return portNames;
}
