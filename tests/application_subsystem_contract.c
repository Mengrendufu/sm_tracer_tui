#include "sst.h"
#include "sp_mngr/sp_mngr.h"
#include "sp_thread/sp_thread.h"

void ApplicationSubsystem_contract(void) {
    SST_Task * const task = AO_SpMngr;
    (void)task;

    SpMngr_ctor();
    (void)SpThread_start();
    SpThread_postRefreshPorts();
}
