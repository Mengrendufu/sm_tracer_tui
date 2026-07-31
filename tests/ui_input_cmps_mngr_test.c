#include <setjmp.h>
#include <stddef.h>
#include <string.h>
#include "hsm/sm_input_cmps_mngr.h"
#include "widgets/input_composer.h"

static jmp_buf l_faultJump_;
static char const *l_faultModule_;
static int l_faultLabel_;
static unsigned l_projectAllCount_;
static unsigned l_projectFromCount_;
static unsigned l_moveCursorCount_;
static unsigned l_showCursorCount_;
static unsigned l_hideCursorCount_;
static struct InputComposer *l_projectedComposer_;
static char l_presentedText_[SM_INPUT_CMPS_MNGR_BUFFER_SIZE];
static size_t l_presentedLen_;
static size_t l_presentedEditPos_;
static size_t l_dirtyPos_;

void DBC_fault_handler(char const * const module, int const label) {
    l_faultModule_ = module;
    l_faultLabel_ = label;
    longjmp(l_faultJump_, 1);
}

void InputComposer_init(struct InputComposer * const composer) {
    memset(composer, 0, sizeof(*composer));
}

void InputComposer_create(
    struct InputComposer * const composer,
    struct ncplane * const parent,
    void * const owner,
    int const y,
    unsigned const cols,
    InputComposer_ResizeCb const resizeCb)
{
    (void)composer;
    (void)parent;
    (void)owner;
    (void)y;
    (void)cols;
    (void)resizeCb;
}

void InputComposer_destroy(struct InputComposer * const composer) {
    (void)composer;
}

void InputComposer_resize(struct InputComposer * const composer,
                          int const y,
                          unsigned const cols)
{
    (void)composer;
    (void)y;
    (void)cols;
}

static void recordProjection_(
    struct InputComposer * const composer,
    char const * const text,
    size_t const len,
    size_t const editPos)
{
    l_projectedComposer_ = composer;
    memcpy(l_presentedText_, text, len + 1U);
    l_presentedLen_ = len;
    l_presentedEditPos_ = editPos;
}

void InputComposer_showCursor(
    struct InputComposer * const composer,
    char const * const text,
    size_t const len,
    size_t const editPos)
{
    ++l_showCursorCount_;
    recordProjection_(composer, text, len, editPos);
}

void InputComposer_hideCursor(
    struct InputComposer * const composer,
    char const * const text,
    size_t const len,
    size_t const editPos)
{
    ++l_hideCursorCount_;
    recordProjection_(composer, text, len, editPos);
}

void InputComposer_projectAll(
    struct InputComposer * const composer,
    char const * const text,
    size_t const len,
    size_t const editPos)
{
    ++l_projectAllCount_;
    recordProjection_(composer, text, len, editPos);
}

void InputComposer_projectFrom(
    struct InputComposer * const composer,
    char const * const text,
    size_t const len,
    size_t const editPos,
    size_t const dirtyPos)
{
    ++l_projectFromCount_;
    l_dirtyPos_ = dirtyPos;
    recordProjection_(composer, text, len, editPos);
}

void InputComposer_moveCursor(
    struct InputComposer * const composer,
    char const * const text,
    size_t const len,
    size_t const editPos)
{
    ++l_moveCursorCount_;
    recordProjection_(composer, text, len, editPos);
}

static void resetProjection_(void) {
    l_projectAllCount_ = 0U;
    l_projectFromCount_ = 0U;
    l_moveCursorCount_ = 0U;
    l_showCursorCount_ = 0U;
    l_hideCursorCount_ = 0U;
    l_projectedComposer_ = (struct InputComposer *)0;
    memset(l_presentedText_, 0, sizeof(l_presentedText_));
    l_presentedLen_ = 0U;
    l_presentedEditPos_ = 0U;
    l_dirtyPos_ = 0U;
}

static void dispatchInput_(SM_InputCmpsMngr * const manager,
                           UI_Signal const sig,
                           char const * const utf8)
{
    UI_InputEvt inputEvt = {
        .super.sig = sig,
    };
    if (utf8 != (char const *)0) {
        size_t const len = strlen(utf8);
        if (len >= sizeof(inputEvt.input.utf8)) {
            return;
        }
        memcpy(inputEvt.input.utf8, utf8, len + 1U);
    }
    SM_InputCmpsMngr_dispatchEvt(manager, &inputEvt);
}

static void dispatchAsciiText_(SM_InputCmpsMngr * const manager,
                               char const * const text)
{
    char input[2] = {'\0', '\0'};
    for (size_t i = 0U; text[i] != '\0'; ++i) {
        input[0] = text[i];
        dispatchInput_(manager, UI_INPUT_SIG, input);
    }
}

