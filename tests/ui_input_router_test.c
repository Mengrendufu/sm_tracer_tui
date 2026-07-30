#include <notcurses/notcurses.h>
#include "thread/ui_input_router_priv.h"

static int expectRoute_(uint32_t const id,
                        uint32_t const modifiers,
                        UI_Signal const expected)
{
    UI_Input const input = {
        .id = id,
        .modifiers = modifiers,
    };
    return UI_InputRouter_route(&input) == expected ? 0 : 1;
}

int main(void) {
    int failed = 0;

    failed += expectRoute_('x', 0U, UI_INPUT_SIG);
    failed += expectRoute_(27U, 0U, UI_KEY_ESC_SIG);
    failed += expectRoute_(0x1FU, 0U, UI_KEY_CTRL_SLASH_SIG);
    failed += expectRoute_('/', NCKEY_MOD_CTRL, UI_KEY_CTRL_SLASH_SIG);
    failed += expectRoute_(NCKEY_UP, 0U, UI_KEY_UP_SIG);
    failed += expectRoute_(NCKEY_DOWN, 0U, UI_KEY_DOWN_SIG);
    failed += expectRoute_(NCKEY_ENTER, 0U, UI_KEY_ENTER_SIG);
    failed += expectRoute_('j', 0U, UI_KEY_J_SIG);
    failed += expectRoute_('k', 0U, UI_KEY_K_SIG);
    failed += expectRoute_(0x0EU, 0U, UI_KEY_CTRL_N_SIG);
    failed += expectRoute_('n', NCKEY_MOD_CTRL, UI_KEY_CTRL_N_SIG);
    failed += expectRoute_(0x10U, 0U, UI_KEY_CTRL_P_SIG);
    failed += expectRoute_('p', NCKEY_MOD_CTRL, UI_KEY_CTRL_P_SIG);
    failed += expectRoute_(NCKEY_PGUP, 0U, UI_KEY_PGUP_SIG);
    failed += expectRoute_(NCKEY_PGDOWN, 0U, UI_KEY_PGDN_SIG);
    failed += expectRoute_(NCKEY_RESIZE, 0U, UI_RESIZE_SIG);

    return failed == 0 ? 0 : 1;
}
