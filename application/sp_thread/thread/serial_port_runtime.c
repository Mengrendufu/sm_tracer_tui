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
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "platform_port.h"
#include "serial_port_runtime_priv.h"

#if defined(_WIN32)

#include <wchar.h>
#include <windows.h>

#define SERIAL_PORT_ENUM_CAPACITY_ 65536U
#define SERIAL_PORT_PATH_CAPACITY_ (SERIAL_PORT_NAME_LEN + 8U)

static HANDLE SerialPortRuntime_port_ = INVALID_HANDLE_VALUE;
static OVERLAPPED SerialPortRuntime_wait_;
static OVERLAPPED SerialPortRuntime_read_;
static DWORD SerialPortRuntime_eventMask_;
static bool SerialPortRuntime_waitPending_;

static bool SerialPortRuntime_isOpen_(void) {
    return SerialPortRuntime_port_ != INVALID_HANDLE_VALUE;
}

static bool SerialPortRuntime_mapStopBits_(
    SerialStopBits const stopBits,
    BYTE * const value)
{
    if (stopBits == SERIAL_STOP_BITS_1) {
        *value = ONESTOPBIT;
    } else if (stopBits == SERIAL_STOP_BITS_1_5) {
        *value = ONE5STOPBITS;
    } else if (stopBits == SERIAL_STOP_BITS_2) {
        *value = TWOSTOPBITS;
    } else {
        return false;
    }
    return true;
}

static bool SerialPortRuntime_mapParity_(SerialParity const parity,
                                         BYTE * const value)
{
    static BYTE const values[] = {
        NOPARITY,
        ODDPARITY,
        EVENPARITY,
        MARKPARITY,
        SPACEPARITY,
    };
    if ((unsigned)parity >= (sizeof(values) / sizeof(values[0]))) {
        return false;
    }
    *value = values[parity];
    return true;
}

static bool SerialPortRuntime_configure_(
    HANDLE const port,
    SerialConfig const * const config)
{
    if ((config->baudRate <= 0)
        || (config->dataBits < 5U)
        || (config->dataBits > 8U))
    {
        return false;
    }

    BYTE stopBits;
    BYTE parity;
    if (!SerialPortRuntime_mapStopBits_(config->stopBits, &stopBits)
        || !SerialPortRuntime_mapParity_(config->parity, &parity))
    {
        return false;
    }

    DCB dcb = {.DCBlength = sizeof(dcb)};
    if (GetCommState(port, &dcb) == FALSE) {
        return false;
    }

    dcb.BaudRate = (DWORD)config->baudRate;
    dcb.ByteSize = config->dataBits;
    dcb.Parity = parity;
    dcb.StopBits = stopBits;
    dcb.fBinary = TRUE;
    dcb.fParity = parity != NOPARITY;
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fDsrSensitivity = FALSE;
    dcb.fOutX = FALSE;
    dcb.fInX = FALSE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;
    dcb.fAbortOnError = FALSE;

    if (config->flowControl == SERIAL_FLOW_XON_XOFF) {
        dcb.fOutX = TRUE;
        dcb.fInX = TRUE;
    } else if (config->flowControl == SERIAL_FLOW_RTS_CTS) {
        dcb.fOutxCtsFlow = TRUE;
        dcb.fRtsControl = RTS_CONTROL_HANDSHAKE;
    } else if (config->flowControl == SERIAL_FLOW_DTR_DSR) {
        dcb.fOutxDsrFlow = TRUE;
        dcb.fDtrControl = DTR_CONTROL_HANDSHAKE;
    } else if (config->flowControl != SERIAL_FLOW_NONE) {
        return false;
    }

    COMMTIMEOUTS timeouts = {0};
    return (SetCommState(port, &dcb) != FALSE)
           && (SetCommTimeouts(port, &timeouts) != FALSE)
           && (PurgeComm(port, PURGE_RXABORT | PURGE_RXCLEAR
                               | PURGE_TXABORT | PURGE_TXCLEAR) != FALSE);
}

