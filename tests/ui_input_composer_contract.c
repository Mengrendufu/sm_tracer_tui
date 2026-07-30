#include "hsm/sm_ui_key.h"
#include "widgets/input_composer.h"

void UI_InputComposer_contract(struct InputComposer * const composer,
                               SM_UI_Key * const keyHsm,
                               UI_Evt const * const e)
{
    InputComposer_init(composer);
    SM_UI_Key_ctor(keyHsm, composer);
    SM_UI_Key_init(keyHsm);
    SM_UI_Key_dispatchEvt(keyHsm, e);
}
