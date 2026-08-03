//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#include <stdlib.h>
#include <string.h>
#include "libserialport.h"
#include "sp_thread/thread/serial_port_runtime_priv.h"

struct sp_port {
    char const *name;
};

enum ListMode_ {
    LIST_MODE_PORTS_,
    LIST_MODE_EMPTY_,
    LIST_MODE_ERROR_
};

static enum ListMode_ l_mode_;
static struct sp_port l_port0_ = {.name = "/dev/ttyUSB0"};
static struct sp_port l_port1_ = {.name = "/dev/ttyACM0"};
static struct sp_port *l_ports_[] = {
    &l_port0_, &l_port1_, (struct sp_port *)0
};
static struct sp_port *l_empty_[] = {(struct sp_port *)0};

enum sp_return sp_list_ports(struct sp_port *** const list) {
    if (l_mode_ == LIST_MODE_ERROR_) {
        *list = (struct sp_port **)0;
        return SP_ERR_FAIL;
    }
    *list = l_mode_ == LIST_MODE_EMPTY_ ? l_empty_ : l_ports_;
    return SP_OK;
}

char *sp_get_port_name(struct sp_port const * const port) {
    return (char *)port->name;
}

void sp_free_port_list(struct sp_port ** const ports) {
    (void)ports;
}

int main(void) {
    int failed = 0;

    l_mode_ = LIST_MODE_PORTS_;
    size_t portNamesSize = 0U;
    char *portNames = SerialPortRuntime_listPorts(&portNamesSize);
    char const expected[] =
        "/dev/ttyUSB0\0/dev/ttyACM0\0";
    failed += portNames != (char *)0
              && portNamesSize == sizeof(expected)
              && memcmp(portNames, expected, sizeof(expected)) == 0
              ? 0 : 1;
    free(portNames);

    l_mode_ = LIST_MODE_EMPTY_;
    portNamesSize = 0U;
    portNames = SerialPortRuntime_listPorts(&portNamesSize);
    char const empty[] = "\0";
    failed += portNames != (char *)0
              && portNamesSize == sizeof(empty)
              && memcmp(portNames, empty, sizeof(empty)) == 0
              ? 0 : 1;
    free(portNames);

    l_mode_ = LIST_MODE_ERROR_;
    portNamesSize = 123U;
    portNames = SerialPortRuntime_listPorts(&portNamesSize);
    failed += (portNames == (char *)0) && (portNamesSize == 0U)
              ? 0 : 1;

    return failed == 0 ? 0 : 1;
}