static bool SerialPortRuntime_armWait_(void) {
    if (!SerialPortRuntime_isOpen_()) {
        return false;
    }

    SerialPortRuntime_eventMask_ = 0U;
    SerialPortRuntime_waitPending_ = false;
    (void)ResetEvent(SerialPortRuntime_wait_.hEvent);

    BOOL const completed = WaitCommEvent(
        SerialPortRuntime_port_, &SerialPortRuntime_eventMask_,
        &SerialPortRuntime_wait_);
    if (completed != FALSE) {
        return SetEvent(SerialPortRuntime_wait_.hEvent) != FALSE;
    }
    if (GetLastError() == ERROR_IO_PENDING) {
        SerialPortRuntime_waitPending_ = true;
        return true;
    }
    return false;
}

static void SerialPortRuntime_release_(void) {
    if (SerialPortRuntime_wait_.hEvent != NULL) {
        (void)CloseHandle(SerialPortRuntime_wait_.hEvent);
    }
    if (SerialPortRuntime_read_.hEvent != NULL) {
        (void)CloseHandle(SerialPortRuntime_read_.hEvent);
    }
    memset(&SerialPortRuntime_wait_, 0,
           sizeof(SerialPortRuntime_wait_));
    memset(&SerialPortRuntime_read_, 0,
           sizeof(SerialPortRuntime_read_));
    SerialPortRuntime_eventMask_ = 0U;
    SerialPortRuntime_waitPending_ = false;
}

static bool SerialPortRuntime_makePath_(
    char const * const name,
    wchar_t * const path,
    size_t const capacity)
{
    wchar_t wideName[SERIAL_PORT_NAME_LEN];
    int const count = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, name, -1, wideName,
        (int)(sizeof(wideName) / sizeof(wideName[0])));
    if (count <= 0) {
        return false;
    }

    int written;
    if (wcsncmp(wideName, L"\\\\.\\", 4U) == 0) {
        written = swprintf(path, capacity, L"%ls", wideName);
    } else {
        written = swprintf(path, capacity, L"\\\\.\\%ls", wideName);
    }
    return (written > 0) && ((size_t)written < capacity);
}

bool SerialPortRuntime_open(SerialConfig const * const config) {
    if ((config == (SerialConfig const *)0)
        || SerialPortRuntime_isOpen_())
    {
        return false;
    }

    wchar_t path[SERIAL_PORT_PATH_CAPACITY_];
    if (!SerialPortRuntime_makePath_(config->portName, path,
                                     SERIAL_PORT_PATH_CAPACITY_))
    {
        return false;
    }

    HANDLE const port = CreateFileW(
        path, GENERIC_READ | GENERIC_WRITE, 0U,
        (LPSECURITY_ATTRIBUTES)0, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL);
    if (port == INVALID_HANDLE_VALUE) {
        return false;
    }

    SerialPortRuntime_wait_.hEvent = CreateEventW(
        (LPSECURITY_ATTRIBUTES)0, TRUE, FALSE, (LPCWSTR)0);
    SerialPortRuntime_read_.hEvent = CreateEventW(
        (LPSECURITY_ATTRIBUTES)0, TRUE, FALSE, (LPCWSTR)0);
    if ((SerialPortRuntime_wait_.hEvent == NULL)
        || (SerialPortRuntime_read_.hEvent == NULL))
    {
        (void)CloseHandle(port);
        SerialPortRuntime_release_();
        return false;
    }

    SerialPortRuntime_port_ = port;
    bool const opened = (SetupComm(port, 4096U, 4096U) != FALSE)
                        && SerialPortRuntime_configure_(port, config)
                        && (SetCommMask(port, EV_RXCHAR | EV_ERR) != FALSE)
                        && SerialPortRuntime_armWait_();
    if (!opened) {
        (void)SerialPortRuntime_close();
        return false;
    }
    return true;
}

