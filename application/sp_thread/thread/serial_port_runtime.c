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
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "libserialport.h"
#include "serial_port_runtime_priv.h"

static struct sp_port *SerialPortRuntime_port_;
static int SerialPortRuntime_fd_ = -1;

static bool SerialPortRuntime_mapStopBits_(
    SerialStopBits const stopBits,
    int * const value)
{
    if (stopBits == SERIAL_STOP_BITS_1) {
        *value = 1;
        return true;
    } else if (stopBits == SERIAL_STOP_BITS_2) {
        *value = 2;
        return true;
    } else {
        return false;
    }
}

static bool SerialPortRuntime_mapParity_(
    SerialParity const parity,
    enum sp_parity * const value)
{
    static enum sp_parity const values[] = {
        SP_PARITY_NONE,
        SP_PARITY_ODD,
        SP_PARITY_EVEN,
        SP_PARITY_MARK,
        SP_PARITY_SPACE,
    };
    if ((unsigned)parity >= (sizeof(values) / sizeof(values[0]))) {
        return false;
    }
    *value = values[parity];
    return true;
}

static bool SerialPortRuntime_mapFlowControl_(
    SerialFlowControl const flowControl,
    enum sp_flowcontrol * const value)
{
    static enum sp_flowcontrol const values[] = {
        SP_FLOWCONTROL_NONE,
        SP_FLOWCONTROL_XONXOFF,
        SP_FLOWCONTROL_RTSCTS,
        SP_FLOWCONTROL_DTRDSR,
    };
    if ((unsigned)flowControl
        >= (sizeof(values) / sizeof(values[0])))
    {
        return false;
    }
    *value = values[flowControl];
    return true;
}

bool SerialPortRuntime_open(SerialConfig const * const config) {
    if ((config == (SerialConfig const *)0)
        || (SerialPortRuntime_port_ != (struct sp_port *)0))
    {
        return false;
    }

    int stopBits;
    enum sp_parity parity;
    enum sp_flowcontrol flowControl;
    if (!SerialPortRuntime_mapStopBits_(config->stopBits, &stopBits)
        || !SerialPortRuntime_mapParity_(config->parity, &parity)
        || !SerialPortRuntime_mapFlowControl_(config->flowControl,
                                              &flowControl))
    {
        return false;
    }

    struct sp_port *port = (struct sp_port *)0;
    if (sp_get_port_by_name(config->portName, &port) != SP_OK) {
        return false;
    }
    if (sp_open(port, SP_MODE_READ_WRITE) != SP_OK) {
        sp_free_port(port);
        return false;
    }

    bool const configured =
        (sp_set_baudrate(port, config->baudRate) == SP_OK)
        && (sp_set_bits(port, config->dataBits) == SP_OK)
        && (sp_set_stopbits(port, stopBits) == SP_OK)
        && (sp_set_parity(port, parity) == SP_OK)
        && (sp_set_flowcontrol(port, flowControl) == SP_OK)
        && (sp_flush(port, SP_BUF_BOTH) == SP_OK);
    if (!configured) {
        (void)sp_close(port);
        sp_free_port(port);
        return false;
    }

    int fd = -1;
    if (sp_get_port_handle(port, &fd) != SP_OK) {
        (void)sp_close(port);
        sp_free_port(port);
        return false;
    }

    SerialPortRuntime_port_ = port;
    SerialPortRuntime_fd_ = fd;
    return true;
}

bool SerialPortRuntime_close(void) {
    if (SerialPortRuntime_port_ == (struct sp_port *)0) {
        return false;
    }

    bool const closed = sp_close(SerialPortRuntime_port_) == SP_OK;
    sp_free_port(SerialPortRuntime_port_);
    SerialPortRuntime_port_ = (struct sp_port *)0;
    SerialPortRuntime_fd_ = -1;
    return closed;
}

int SerialPortRuntime_fd(void) {
    return SerialPortRuntime_fd_;
}

int SerialPortRuntime_read(uint8_t * const data,
                           size_t const capacity)
{
    if ((SerialPortRuntime_port_ == (struct sp_port *)0)
        || (data == (uint8_t *)0)
        || (capacity == 0U))
    {
        return SP_ERR_ARG;
    }

    return sp_nonblocking_read(SerialPortRuntime_port_, data, capacity);
}

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
