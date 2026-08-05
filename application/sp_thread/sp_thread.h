//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#ifndef SP_THREAD_H_
#define SP_THREAD_H_

#include <stdint.h>

//============================================================================
//=== Serial-port operation contract

enum {
    SERIAL_PORT_NAME_LEN = 256U
};

typedef enum {
    SERIAL_STOP_BITS_1,
    SERIAL_STOP_BITS_1_5,
    SERIAL_STOP_BITS_2
} SerialStopBits;

typedef enum {
    SERIAL_PARITY_NONE,
    SERIAL_PARITY_ODD,
    SERIAL_PARITY_EVEN,
    SERIAL_PARITY_MARK,
    SERIAL_PARITY_SPACE
} SerialParity;

typedef enum {
    SERIAL_FLOW_NONE,
    SERIAL_FLOW_XON_XOFF,
    SERIAL_FLOW_RTS_CTS,
    SERIAL_FLOW_DTR_DSR
} SerialFlowControl;

typedef struct {
    char portName[SERIAL_PORT_NAME_LEN];
    int baudRate;
    uint8_t dataBits;
    SerialStopBits stopBits;
    SerialParity parity;
    SerialFlowControl flowControl;
} SerialConfig;

//============================================================================
//=== SpThread lifecycle

// Start the serial-port thread in its disconnected blocking state.
// Returns zero on success and nonzero on an operating-system failure.
int SpThread_start(void);

// Copy and post a serial-port-open request to the thread event inbox.
void SpThread_postOpenPort(SerialConfig const *config);

// Copy and post a configuration request for an already-open serial port.
void SpThread_postApplyConfig(SerialConfig const *config);

// Post a serial-port-close request to the thread event inbox.
void SpThread_postClosePort(void);

// Post a serial-port-list refresh request to the thread event inbox.
void SpThread_postRefreshPorts(void);

#endif // SP_THREAD_H_
