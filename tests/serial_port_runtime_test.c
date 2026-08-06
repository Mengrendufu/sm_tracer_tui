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
#include <stdint.h>
#include <string.h>
#include "libserialport.h"
#include "sp_thread/sp_thread.h"
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
static enum sp_return l_openResult_ = SP_OK;
static enum sp_return l_closeResult_ = SP_OK;
static enum sp_return l_configResult_ = SP_OK;
static int l_openCalls_;
static int l_closeCalls_;
static int l_freeCalls_;
static int l_baudrate_;
static int l_dataBits_;
static int l_stopBits_;
static enum sp_parity l_parity_;
static enum sp_flowcontrol l_flowControl_;
static int l_portFd_ = 42;
static uint8_t l_readData_[] = {0x11U, 0x22U, 0x33U};

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

enum sp_return sp_get_port_by_name(
    char const * const name,
    struct sp_port ** const port)
{
    l_port0_.name = name;
    *port = &l_port0_;
    return SP_OK;
}

void sp_free_port(struct sp_port * const port) {
    (void)port;
    ++l_freeCalls_;
}

enum sp_return sp_open(struct sp_port * const port,
                       enum sp_mode const mode)
{
    (void)port;
    (void)mode;
    ++l_openCalls_;
    return l_openResult_;
}

enum sp_return sp_close(struct sp_port * const port) {
    (void)port;
    ++l_closeCalls_;
    return l_closeResult_;
}

enum sp_return sp_set_baudrate(struct sp_port * const port,
                               int const baudrate)
{
    (void)port;
    l_baudrate_ = baudrate;
    return l_configResult_;
}

enum sp_return sp_set_bits(struct sp_port * const port,
                           int const bits)
{
    (void)port;
    l_dataBits_ = bits;
    return l_configResult_;
}

enum sp_return sp_set_stopbits(struct sp_port * const port,
                               int const stopBits)
{
    (void)port;
    l_stopBits_ = stopBits;
    return l_configResult_;
}

enum sp_return sp_set_parity(struct sp_port * const port,
                             enum sp_parity const parity)
{
    (void)port;
    l_parity_ = parity;
    return l_configResult_;
}

enum sp_return sp_set_flowcontrol(
    struct sp_port * const port,
    enum sp_flowcontrol const flowControl)
{
    (void)port;
    l_flowControl_ = flowControl;
    return l_configResult_;
}

enum sp_return sp_flush(struct sp_port * const port,
                        enum sp_buffer const buffers)
{
    (void)port;
    (void)buffers;
    return l_configResult_;
}

enum sp_return sp_get_port_handle(struct sp_port const * const port,
                                  void * const resultPtr)
{
    (void)port;
    *((int *)resultPtr) = l_portFd_;
    return SP_OK;
}

enum sp_return sp_nonblocking_read(struct sp_port * const port,
                                   void * const data,
                                   size_t const capacity)
{
    (void)port;
    size_t const size = capacity < sizeof(l_readData_)
                        ? capacity : sizeof(l_readData_);
    memcpy(data, l_readData_, size);
    return (enum sp_return)size;
}