bool SerialPortRuntime_reconfigure(
    SerialConfig const * const config)
{
    return (config != (SerialConfig const *)0)
           && SerialPortRuntime_isOpen_()
           && SerialPortRuntime_configure_(SerialPortRuntime_port_, config);
}

bool SerialPortRuntime_close(void) {
    if (!SerialPortRuntime_isOpen_()) {
        return false;
    }

    bool closed = true;
    if (SetCommMask(SerialPortRuntime_port_, 0U) == FALSE) {
        closed = false;
        (void)CancelIoEx(SerialPortRuntime_port_,
                         &SerialPortRuntime_wait_);
    }
    if (SerialPortRuntime_waitPending_) {
        DWORD transferred;
        if ((GetOverlappedResult(SerialPortRuntime_port_,
                                 &SerialPortRuntime_wait_,
                                 &transferred, TRUE) == FALSE)
            && (GetLastError() != ERROR_OPERATION_ABORTED))
        {
            closed = false;
        }
        SerialPortRuntime_waitPending_ = false;
    }

    if (CloseHandle(SerialPortRuntime_port_) == FALSE) {
        closed = false;
    }
    SerialPortRuntime_port_ = INVALID_HANDLE_VALUE;
    SerialPortRuntime_release_();
    return closed;
}

PlatformWaitObject SerialPortRuntime_waitObject(void) {
    if (!SerialPortRuntime_isOpen_()) {
        return PLATFORM_WAIT_OBJECT_INVALID;
    }
    return PlatformWaitObject_fromHandle(
        SerialPortRuntime_wait_.hEvent);
}

int SerialPortRuntime_read(uint8_t * const data,
                           size_t const capacity)
{
    if (!SerialPortRuntime_isOpen_()
        || (data == (uint8_t *)0)
        || (capacity == 0U)
        || (capacity > (size_t)MAXDWORD)
        || (capacity > (size_t)INT_MAX))
    {
        return -1;
    }

    if (SerialPortRuntime_waitPending_) {
        DWORD transferred;
        if (GetOverlappedResult(SerialPortRuntime_port_,
                                &SerialPortRuntime_wait_,
                                &transferred, FALSE) == FALSE)
        {
            if (GetLastError() == ERROR_IO_INCOMPLETE) {
                return 0;
            }
            return -1;
        }
        SerialPortRuntime_waitPending_ = false;
    }
    (void)ResetEvent(SerialPortRuntime_wait_.hEvent);

    DWORD errors;
    COMSTAT status;
    if (ClearCommError(SerialPortRuntime_port_, &errors, &status) == FALSE) {
        return -1;
    }

    DWORD bytesRead = 0U;
    DWORD const requested = status.cbInQue < (DWORD)capacity
        ? status.cbInQue : (DWORD)capacity;
    if (requested > 0U) {
        (void)ResetEvent(SerialPortRuntime_read_.hEvent);
        if (ReadFile(SerialPortRuntime_port_, data, requested, &bytesRead,
                     &SerialPortRuntime_read_) == FALSE)
        {
            if (GetLastError() != ERROR_IO_PENDING) {
                return -1;
            }
            if (GetOverlappedResult(SerialPortRuntime_port_,
                                    &SerialPortRuntime_read_,
                                    &bytesRead, TRUE) == FALSE)
            {
                return -1;
            }
        }
    }

    if (!SerialPortRuntime_armWait_()) {
        return -1;
    }
    return (int)bytesRead;
}

static bool SerialPortRuntime_isComName_(wchar_t const * const name) {
    if ((name[0] != L'C') || (name[1] != L'O') || (name[2] != L'M')
        || (name[3] < L'0') || (name[3] > L'9'))
    {
        return false;
    }
    for (size_t i = 4U; name[i] != L'\0'; ++i) {
        if ((name[i] < L'0') || (name[i] > L'9')) {
            return false;
        }
    }
    return true;
}