int main(void) {
    SM_InputCmpsMngr manager;
    int failed = 0;

    SM_InputCmpsMngr_ctor(&manager);
    SM_InputCmpsMngr_init(&manager);

    resetProjection_();
    SM_InputCmpsMngr_setActive(&manager, true);
    failed += l_showCursorCount_ == 1U
              && l_hideCursorCount_ == 0U
              && l_projectedComposer_ == &manager.composer
              && l_presentedLen_ == 0U
              && l_presentedEditPos_ == 0U
              ? 0 : 1;

    resetProjection_();
    SM_InputCmpsMngr_setActive(&manager, false);
    failed += l_showCursorCount_ == 0U
              && l_hideCursorCount_ == 1U
              && l_projectedComposer_ == &manager.composer
              && l_presentedLen_ == 0U
              && l_presentedEditPos_ == 0U
              ? 0 : 1;

    resetProjection_();
    dispatchInput_(&manager, UI_INPUT_SIG, "A");
    failed += l_projectFromCount_ == 1U
              && l_projectAllCount_ == 0U
              && l_moveCursorCount_ == 0U
              && l_projectedComposer_ == &manager.composer
              && strcmp(l_presentedText_, "A") == 0
              && l_dirtyPos_ == 0U
              && strcmp(manager.buffer, "A") == 0
              && manager.length == 1U
              && manager.editPos == 1U
              ? 0 : 1;

    resetProjection_();
    dispatchInput_(&manager, UI_INPUT_SIG, "B");
    failed += l_projectFromCount_ == 1U
              && l_projectAllCount_ == 0U
              && l_moveCursorCount_ == 0U
              && l_dirtyPos_ == 1U
              && strcmp(manager.buffer, "AB") == 0
              && manager.length == 2U
              && manager.editPos == 2U
              ? 0 : 1;

    resetProjection_();
    dispatchInput_(&manager, UI_KEY_LEFT_SIG, (char const *)0);
    failed += l_projectFromCount_ == 0U
              && l_projectAllCount_ == 0U
              && l_moveCursorCount_ == 1U
              && strcmp(l_presentedText_, "AB") == 0
              && l_presentedLen_ == 2U
              && l_presentedEditPos_ == 1U
              && manager.editPos == 1U
              ? 0 : 1;

    resetProjection_();
    dispatchInput_(&manager, UI_INPUT_SIG, "X");
    failed += l_projectFromCount_ == 1U
              && l_projectAllCount_ == 0U
              && l_moveCursorCount_ == 0U
              && l_dirtyPos_ == 1U
              && strcmp(l_presentedText_, "AXB") == 0
              && l_presentedLen_ == 3U
              && l_presentedEditPos_ == 2U
              && strcmp(manager.buffer, "AXB") == 0
              ? 0 : 1;

    resetProjection_();
    dispatchInput_(&manager, UI_KEY_BACKSPACE_SIG,
                   (char const *)0);
    failed += l_projectFromCount_ == 1U
              && l_projectAllCount_ == 0U
              && l_moveCursorCount_ == 0U
              && l_dirtyPos_ == 1U
              && strcmp(l_presentedText_, "AB") == 0
              && l_presentedLen_ == 2U
              && l_presentedEditPos_ == 1U
              && strcmp(manager.buffer, "AB") == 0
              ? 0 : 1;

    resetProjection_();
    dispatchInput_(&manager, UI_KEY_END_SIG, (char const *)0);
    failed += l_projectFromCount_ == 0U
              && l_projectAllCount_ == 0U
              && l_moveCursorCount_ == 1U
              && l_presentedEditPos_ == 2U
              && manager.editPos == 2U
              ? 0 : 1;

    resetProjection_();
    dispatchInput_(&manager, UI_KEY_HOME_SIG, (char const *)0);
    failed += l_projectFromCount_ == 0U
              && l_projectAllCount_ == 0U
              && l_moveCursorCount_ == 1U
              && l_presentedEditPos_ == 0U
              && manager.editPos == 0U
              ? 0 : 1;

    resetProjection_();
    dispatchInput_(&manager, UI_KEY_END_SIG, (char const *)0);
    dispatchInput_(&manager, UI_INPUT_SIG, "C");
    dispatchInput_(&manager, UI_KEY_LEFT_SIG, (char const *)0);
    resetProjection_();
    dispatchInput_(&manager, UI_KEY_CTRL_U_SIG, (char const *)0);
    failed += l_projectFromCount_ == 1U
              && l_projectAllCount_ == 0U
              && l_moveCursorCount_ == 0U
              && l_dirtyPos_ == 0U
              && strcmp(l_presentedText_, "C") == 0
              && l_presentedLen_ == 1U
              && l_presentedEditPos_ == 0U
              && strcmp(manager.buffer, "C") == 0
              && manager.length == 1U
              && manager.editPos == 0U
              ? 0 : 1;

    resetProjection_();
    dispatchInput_(&manager, UI_KEY_CTRL_U_SIG, (char const *)0);
    dispatchInput_(&manager, UI_KEY_HOME_SIG, (char const *)0);
    failed += l_projectFromCount_ == 0U
              && l_projectAllCount_ == 0U
              && l_moveCursorCount_ == 0U
              && strcmp(manager.buffer, "C") == 0
              && manager.editPos == 0U
              ? 0 : 1;

    resetProjection_();
    dispatchInput_(&manager, UI_KEY_END_SIG, (char const *)0);
    failed += l_moveCursorCount_ == 1U
              && l_presentedEditPos_ == 1U
              && manager.editPos == 1U
              ? 0 : 1;

    resetProjection_();
    dispatchInput_(&manager, UI_KEY_END_SIG, (char const *)0);
    failed += l_projectFromCount_ == 0U
              && l_projectAllCount_ == 0U
              && l_moveCursorCount_ == 0U
              && manager.editPos == 1U
              ? 0 : 1;

    dispatchInput_(&manager, UI_KEY_CTRL_U_SIG, (char const *)0);
    dispatchAsciiText_(&manager, "alpha beta   ");
    resetProjection_();
    dispatchInput_(&manager, UI_KEY_CTRL_W_SIG, (char const *)0);
    failed += l_projectFromCount_ == 1U
              && l_projectAllCount_ == 0U
              && l_moveCursorCount_ == 0U
              && l_dirtyPos_ == 6U
              && strcmp(manager.buffer, "alpha ") == 0
              && manager.length == 6U
              && manager.editPos == 6U
              ? 0 : 1;

    dispatchAsciiText_(&manager, "beta");
    dispatchInput_(&manager, UI_KEY_LEFT_SIG, (char const *)0);
    dispatchInput_(&manager, UI_KEY_LEFT_SIG, (char const *)0);
    resetProjection_();
    dispatchInput_(&manager, UI_KEY_CTRL_W_SIG, (char const *)0);
    failed += l_projectFromCount_ == 1U
              && l_dirtyPos_ == 6U
              && strcmp(manager.buffer, "alpha ta") == 0
              && manager.length == 8U
              && manager.editPos == 6U
              ? 0 : 1;

    dispatchInput_(&manager, UI_KEY_HOME_SIG, (char const *)0);
    resetProjection_();
    dispatchInput_(&manager, UI_KEY_CTRL_W_SIG, (char const *)0);
    failed += l_projectFromCount_ == 0U
              && l_projectAllCount_ == 0U
              && l_moveCursorCount_ == 0U
              && strcmp(manager.buffer, "alpha ta") == 0
              && manager.editPos == 0U
              ? 0 : 1;

    dispatchInput_(&manager, UI_KEY_END_SIG, (char const *)0);
    dispatchInput_(&manager, UI_KEY_CTRL_U_SIG, (char const *)0);
    dispatchInput_(&manager, UI_INPUT_SIG, "\xE4\xBD\xA0");
    dispatchInput_(&manager, UI_INPUT_SIG, "\xE5\xA5\xBD");
    dispatchAsciiText_(&manager, " x");
    dispatchInput_(&manager, UI_KEY_CTRL_W_SIG, (char const *)0);
    failed += strcmp(manager.buffer,
                     "\xE4\xBD\xA0\xE5\xA5\xBD ") == 0
              && manager.length == 7U
              && manager.editPos == 7U
              ? 0 : 1;

    resetProjection_();
    dispatchInput_(&manager, UI_KEY_CTRL_W_SIG, (char const *)0);
    failed += l_projectFromCount_ == 1U
              && l_dirtyPos_ == 0U
              && manager.buffer[0] == '\0'
              && manager.length == 0U
              && manager.editPos == 0U
              ? 0 : 1;

    UI_InputEvt const enterEvt = {
        .super.sig = UI_KEY_ENTER_SIG,
    };
    resetProjection_();
    SM_InputCmpsMngr_dispatchEvt(&manager, &enterEvt);
    failed += (l_projectFromCount_ == 0U)
              && (l_projectAllCount_ == 0U)
              && (l_moveCursorCount_ == 0U)
              ? 0 : 1;

    UI_InputEvt const invalidEvt = {
        .super.sig = UI_TIMER_SIG,
    };
    l_faultModule_ = (char const *)0;
    l_faultLabel_ = 0;
    if (setjmp(l_faultJump_) == 0) {
        SM_InputCmpsMngr_dispatchEvt(&manager, &invalidEvt);
        ++failed;
    } else {
        failed += strcmp(l_faultModule_, "sm_input_cmps_mngr") == 0
                  && l_faultLabel_ == 302
                  ? 0 : 1;
    }

    return failed == 0 ? 0 : 1;
}
