#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "app_sig.h"
#include "sp_mngr/sp_mngr.h"
#include "sp_thread/sp_thread.h"
#include "ui_evt.h"

static char l_text_[2048];
static unsigned l_textPosts_;
static unsigned l_refreshPosts_;
static unsigned l_aoPosts_;
static SST_Signal l_aoSignals_[4];
static unsigned l_openPosts_;
static unsigned l_applyConfigPosts_;
static unsigned l_closePosts_;
static SerialConfig l_openConfig_;
static SerialConfig l_appliedConfig_;
static UI_ConnectionStatus l_connectionStatus_;
static char l_portNames_[128];
static size_t l_portNamesSize_;
static char l_protocolPaths_[512];
static size_t l_protocolPathsSize_;
static char l_loadedProtocol_[SPMNGR_VALUE_LEN];
static unsigned l_protocolLoadedPosts_;

void UI_postText(char const * const text) {
    ++l_textPosts_;
    (void)snprintf(l_text_, sizeof(l_text_), "%s", text);
}

void UI_postPortList(char const * const portNames,
                     size_t const portNamesSize)
{
    l_portNamesSize_ = portNamesSize;
    memcpy(l_portNames_, portNames, portNamesSize);
}

void UI_postProtocolList(char const * const protocolPaths,
                         size_t const protocolPathsSize)
{
    l_protocolPathsSize_ = protocolPathsSize;
    memcpy(l_protocolPaths_, protocolPaths, protocolPathsSize);
}

void UI_postProtocolLoaded(char const * const relativePath) {
    ++l_protocolLoadedPosts_;
    (void)snprintf(l_loadedProtocol_, sizeof(l_loadedProtocol_),
                   "%s", relativePath);
}

void UI_postConnectionStatus(UI_ConnectionStatus const status) {
    l_connectionStatus_ = status;
}

void SpThread_postRefreshPorts(void) {
    ++l_refreshPosts_;
}

void SpThread_postOpenPort(SerialConfig const * const config) {
    ++l_openPosts_;
    l_openConfig_ = *config;
}

void SpThread_postApplyConfig(SerialConfig const * const config) {
    ++l_applyConfigPosts_;
    l_appliedConfig_ = *config;
}

void SpThread_postClosePort(void) {
    ++l_closePosts_;
}

void SST_Task_ctor(SST_Task * const me,
                   SST_Handler const init,
                   SST_Handler const dispatch)
{
    me->init = init;
    me->dispatch = dispatch;
}

void SST_Task_post(SST_Task * const me, SST_Evt const * const e) {
    (void)me;
    if (l_aoPosts_ < (sizeof(l_aoSignals_) / sizeof(l_aoSignals_[0]))) {
        l_aoSignals_[l_aoPosts_] = e->sig;
    }
    ++l_aoPosts_;
}

static int expectContains_(char const * const needle) {
    return strstr(l_text_, needle) != (char const *)0 ? 0 : 1;
}

