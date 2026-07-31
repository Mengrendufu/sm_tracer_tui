#include "hsm/sm_input_cmps_mngr.h"

void UI_InputComposer_contract(SM_InputCmpsMngr * const manager,
                               UI_InputEvt const * const e)
{
    SM_InputCmpsMngr_ctor(manager);
    SM_InputCmpsMngr_init(manager);
    SM_InputCmpsMngr_dispatchEvt(manager, e);
}