int main(void) {
    int failed = 0;
    SerialConfig appliedConfig;
    failed += !SerialPortRuntime_getAppliedConfig(&appliedConfig) ? 0 : 1;

    failed += !PlatformWaitObject_isValid(
        SerialPortRuntime_waitObject()) ? 0 : 1;

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

    SerialConfig const config = {
        .portName = "/dev/ttyUSB0",
        .baudRate = 115200,
        .dataBits = 8U,
        .stopBits = SERIAL_STOP_BITS_1,
        .parity = SERIAL_PARITY_EVEN,
        .flowControl = SERIAL_FLOW_RTS_CTS,
    };

    l_openResult_ = SP_ERR_FAIL;
    failed += !SerialPortRuntime_open(&config) ? 0 : 1;
    failed += (l_closeCalls_ == 0) && (l_freeCalls_ == 1)
              ? 0 : 1;

    l_openResult_ = SP_OK;
    l_configResult_ = SP_ERR_FAIL;
    failed += !SerialPortRuntime_open(&config) ? 0 : 1;
    failed += (l_closeCalls_ == 1) && (l_freeCalls_ == 2)
              ? 0 : 1;

    l_configResult_ = SP_OK;
    failed += SerialPortRuntime_open(&config) ? 0 : 1;
    failed += SerialPortRuntime_getAppliedConfig(&appliedConfig) ? 0 : 1;
    failed += memcmp(&appliedConfig, &config, sizeof(config)) == 0 ? 0 : 1;
    failed += strcmp(l_port0_.name, config.portName) == 0 ? 0 : 1;
    failed += l_openCalls_ == 3 ? 0 : 1;
    failed += l_baudrate_ == 115200 ? 0 : 1;
    failed += l_dataBits_ == 8 ? 0 : 1;
    failed += l_stopBits_ == 1 ? 0 : 1;
    failed += l_parity_ == SP_PARITY_EVEN ? 0 : 1;
    failed += l_flowControl_ == SP_FLOWCONTROL_RTSCTS ? 0 : 1;
    failed += PlatformWaitObject_equal(
        SerialPortRuntime_waitObject(),
        PlatformWaitObject_fromDescriptor(l_portFd_)) ? 0 : 1;

    SerialConfig updatedConfig = config;
    updatedConfig.baudRate = 9600;
    updatedConfig.dataBits = 7U;
    updatedConfig.parity = SERIAL_PARITY_ODD;
    int const openCallsBeforeReconfigure = l_openCalls_;
    failed += SerialPortRuntime_reconfigure(&updatedConfig) ? 0 : 1;
    failed += SerialPortRuntime_getAppliedConfig(&appliedConfig) ? 0 : 1;
    failed += memcmp(&appliedConfig, &updatedConfig,
                     sizeof(updatedConfig)) == 0 ? 0 : 1;
    failed += l_openCalls_ == openCallsBeforeReconfigure ? 0 : 1;
    failed += l_baudrate_ == 9600 ? 0 : 1;
    failed += l_dataBits_ == 7 ? 0 : 1;
    failed += l_parity_ == SP_PARITY_ODD ? 0 : 1;
    failed += PlatformWaitObject_equal(
        SerialPortRuntime_waitObject(),
        PlatformWaitObject_fromDescriptor(l_portFd_)) ? 0 : 1;

    l_configResult_ = SP_ERR_FAIL;
    failed += !SerialPortRuntime_reconfigure(&config) ? 0 : 1;
    failed += PlatformWaitObject_equal(
        SerialPortRuntime_waitObject(),
        PlatformWaitObject_fromDescriptor(l_portFd_)) ? 0 : 1;
    l_configResult_ = SP_OK;

    uint8_t readData[sizeof(l_readData_)] = {0};
    int const readSize = SerialPortRuntime_read(readData,
                                                sizeof(readData));
    failed += readSize == (int)sizeof(l_readData_)
              && memcmp(readData, l_readData_, sizeof(readData)) == 0
              ? 0 : 1;

    l_closeResult_ = SP_ERR_FAIL;
    failed += !SerialPortRuntime_close() ? 0 : 1;
    failed += !SerialPortRuntime_getAppliedConfig(&appliedConfig) ? 0 : 1;
    failed += (l_closeCalls_ == 2) && (l_freeCalls_ == 3)
              ? 0 : 1;
    failed += !PlatformWaitObject_isValid(
        SerialPortRuntime_waitObject()) ? 0 : 1;
    failed += !SerialPortRuntime_close() ? 0 : 1;
    failed += (l_closeCalls_ == 2) && (l_freeCalls_ == 3)
              ? 0 : 1;

    return failed == 0 ? 0 : 1;
}
