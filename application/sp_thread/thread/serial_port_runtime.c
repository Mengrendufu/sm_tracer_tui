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
#include "serial_port_platform.h"
#include "serial_port_runtime_priv.h"

static struct sp_port *SerialPortRuntime_port_;
static struct sp_event_set *SerialPortRuntime_eventSet_;
static SerialConfig SerialPortRuntime_appliedConfig_;
static bool SerialPortRuntime_configValid_;

static void SerialPortRuntime_storeConfig_(
    SerialConfig const * const config)
{
    SerialPortRuntime_appliedConfig_ = *config;
    SerialPortRuntime_configValid_ = true;
}

static void SerialPortRuntime_clearConfig_(void) {
    memset(&SerialPortRuntime_appliedConfig_, 0,
           sizeof(SerialPortRuntime_appliedConfig_));
    SerialPortRuntime_configValid_ = false;
}

static bool SerialPortRuntime_mapParity_(
    SerialParity const source,
    enum sp_parity * const target)
{
    static enum sp_parity const values[] = {
        SP_PARITY_NONE,
        SP_PARITY_ODD,
        SP_PARITY_EVEN,
        SP_PARITY_MARK,
        SP_PARITY_SPACE,
    };
    if ((unsigned)source >= (sizeof(values) / sizeof(values[0]))) {
        return false;
    }
    *target = values[source];
    return true;
}

static bool SerialPortRuntime_mapFlowControl_(
    SerialFlowControl const source,
    enum sp_flowcontrol * const target)
{
    static enum sp_flowcontrol const values[] = {
        SP_FLOWCONTROL_NONE,
        SP_FLOWCONTROL_XONXOFF,
        SP_FLOWCONTROL_RTSCTS,
        SP_FLOWCONTROL_DTRDSR,
    };
    if ((unsigned)source >= (sizeof(values) / sizeof(values[0]))) {
        return false;
    }
    *target = values[source];
    return true;
}

static bool SerialPortRuntime_mapConfig_(
    SerialConfig const * const config,
    enum sp_parity * const parity,
    enum sp_flowcontrol * const flowControl)
{
    if ((config == (SerialConfig const *)0)
        || (config->portName[0] == '\0')
        || (config->baudRate <= 0)
        || (config->dataBits < 5U)
        || (config->dataBits > 8U)
        || ((config->stopBits != SERIAL_STOP_BITS_1)
            && (config->stopBits != SERIAL_STOP_BITS_1_5)
            && (config->stopBits != SERIAL_STOP_BITS_2)))
    {
        return false;
    }

    return SerialPortRuntime_mapParity_(config->parity, parity)
           && SerialPortRuntime_mapFlowControl_(
                  config->flowControl, flowControl);
}

static bool SerialPortRuntime_applyConfig_(
    struct sp_port * const port,
    SerialConfig const * const config,
    enum sp_parity const parity,
    enum sp_flowcontrol const flowControl)
{
    bool stopBitsApplied;
    if (config->stopBits == SERIAL_STOP_BITS_1_5) {
        stopBitsApplied =
            SerialPortPlatform_applyOneAndHalfStopBits(port);
    } else {
        int const stopBits = config->stopBits == SERIAL_STOP_BITS_1
                             ? 1 : 2;
        stopBitsApplied = sp_set_stopbits(port, stopBits) == SP_OK;
    }

    return stopBitsApplied
           && (sp_set_baudrate(port, config->baudRate) == SP_OK)
           && (sp_set_bits(port, config->dataBits) == SP_OK)
           && (sp_set_parity(port, parity) == SP_OK)
           && (sp_set_flowcontrol(port, flowControl) == SP_OK)
           && (sp_flush(port, SP_BUF_BOTH) == SP_OK);
}

static void SerialPortRuntime_release_(
    struct sp_port * const port,
    struct sp_event_set * const eventSet)
{
    sp_free_event_set(eventSet);
    if (port != (struct sp_port *)0) {
        (void)sp_close(port);
        sp_free_port(port);
    }
}

bool SerialPortRuntime_open(SerialConfig const * const config) {
    if (SerialPortRuntime_port_ != (struct sp_port *)0) {
        return false;
    }

    enum sp_parity parity;
    enum sp_flowcontrol flowControl;
    if (!SerialPortRuntime_mapConfig_(config, &parity, &flowControl)) {
        return false;
    }

    struct sp_port *port = (struct sp_port *)0;
    if ((sp_get_port_by_name(config->portName, &port) != SP_OK)
        || (port == (struct sp_port *)0))
    {
        return false;
    }
    if (sp_open(port, SP_MODE_READ_WRITE) != SP_OK) {
        sp_free_port(port);
        return false;
    }
    if (!SerialPortRuntime_applyConfig_(port, config,
                                        parity, flowControl))
    {
        SerialPortRuntime_release_(port, (struct sp_event_set *)0);
        return false;
    }

    struct sp_event_set *eventSet = (struct sp_event_set *)0;
    if ((sp_new_event_set(&eventSet) != SP_OK)
        || (eventSet == (struct sp_event_set *)0)
        || (sp_add_port_events(eventSet, port,
                               SP_EVENT_RX_READY
                               | SP_EVENT_ERROR) != SP_OK)
        || !PlatformWaitObject_isValid(
               SerialPortPlatform_eventWaitObject(eventSet)))
    {
        SerialPortRuntime_release_(port, eventSet);
        return false;
    }

    SerialPortRuntime_port_ = port;
    SerialPortRuntime_eventSet_ = eventSet;
    SerialPortRuntime_storeConfig_(config);
    return true;
}

bool SerialPortRuntime_reconfigure(
    SerialConfig const * const config)
{
    if (SerialPortRuntime_port_ == (struct sp_port *)0) {
        return false;
    }

    enum sp_parity parity;
    enum sp_flowcontrol flowControl;
    bool const configured =
        SerialPortRuntime_mapConfig_(config, &parity, &flowControl)
        && SerialPortRuntime_applyConfig_(
               SerialPortRuntime_port_, config, parity, flowControl);
    if (configured) {
        SerialPortRuntime_storeConfig_(config);
    }
    return configured;
}

bool SerialPortRuntime_getAppliedConfig(
    SerialConfig * const config)
{
    if ((config == (SerialConfig *)0)
        || !SerialPortRuntime_configValid_)
    {
        return false;
    }
    *config = SerialPortRuntime_appliedConfig_;
    return true;
}

bool SerialPortRuntime_close(void) {
    if (SerialPortRuntime_port_ == (struct sp_port *)0) {
        return false;
    }

    sp_free_event_set(SerialPortRuntime_eventSet_);
    SerialPortRuntime_eventSet_ = (struct sp_event_set *)0;
    bool const closed = sp_close(SerialPortRuntime_port_) == SP_OK;
    sp_free_port(SerialPortRuntime_port_);
    SerialPortRuntime_port_ = (struct sp_port *)0;
    SerialPortRuntime_clearConfig_();
    return closed;
}

PlatformWaitObject SerialPortRuntime_waitObject(void) {
    if (SerialPortRuntime_eventSet_ == (struct sp_event_set *)0) {
        return PLATFORM_WAIT_OBJECT_INVALID;
    }
    return SerialPortPlatform_eventWaitObject(
        SerialPortRuntime_eventSet_);
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

    size_t size = 1U;
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
