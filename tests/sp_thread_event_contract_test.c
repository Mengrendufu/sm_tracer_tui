//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#include <string.h>
#include "app_sig.h"
#include "sp_thread/sp_thread.h"
#include "sp_thread/thread/sp_thread_evt_priv.h"

int main(void) {
    int failed = SpThread_evtInit() == 0 ? 0 : 1;

    SerialConfig config = {
        .portName = "/dev/ttyTEST0",
        .baudRate = 115200,
        .dataBits = 8U,
        .stopBits = SERIAL_STOP_BITS_1,
        .parity = SERIAL_PARITY_NONE,
        .flowControl = SERIAL_FLOW_NONE,
    };
    SpThread_postOpenPort(&config);

    config.portName[0] = '\0';
    config.baudRate = 0;

    failed += SpThread_evtConsumeWake() == 0 ? 0 : 1;

    SpThreadEvt e;
    failed += SpThread_evtDequeue(&e) ? 0 : 1;
    failed += e.sig == SPTHRD_OPEN_PORT_SIG ? 0 : 1;
    failed += strcmp(e.config.portName, "/dev/ttyTEST0") == 0
              ? 0 : 1;
    failed += e.config.baudRate == 115200 ? 0 : 1;
    failed += e.config.dataBits == 8U ? 0 : 1;
    failed += e.config.stopBits == SERIAL_STOP_BITS_1 ? 0 : 1;
    failed += e.config.parity == SERIAL_PARITY_NONE ? 0 : 1;
    failed += e.config.flowControl == SERIAL_FLOW_NONE ? 0 : 1;
    failed += SPMNGR_PORT_OPENED_SIG != SPMNGR_PORT_OPEN_FAILED_SIG
              ? 0 : 1;
    failed += SPMNGR_PORT_ALREADY_CONNECTED_SIG
              != SPMNGR_PORT_OPENED_SIG ? 0 : 1;

    SpThread_postClosePort();
    failed += SpThread_evtConsumeWake() == 0 ? 0 : 1;
    failed += SpThread_evtDequeue(&e) ? 0 : 1;
    failed += e.sig == SPTHRD_CLOSE_PORT_SIG ? 0 : 1;

    SpThread_evtDeinit();
    return failed == 0 ? 0 : 1;
}
