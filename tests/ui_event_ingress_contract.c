#include "ui_evt.h"

void UI_eventIngress_contract(void) {
    UI_postText("text");
    char const portNames[] = "COM3\0COM5\0";
    UI_postPortList(portNames, sizeof(portNames));
    char const protocolPaths[] = "base/hdlc.json\0";
    UI_postProtocolList(protocolPaths, sizeof(protocolPaths));
    UI_postProtocolLoaded("base/hdlc.json");
    UI_postConnectionStatus(UI_CONNECTION_CONNECTING);
    UI_postConnectionStatus(UI_CONNECTION_CONNECTED);
    UI_postConnectionStatus(UI_CONNECTION_DISCONNECTING);
    UI_postConnectionStatus(UI_CONNECTION_DISCONNECTED);
}
