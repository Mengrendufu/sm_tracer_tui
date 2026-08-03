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
    char *text = SerialPortRuntime_listPortsText();
    failed += text != (char *)0
              && strcmp(text,
                        "Serial ports:\n"
                        "/dev/ttyUSB0\n"
                        "/dev/ttyACM0\n") == 0
              ? 0 : 1;
    free(text);

    l_mode_ = LIST_MODE_EMPTY_;
    text = SerialPortRuntime_listPortsText();
    failed += text != (char *)0
              && strcmp(text, "Serial ports: none\n") == 0
              ? 0 : 1;
    free(text);

    l_mode_ = LIST_MODE_ERROR_;
    text = SerialPortRuntime_listPortsText();
    failed += text == (char *)0 ? 0 : 1;

    return failed == 0 ? 0 : 1;
}