char *SerialPortRuntime_listPorts(size_t * const portNamesSize) {
    if (portNamesSize == (size_t *)0) {
        return (char *)0;
    }
    *portNamesSize = 0U;

    wchar_t * const devices = (wchar_t *)malloc(
        SERIAL_PORT_ENUM_CAPACITY_ * sizeof(wchar_t));
    if (devices == (wchar_t *)0) {
        return (char *)0;
    }
    DWORD const length = QueryDosDeviceW(
        (LPCWSTR)0, devices, SERIAL_PORT_ENUM_CAPACITY_);
    if (length == 0U) {
        free(devices);
        return (char *)0;
    }

    size_t size = 1U;
    size_t count = 0U;
    for (wchar_t const *name = devices; *name != L'\0';
         name += wcslen(name) + 1U)
    {
        if (!SerialPortRuntime_isComName_(name)) {
            continue;
        }
        int const bytes = WideCharToMultiByte(
            CP_UTF8, WC_ERR_INVALID_CHARS, name, -1,
            (char *)0, 0, (char const *)0, (BOOL *)0);
        if ((bytes <= 0) || ((size_t)bytes > (SIZE_MAX - size))) {
            free(devices);
            return (char *)0;
        }
        size += (size_t)bytes;
        ++count;
    }
    if (count == 0U) {
        size = 2U;
    }

    char * const result = (char *)malloc(size);
    if (result == (char *)0) {
        free(devices);
        return (char *)0;
    }

    size_t offset = 0U;
    for (wchar_t const *name = devices; *name != L'\0';
         name += wcslen(name) + 1U)
    {
        if (!SerialPortRuntime_isComName_(name)) {
            continue;
        }
        int const bytes = WideCharToMultiByte(
            CP_UTF8, WC_ERR_INVALID_CHARS, name, -1,
            &result[offset], (int)(size - offset),
            (char const *)0, (BOOL *)0);
        if (bytes <= 0) {
            free(result);
            free(devices);
            return (char *)0;
        }
        offset += (size_t)bytes;
    }
    result[offset] = '\0';
    if (count == 0U) {
        result[1] = '\0';
    }

    free(devices);
    *portNamesSize = size;
    return result;
}

#else

#include "libserialport.h"

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

static bool SerialPortRuntime_configure_(
    struct sp_port * const port,
    SerialConfig const * const config,
    int const stopBits,
    enum sp_parity const parity,
    enum sp_flowcontrol const flowControl)
{
    return (sp_set_baudrate(port, config->baudRate) == SP_OK)
           && (sp_set_bits(port, config->dataBits) == SP_OK)
           && (sp_set_stopbits(port, stopBits) == SP_OK)
           && (sp_set_parity(port, parity) == SP_OK)
           && (sp_set_flowcontrol(port, flowControl) == SP_OK)
           && (sp_flush(port, SP_BUF_BOTH) == SP_OK);
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

    bool const configured = SerialPortRuntime_configure_(
        port, config, stopBits, parity, flowControl);
    if (!configured) {
        (void)sp_close(port);
        sp_free_port(port);
        return false;
    }

    int descriptor = -1;
    if (sp_get_port_handle(port, &descriptor) != SP_OK) {
        (void)sp_close(port);
        sp_free_port(port);
        return false;
    }

    SerialPortRuntime_port_ = port;
    SerialPortRuntime_fd_ = descriptor;
    return true;
}

bool SerialPortRuntime_reconfigure(
    SerialConfig const * const config)
{
    if ((config == (SerialConfig const *)0)
        || (SerialPortRuntime_port_ == (struct sp_port *)0))
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

    return SerialPortRuntime_configure_(SerialPortRuntime_port_, config,
                                        stopBits, parity, flowControl);
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

PlatformWaitObject SerialPortRuntime_waitObject(void) {
    return PlatformWaitObject_fromDescriptor(SerialPortRuntime_fd_);
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

#endif
