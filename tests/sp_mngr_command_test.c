#include <stdio.h>
#include <string.h>
#include "app_sig.h"
#include "sp_mngr/sp_mngr.h"

static char l_text_[2048];

void UI_postText(char const * const text) {
    (void)snprintf(l_text_, sizeof(l_text_), "%s", text);
}

void SST_Task_ctor(SST_Task * const me,
                   SST_Handler const init,
                   SST_Handler const dispatch)
{
    me->init = init;
    me->dispatch = dispatch;
}

static int expectContains_(char const * const needle) {
    return strstr(l_text_, needle) != (char const *)0 ? 0 : 1;
}

int main(void) {
    int failed = 0;
    SpMngr_ctor();
    (*AO_SpMngr->init)(AO_SpMngr, (SST_Evt const *)0);

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
    failed += strcmp(l_text_, "SpMngr: port refresh requested.\n") == 0
              ? 0 : 1;

    return failed == 0 ? 0 : 1;
}
