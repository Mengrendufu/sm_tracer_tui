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

static char *SerialPortRuntime_copyText_(char const * const text) {
    size_t const size = strlen(text) + 1U;
    char * const copy = (char *)malloc(size);
    if (copy != (char *)0) {
        memcpy(copy, text, size);
    }
    return copy;
}

char *SerialPortRuntime_listPortsText(void) {
    static char const heading[] = "Serial ports:\n";
    static char const empty[] = "Serial ports: none\n";

    struct sp_port **ports = (struct sp_port **)0;
    if (sp_list_ports(&ports) != SP_OK) {
        return (char *)0;
    }

    if (ports[0] == (struct sp_port *)0) {
        sp_free_port_list(ports);
        return SerialPortRuntime_copyText_(empty);
    }

    size_t textSize = sizeof(heading);
    for (size_t i = 0U; ports[i] != (struct sp_port *)0; ++i) {
        char const * const name = sp_get_port_name(ports[i]);
        if ((name == (char const *)0)
            || (strlen(name) > (SIZE_MAX - textSize - 1U)))
        {
            sp_free_port_list(ports);
            return (char *)0;
        }
        textSize += strlen(name) + 1U;
    }

    char * const text = (char *)malloc(textSize);
    if (text == (char *)0) {
        sp_free_port_list(ports);
        return (char *)0;
    }

    size_t offset = sizeof(heading) - 1U;
    memcpy(text, heading, offset);
    for (size_t i = 0U; ports[i] != (struct sp_port *)0; ++i) {
        char const * const name = sp_get_port_name(ports[i]);
        size_t const nameSize = strlen(name);
        memcpy(&text[offset], name, nameSize);
        offset += nameSize;
        text[offset] = '\n';
        ++offset;
    }
    text[offset] = '\0';

    sp_free_port_list(ports);
    return text;
}
