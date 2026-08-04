//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#ifndef APP_SIG_H_
#define APP_SIG_H_

enum AppSignals {
    APP_SIG_DUMMY = 0U,

    // published SST_Evt topics
    MAX_PUB_SIG,

    // global non-published SST_Evt signals
    BLINKY_TIMEOUT_SIG,

    SPMNGR_CONFIG_UPDATE_SIG,
    SPMNGR_PORT_CONNECT_SIG,
    SPMNGR_PORT_OPENED_SIG,
    SPMNGR_PORT_OPEN_FAILED_SIG,
    SPMNGR_PORT_DISCONNECT_SIG,
    SPMNGR_PORT_CLOSED_SIG,
    SPMNGR_PORT_CLOSE_FAILED_SIG,
    SPMNGR_REFRESH_PORTS_SIG,
    SPMNGR_REFRESHED_PORTS_SIG,
    SPMNGR_RX_PACKET_SIG,

    MAX_SIG
};

#endif // APP_SIG_H_