int main(void) {
    int failed = 0;
    SpMngr_ctor();
    (*AO_SpMngr->init)(AO_SpMngr, (SST_Evt const *)0);
    failed += strcmp(
        l_text_,
        "[SYS_INFO]> Requesting initial serial port refresh.\n") == 0
        ? 0 : 1;
    failed += (l_aoPosts_ == 2U)
              && (l_aoSignals_[0] == SPMNGR_REFRESH_PROTOCOLS_SIG)
              && (l_aoSignals_[1] == SPMNGR_REFRESH_PORTS_SIG)
              ? 0 : 1;
    failed += l_textPosts_ == 2U ? 0 : 1;

    SST_Evt const refreshProtocols = {
        .sig = SPMNGR_REFRESH_PROTOCOLS_SIG,
    };
    (*AO_SpMngr->dispatch)(AO_SpMngr, &refreshProtocols);
    failed += l_textPosts_ == 2U ? 0 : 1;
    failed += l_protocolPathsSize_ > 2U
              && strcmp(l_protocolPaths_, "blinky_c51.json") == 0
              ? 0 : 1;

    SpMngrProtocolEvt loadProtocol = {
        .super.sig = SPMNGR_LOAD_PROTOCOL_SIG,
        .relativePath = "blinky_c51.json",
    };
    (*AO_SpMngr->dispatch)(AO_SpMngr, &loadProtocol.super);
    failed += l_protocolLoadedPosts_ == 1U
              && strcmp(l_loadedProtocol_, "blinky_c51.json") == 0
              ? 0 : 1;
    failed += strcmp(
        l_text_,
        "[SYS_INFO]> Protocol loaded: blinky_c51.json\n") == 0
        ? 0 : 1;

    (void)snprintf(loadProtocol.relativePath,
                   sizeof(loadProtocol.relativePath),
                   "%s", "missing.json");
    (*AO_SpMngr->dispatch)(AO_SpMngr, &loadProtocol.super);
    failed += l_protocolLoadedPosts_ == 1U ? 0 : 1;
    failed += expectContains_("[SYS_INFO]> Protocol load failed:");

    SpMngrConfigEvt config = {
        .super.sig = SPMNGR_CONFIG_UPDATE_SIG,
        .config = {
            .port = "none",
            .baudrate = "9600",
            .dataBits = "7",
            .stopBits = "2",
            .parity = "even",
            .flowControl = "none",
        },
    };
    (*AO_SpMngr->dispatch)(AO_SpMngr, &config.super);
    failed += expectContains_("[SYS_INFO]> Configuration updated:");
    failed += expectContains_("port=none");
    failed += expectContains_("baud=9600");
    failed += l_applyConfigPosts_ == 0U ? 0 : 1;

    config.super.sig = SPMNGR_PORT_CONNECT_SIG;
    (void)snprintf(config.config.port, sizeof(config.config.port),
                   "%s", "com3");
    (*AO_SpMngr->dispatch)(AO_SpMngr, &config.super);
    failed += expectContains_("[SYS_INFO]> Connect requested:");
    failed += expectContains_("port=com3");
    failed += l_openPosts_ == 1U ? 0 : 1;
    failed += strcmp(l_openConfig_.portName, "com3") == 0 ? 0 : 1;
    failed += l_openConfig_.baudRate == 9600 ? 0 : 1;
    failed += l_openConfig_.dataBits == 7U ? 0 : 1;
    failed += l_openConfig_.stopBits == SERIAL_STOP_BITS_2 ? 0 : 1;
    failed += l_openConfig_.parity == SERIAL_PARITY_EVEN ? 0 : 1;
    failed += l_openConfig_.flowControl == SERIAL_FLOW_NONE ? 0 : 1;
    failed += l_connectionStatus_ == UI_CONNECTION_CONNECTING ? 0 : 1;

    config.super.sig = SPMNGR_CONFIG_UPDATE_SIG;
    (*AO_SpMngr->dispatch)(AO_SpMngr, &config.super);
    failed += l_applyConfigPosts_ == 1U ? 0 : 1;
    failed += strcmp(l_appliedConfig_.portName, "com3") == 0 ? 0 : 1;
    failed += l_appliedConfig_.baudRate == 9600 ? 0 : 1;
    failed += l_appliedConfig_.dataBits == 7U ? 0 : 1;
    failed += l_appliedConfig_.stopBits == SERIAL_STOP_BITS_2 ? 0 : 1;
    failed += l_appliedConfig_.parity == SERIAL_PARITY_EVEN ? 0 : 1;
    failed += l_appliedConfig_.flowControl == SERIAL_FLOW_NONE ? 0 : 1;

    SST_Evt const configApplied = {
        .sig = SPMNGR_CONFIG_APPLIED_SIG,
    };
    (*AO_SpMngr->dispatch)(AO_SpMngr, &configApplied);
    failed += strcmp(
        l_text_, "[SYS_INFO]> Serial configuration applied.\n") == 0
        ? 0 : 1;

    SST_Evt const configApplyFailed = {
        .sig = SPMNGR_CONFIG_APPLY_FAILED_SIG,
    };
    (*AO_SpMngr->dispatch)(AO_SpMngr, &configApplyFailed);
    failed += strcmp(
        l_text_,
        "[SYS_INFO]> Serial configuration failed; port closed.\n") == 0
        ? 0 : 1;
    failed += l_connectionStatus_ == UI_CONNECTION_DISCONNECTED ? 0 : 1;

    config.super.sig = SPMNGR_PORT_CONNECT_SIG;
    (void)snprintf(config.config.baudrate,
                   sizeof(config.config.baudrate), "%s", "invalid");
    (*AO_SpMngr->dispatch)(AO_SpMngr, &config.super);
    failed += l_openPosts_ == 1U ? 0 : 1;
    failed += strcmp(
        l_text_, "[SYS_INFO]> Invalid serial configuration.\n") == 0
        ? 0 : 1;

    SST_Evt const opened = {
        .sig = SPMNGR_PORT_OPENED_SIG,
    };
    (*AO_SpMngr->dispatch)(AO_SpMngr, &opened);
    failed += strcmp(l_text_, "[SYS_INFO]> Serial port opened.\n") == 0
              ? 0 : 1;
    failed += l_connectionStatus_ == UI_CONNECTION_CONNECTED ? 0 : 1;

    SST_Evt const alreadyConnected = {
        .sig = SPMNGR_PORT_ALREADY_CONNECTED_SIG,
    };
    unsigned const textPostsBeforeAlreadyConnected = l_textPosts_;
    (void)snprintf(l_text_, sizeof(l_text_), "%s", "unchanged");
    l_connectionStatus_ = UI_CONNECTION_CONNECTING;
    (*AO_SpMngr->dispatch)(AO_SpMngr, &alreadyConnected);
    failed += strcmp(l_text_, "unchanged") == 0 ? 0 : 1;
    failed += l_textPosts_ == textPostsBeforeAlreadyConnected ? 0 : 1;
    failed += l_connectionStatus_ == UI_CONNECTION_CONNECTED ? 0 : 1;

    SST_Evt const openFailed = {
        .sig = SPMNGR_PORT_OPEN_FAILED_SIG,
    };
    (*AO_SpMngr->dispatch)(AO_SpMngr, &openFailed);
    failed += strcmp(
        l_text_, "[SYS_INFO]> Serial port open failed.\n") == 0
        ? 0 : 1;
    failed += l_connectionStatus_ == UI_CONNECTION_DISCONNECTED ? 0 : 1;

    SST_Evt const disconnect = {
        .sig = SPMNGR_PORT_DISCONNECT_SIG,
    };
    (*AO_SpMngr->dispatch)(AO_SpMngr, &disconnect);
    failed += strcmp(l_text_, "[SYS_INFO]> Disconnect requested.\n") == 0
              ? 0 : 1;
    failed += l_closePosts_ == 1U ? 0 : 1;
    failed += l_connectionStatus_ == UI_CONNECTION_DISCONNECTING ? 0 : 1;

    SST_Evt const closed = {
        .sig = SPMNGR_PORT_CLOSED_SIG,
    };
    (*AO_SpMngr->dispatch)(AO_SpMngr, &closed);
    failed += strcmp(l_text_, "[SYS_INFO]> Serial port closed.\n") == 0
              ? 0 : 1;
    failed += l_connectionStatus_ == UI_CONNECTION_DISCONNECTED ? 0 : 1;

    (*AO_SpMngr->dispatch)(AO_SpMngr, &opened);
    SST_Evt const closeFailed = {
        .sig = SPMNGR_PORT_CLOSE_FAILED_SIG,
    };
    (*AO_SpMngr->dispatch)(AO_SpMngr, &closeFailed);
    failed += strcmp(
        l_text_, "[SYS_INFO]> Serial port close failed.\n") == 0
        ? 0 : 1;
    failed += l_connectionStatus_ == UI_CONNECTION_DISCONNECTED ? 0 : 1;

    (*AO_SpMngr->dispatch)(AO_SpMngr, &opened);
    SST_Evt const connectionLost = {
        .sig = SPMNGR_PORT_CONNECTION_LOST_SIG,
    };
    (*AO_SpMngr->dispatch)(AO_SpMngr, &connectionLost);
    failed += strcmp(l_text_,
                     "[SYS_INFO]> Serial port connection lost.\n") == 0
              ? 0 : 1;
    failed += l_connectionStatus_ == UI_CONNECTION_DISCONNECTED ? 0 : 1;
    failed += l_refreshPosts_ == 1U ? 0 : 1;

    SST_Evt const refresh = {
        .sig = SPMNGR_REFRESH_PORTS_SIG,
    };
    (*AO_SpMngr->dispatch)(AO_SpMngr, &refresh);
    failed += l_refreshPosts_ == 2U ? 0 : 1;

    char const portNames[] = "/dev/ttyTEST0\0";
    SpMngrPortsEvt ports = {
        .super.sig = SPMNGR_REFRESHED_PORTS_SIG,
        .portNames = (char *)malloc(sizeof(portNames)),
        .portNamesSize = sizeof(portNames),
    };
    if (ports.portNames == (char *)0) {
        return 1;
    }
    memcpy(ports.portNames, portNames, sizeof(portNames));
    (*AO_SpMngr->dispatch)(AO_SpMngr, &ports.super);
    failed += l_portNamesSize_ == sizeof(portNames)
              && memcmp(l_portNames_, portNames,
                        sizeof(portNames)) == 0
              ? 0 : 1;

    ports.portNames = (char *)0;
    ports.portNamesSize = 0U;
    (*AO_SpMngr->dispatch)(AO_SpMngr, &ports.super);
    failed += strcmp(l_text_,
                     "[SYS_INFO]> Serial port refresh failed.\n") == 0
              ? 0 : 1;
    failed += l_portNamesSize_ == sizeof(portNames)
              && memcmp(l_portNames_, portNames,
                        sizeof(portNames)) == 0
              ? 0 : 1;

    uint8_t const framePart1[] = {0x7EU, 0x07U};
    SpMngrRxPacketEvt packet = {
        .super.sig = SPMNGR_RX_PACKET_SIG,
        .data = (uint8_t *)malloc(sizeof(framePart1)),
        .size = sizeof(framePart1),
    };
    if (packet.data == (uint8_t *)0) {
        return 1;
    }
    memcpy(packet.data, framePart1, sizeof(framePart1));
    (void)snprintf(l_text_, sizeof(l_text_), "%s", "unchanged");
    (*AO_SpMngr->dispatch)(AO_SpMngr, &packet.super);
    failed += strcmp(l_text_, "unchanged") == 0 ? 0 : 1;

    uint8_t const framePart2[] = {
        0x0BU, 0x00U, 0xEDU,
    };
    packet.data = (uint8_t *)malloc(sizeof(framePart2));
    packet.size = sizeof(framePart2);
    if (packet.data == (uint8_t *)0) {
        return 1;
    }
    memcpy(packet.data, framePart2, sizeof(framePart2));
    (*AO_SpMngr->dispatch)(AO_SpMngr, &packet.super);
    failed += strcmp(l_text_, "[007]==ledOn ==\n") == 0
              ? 0 : 1;

    return failed == 0 ? 0 : 1;
}
