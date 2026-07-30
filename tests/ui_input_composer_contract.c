#include "hsm/sm_input_composer_manager.h"
#include "widgets/input_composer.h"

void UI_InputComposer_contract(struct InputComposer * const composer,
                               SM_InputComposerManager * const manager,
                               UI_Evt const * const e)
{
    InputComposer_init(composer);
    SM_InputComposerManager_ctor(manager, composer);
    SM_InputComposerManager_init(manager);
    SM_InputComposerManager_dispatchEvt(manager, e);
}
