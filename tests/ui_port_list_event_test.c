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
#include "ui_evt.h"
#include "ui/thread/ui_evt_priv.h"

int main(void) {
    int failed = UI_evtInit() == 0 ? 0 : 1;

    char portNames[] = "COM3\0COM5\0";
    UI_postPortList(portNames, sizeof(portNames));
    portNames[0] = 'X';

    failed += UI_evtConsumeWake() == 0 ? 0 : 1;
    UI_Evt * const e = UI_evtDequeue();
    failed += e != (UI_Evt *)0 ? 0 : 1;

    if (e != (UI_Evt *)0) {
        UI_PortListEvt const * const ports =
            (UI_PortListEvt const *)e;
        char const expected[] = "COM3\0COM5\0";
        failed += ports->super.sig == UI_REFRESHED_PORTS_SIG ? 0 : 1;
        failed += ports->portNamesSize == sizeof(expected) ? 0 : 1;
        failed += memcmp(ports->portNames, expected,
                         sizeof(expected)) == 0
                  ? 0 : 1;
        UI_evtFree(e);
    }

    char protocolPaths[] = "base/hdlc.json\0custom/raw.json\0";
    UI_postProtocolList(protocolPaths, sizeof(protocolPaths));
    protocolPaths[0] = 'X';

    failed += UI_evtConsumeWake() == 0 ? 0 : 1;
    UI_Evt * const protocolsEvt = UI_evtDequeue();
    failed += protocolsEvt != (UI_Evt *)0 ? 0 : 1;
    if (protocolsEvt != (UI_Evt *)0) {
        UI_ProtocolListEvt const * const protocols =
            (UI_ProtocolListEvt const *)protocolsEvt;
        char const expected[] =
            "base/hdlc.json\0custom/raw.json\0";
        failed += protocols->super.sig == UI_REFRESHED_PROTOCOLS_SIG
                  ? 0 : 1;
        failed += protocols->protocolPathsSize == sizeof(expected)
                  ? 0 : 1;
        failed += memcmp(protocols->protocolPaths, expected,
                         sizeof(expected)) == 0
                  ? 0 : 1;
        UI_evtFree(protocolsEvt);
    }

    UI_postProtocolLoaded("base/hdlc.json");
    failed += UI_evtConsumeWake() == 0 ? 0 : 1;
    UI_Evt * const loadedEvt = UI_evtDequeue();
    failed += loadedEvt != (UI_Evt *)0 ? 0 : 1;
    if (loadedEvt != (UI_Evt *)0) {
        UI_AppEvt const * const loaded =
            (UI_AppEvt const *)loadedEvt;
        failed += loaded->super.sig == UI_PROTOCOL_LOADED_SIG ? 0 : 1;
        failed += strcmp(loaded->pld.msg.text, "base/hdlc.json") == 0
                  ? 0 : 1;
        UI_evtFree(loadedEvt);
    }

    UI_postConnectionStatus(UI_CONNECTION_CONNECTED);
    failed += UI_evtConsumeWake() == 0 ? 0 : 1;
    UI_Evt * const connectionEvt = UI_evtDequeue();
    failed += connectionEvt != (UI_Evt *)0 ? 0 : 1;
    if (connectionEvt != (UI_Evt *)0) {
        UI_ConnectionEvt const * const connection =
            (UI_ConnectionEvt const *)connectionEvt;
        failed += connection->super.sig == UI_CONNECTION_STATUS_SIG
                  ? 0 : 1;
        failed += connection->status == UI_CONNECTION_CONNECTED
                  ? 0 : 1;
        UI_evtFree(connectionEvt);
    }

    return failed == 0 ? 0 : 1;
}
