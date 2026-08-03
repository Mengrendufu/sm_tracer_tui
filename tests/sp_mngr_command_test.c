#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app_sig.h"
#include "sp_mngr/sp_mngr.h"

static char l_text_[2048];
static unsigned l_refreshPosts_;
static unsigned l_aoPosts_;
static SST_Signal l_lastAoSig_;
static char l_portNames_[128];
static size_t l_portNamesSize_;

void UI_postText(char const * const text) {
    (void)snprintf(l_text_, sizeof(l_text_), "%s", text);
}

void UI_postPortList(char const * const portNames,
                     size_t const portNamesSize)
{
    l_portNamesSize_ = portNamesSize;
    memcpy(l_portNames_, portNames, portNamesSize);
}

void SpThread_postRefreshPorts(void) {
    ++l_refreshPosts_;
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
    ++l_aoPosts_;
    l_lastAoSig_ = e->sig;
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
        "SpMngr: requesting initial serial port refresh.\n") == 0
        ? 0 : 1;
    failed += (l_aoPosts_ == 1U)
              && (l_lastAoSig_ == SPMNGR_REFRESH_PORTS_SIG)
              ? 0 : 1;

    SpMngrConfigEvt config = {
        .super.sig = SPMNGR_CONFIG_UPDATE_SIG,
        .config = {
            .port = "none",
            .baudrate = "9600",
            .dataBits = "7",
            .stopBits = "2",
            .parity = "even",
            .flowControl = "none",
            .protocol = "hdlc",
        },
    };
    (*AO_SpMngr->dispatch)(AO_SpMngr, &config.super);
    failed += expectContains_("configuration updated");
    failed += expectContains_("port=none");
    failed += expectContains_("baud=9600");
    failed += expectContains_("proto=hdlc");

    config.super.sig = SPMNGR_PORT_CONNECT_SIG;
    (void)snprintf(config.config.port, sizeof(config.config.port),
                   "%s", "com3");
    (*AO_SpMngr->dispatch)(AO_SpMngr, &config.super);
    failed += expectContains_("connect requested");
    failed += expectContains_("port=com3");

    SST_Evt const disconnect = {
        .sig = SPMNGR_PORT_DISCONNECT_SIG,
    };
    (*AO_SpMngr->dispatch)(AO_SpMngr, &disconnect);
    failed += strcmp(l_text_, "SpMngr: disconnect requested.\n") == 0
              ? 0 : 1;

    SST_Evt const refresh = {
        .sig = SPMNGR_REFRESH_PORTS_SIG,
    };
    (*AO_SpMngr->dispatch)(AO_SpMngr, &refresh);
    failed += l_refreshPosts_ == 1U ? 0 : 1;

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
    failed += strcmp(l_text_, "Serial port refresh failed.\n") == 0
              ? 0 : 1;
    failed += l_portNamesSize_ == sizeof(portNames)
              && memcmp(l_portNames_, portNames,
                        sizeof(portNames)) == 0
              ? 0 : 1;

    return failed == 0 ? 0 : 1;
}
