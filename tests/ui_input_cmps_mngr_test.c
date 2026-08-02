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
static unsigned l_suggestionShowCount_;
static unsigned l_suggestionHideCount_;
static size_t l_suggestionCount_;
static size_t l_suggestionSelected_;
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

void CommandSuggestion_init(
    struct CommandSuggestion * const suggestion)
{
    suggestion->plane = (struct ncplane *)0;
}

void CommandSuggestion_create(
    struct CommandSuggestion * const suggestion,
    struct ncplane * const parent)
{
    (void)suggestion;
    (void)parent;
}

void CommandSuggestion_destroy(
    struct CommandSuggestion * const suggestion)
{
    (void)suggestion;
}

void CommandSuggestion_show(
    struct CommandSuggestion * const suggestion,
    int const inputY,
    unsigned const maxCols,
    char const * const candidates[],
    size_t const count,
    size_t const selected)
{
    (void)suggestion;
    (void)inputY;
    (void)maxCols;
    (void)candidates;
    ++l_suggestionShowCount_;
    l_suggestionCount_ = count;
    l_suggestionSelected_ = selected;
}

void CommandSuggestion_hide(
    struct CommandSuggestion * const suggestion)
{
    (void)suggestion;
    ++l_suggestionHideCount_;
}

static void resetProjection_(void) {
    l_projectAllCount_ = 0U;
    l_projectFromCount_ = 0U;
    l_moveCursorCount_ = 0U;
    l_showCursorCount_ = 0U;
    l_hideCursorCount_ = 0U;
    l_suggestionShowCount_ = 0U;
    l_suggestionHideCount_ = 0U;
    l_suggestionCount_ = 0U;
    l_suggestionSelected_ = 0U;
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

    SM_InputCmpsMngr_setActive(&manager, true);

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

    SM_InputCmpsMngr wordMoveManager;
    SM_InputCmpsMngr_ctor(&wordMoveManager);
    SM_InputCmpsMngr_init(&wordMoveManager);
    SM_InputCmpsMngr_setActive(&wordMoveManager, true);
    dispatchAsciiText_(&wordMoveManager, "alpha  beta");

    dispatchInput_(&wordMoveManager, UI_KEY_CTRL_LEFT_SIG,
                   (char const *)0);
    failed += wordMoveManager.editPos == 7U ? 0 : 1;
    dispatchInput_(&wordMoveManager, UI_KEY_CTRL_LEFT_SIG,
                   (char const *)0);
    failed += wordMoveManager.editPos == 0U ? 0 : 1;
    dispatchInput_(&wordMoveManager, UI_KEY_CTRL_RIGHT_SIG,
                   (char const *)0);
    failed += wordMoveManager.editPos == 7U ? 0 : 1;
    dispatchInput_(&wordMoveManager, UI_KEY_CTRL_RIGHT_SIG,
                   (char const *)0);
    failed += wordMoveManager.editPos == 11U ? 0 : 1;

    SM_InputCmpsMngr slashBeforeTextManager;
    SM_InputCmpsMngr_ctor(&slashBeforeTextManager);
    SM_InputCmpsMngr_init(&slashBeforeTextManager);
    SM_InputCmpsMngr_setActive(&slashBeforeTextManager, true);
    dispatchAsciiText_(&slashBeforeTextManager,
                       "/dis /con /ref hi there");
    dispatchInput_(&slashBeforeTextManager, UI_KEY_CTRL_LEFT_SIG,
                   (char const *)0);
    dispatchInput_(&slashBeforeTextManager, UI_KEY_CTRL_LEFT_SIG,
                   (char const *)0);

    l_faultModule_ = (char const *)0;
    l_faultLabel_ = 0;
    if (setjmp(l_faultJump_) == 0) {
        dispatchInput_(&slashBeforeTextManager, UI_INPUT_SIG, "/");
        failed += strcmp(slashBeforeTextManager.buffer,
                         "/dis /con /ref /hi there") == 0
                  && slashBeforeTextManager.editPos == 16U
                  && slashBeforeTextManager.candidateCount == 0U
                  ? 0 : 1;
    } else {
        ++failed;
    }

    SM_InputCmpsMngr whitespaceManager;
    SM_InputCmpsMngr_ctor(&whitespaceManager);
    SM_InputCmpsMngr_init(&whitespaceManager);
    SM_InputCmpsMngr_setActive(&whitespaceManager, true);
    dispatchAsciiText_(&whitespaceManager, "alpha");
    dispatchInput_(&whitespaceManager, UI_INPUT_SIG, "\t");
    resetProjection_();
    dispatchInput_(&whitespaceManager, UI_INPUT_SIG, "/");
    failed += strcmp(whitespaceManager.buffer, "alpha\t/") == 0
              && whitespaceManager.candidateCount == 3U
              && l_suggestionShowCount_ == 1U
              ? 0 : 1;

    SM_InputCmpsMngr tokenManager;
    SM_InputCmpsMngr_ctor(&tokenManager);
    SM_InputCmpsMngr_init(&tokenManager);
    SM_InputCmpsMngr_setActive(&tokenManager, true);

    resetProjection_();
    dispatchInput_(&tokenManager, UI_INPUT_SIG, "/");
    failed += strcmp(tokenManager.buffer, "/") == 0
              && tokenManager.candidateCount == 3U
              && tokenManager.selectedCandidate == 0U
              && l_suggestionShowCount_ == 1U
              && l_suggestionCount_ == 3U
              && l_suggestionSelected_ == 0U
              ? 0 : 1;

    resetProjection_();
    dispatchInput_(&tokenManager, UI_KEY_UP_SIG, (char const *)0);
    failed += tokenManager.selectedCandidate == 2U
              && l_suggestionShowCount_ == 1U
              && l_suggestionSelected_ == 2U
              ? 0 : 1;
    dispatchInput_(&tokenManager, UI_KEY_DOWN_SIG, (char const *)0);
    failed += tokenManager.selectedCandidate == 0U ? 0 : 1;

    resetProjection_();
    dispatchInput_(&tokenManager, UI_KEY_ENTER_SIG, (char const *)0);
    failed += strcmp(tokenManager.buffer, "$connect") == 0
              && tokenManager.length == 8U
              && tokenManager.editPos == 8U
              && tokenManager.tokenCount == 1U
              && tokenManager.tokens[0].begin == 0U
              && tokenManager.tokens[0].end == 8U
              && tokenManager.tokens[0].command
                 == SM_INPUT_COMMAND_CONNECT
              && l_projectAllCount_ == 1U
              && l_suggestionHideCount_ == 1U
              ? 0 : 1;

    dispatchInput_(&tokenManager, UI_INPUT_SIG, " ");
    dispatchInput_(&tokenManager, UI_INPUT_SIG, "/");
    dispatchInput_(&tokenManager, UI_KEY_DOWN_SIG, (char const *)0);
    dispatchInput_(&tokenManager, UI_KEY_TAB_SIG, (char const *)0);
    failed += strcmp(tokenManager.buffer, "$connect $disconnect") == 0
              && tokenManager.length == 20U
              && tokenManager.editPos == 20U
              && tokenManager.tokenCount == 2U
              && tokenManager.tokens[0].begin == 0U
              && tokenManager.tokens[0].end == 8U
              && tokenManager.tokens[1].begin == 9U
              && tokenManager.tokens[1].end == 20U
              && tokenManager.tokens[1].command
                 == SM_INPUT_COMMAND_DISCONNECT
              ? 0 : 1;

    dispatchInput_(&tokenManager, UI_KEY_CTRL_LEFT_SIG,
                   (char const *)0);
    failed += tokenManager.editPos == 9U ? 0 : 1;
    dispatchInput_(&tokenManager, UI_KEY_CTRL_RIGHT_SIG,
                   (char const *)0);
    failed += tokenManager.editPos == 20U ? 0 : 1;

    dispatchInput_(&tokenManager, UI_KEY_LEFT_SIG, (char const *)0);
    failed += tokenManager.editPos == 9U ? 0 : 1;
    dispatchInput_(&tokenManager, UI_KEY_RIGHT_SIG, (char const *)0);
    failed += tokenManager.editPos == 20U ? 0 : 1;

    dispatchInput_(&tokenManager, UI_KEY_BACKSPACE_SIG,
                   (char const *)0);
    failed += strcmp(tokenManager.buffer, "$connect ") == 0
              && tokenManager.length == 9U
              && tokenManager.editPos == 9U
              && tokenManager.tokenCount == 1U
              ? 0 : 1;

    dispatchInput_(&tokenManager, UI_KEY_LEFT_SIG, (char const *)0);
    dispatchInput_(&tokenManager, UI_KEY_BACKSPACE_SIG,
                   (char const *)0);
    failed += strcmp(tokenManager.buffer, " ") == 0
              && tokenManager.length == 1U
              && tokenManager.editPos == 0U
              && tokenManager.tokenCount == 0U
              ? 0 : 1;

    SM_InputCmpsMngr matchManager;
    SM_InputCmpsMngr_ctor(&matchManager);
    SM_InputCmpsMngr_init(&matchManager);
    SM_InputCmpsMngr_setActive(&matchManager, true);

    resetProjection_();
    dispatchAsciiText_(&matchManager, "a/");
    failed += strcmp(matchManager.buffer, "a/") == 0
              && matchManager.candidateCount == 0U
              && l_suggestionShowCount_ == 0U
              ? 0 : 1;

    dispatchInput_(&matchManager, UI_KEY_CTRL_U_SIG,
                   (char const *)0);
    dispatchInput_(&matchManager, UI_INPUT_SIG, "/");
    resetProjection_();
    dispatchInput_(&matchManager, UI_INPUT_SIG, "d");
    failed += strcmp(matchManager.buffer, "/d") == 0
              && matchManager.candidateCount == 1U
              && matchManager.candidates[0]
                 == SM_INPUT_COMMAND_DISCONNECT
              && l_suggestionShowCount_ == 1U
              && l_suggestionCount_ == 1U
              ? 0 : 1;

    resetProjection_();
    dispatchInput_(&matchManager, UI_KEY_BACKSPACE_SIG,
                   (char const *)0);
    failed += strcmp(matchManager.buffer, "/") == 0
              && matchManager.candidateCount == 3U
              && l_suggestionShowCount_ == 1U
              && l_suggestionCount_ == 3U
              ? 0 : 1;

    resetProjection_();
    dispatchInput_(&matchManager, UI_KEY_ESC_SIG, (char const *)0);
    failed += strcmp(matchManager.buffer, "/") == 0
              && matchManager.candidateCount == 0U
              && l_suggestionHideCount_ == 1U
              ? 0 : 1;

    resetProjection_();
    SM_InputCmpsMngr_setActive(&matchManager, false);
    SM_InputCmpsMngr_setActive(&matchManager, true);
    failed += l_hideCursorCount_ == 1U
              && l_showCursorCount_ == 1U
              && l_suggestionShowCount_ == 1U
              && matchManager.candidateCount == 3U
              ? 0 : 1;

    SM_InputCmpsMngr recoveryManager;
    SM_InputCmpsMngr_ctor(&recoveryManager);
    SM_InputCmpsMngr_init(&recoveryManager);
    SM_InputCmpsMngr_setActive(&recoveryManager, true);

    dispatchInput_(&recoveryManager, UI_INPUT_SIG, "/");
    resetProjection_();
    dispatchInput_(&recoveryManager, UI_INPUT_SIG, "x");
    failed += strcmp(recoveryManager.buffer, "/x") == 0
              && recoveryManager.candidateCount == 0U
              && l_suggestionHideCount_ == 1U
              ? 0 : 1;

    resetProjection_();
    dispatchInput_(&recoveryManager, UI_KEY_BACKSPACE_SIG,
                   (char const *)0);
    dispatchInput_(&recoveryManager, UI_INPUT_SIG, "d");
    failed += strcmp(recoveryManager.buffer, "/d") == 0
              && recoveryManager.candidateCount == 1U
              && recoveryManager.candidates[0]
                 == SM_INPUT_COMMAND_DISCONNECT
              && l_suggestionShowCount_ == 2U
              && l_suggestionCount_ == 1U
              ? 0 : 1;

    SM_InputCmpsMngr cursorManager;
    SM_InputCmpsMngr_ctor(&cursorManager);
    SM_InputCmpsMngr_init(&cursorManager);
    SM_InputCmpsMngr_setActive(&cursorManager, true);

    dispatchAsciiText_(&cursorManager, "/ something anotherthing");
    resetProjection_();
    dispatchInput_(&cursorManager, UI_KEY_HOME_SIG,
                   (char const *)0);
    dispatchInput_(&cursorManager, UI_KEY_RIGHT_SIG,
                   (char const *)0);
    failed += cursorManager.editPos == 1U
              && cursorManager.candidateCount == 3U
              && l_suggestionShowCount_ == 1U
              && l_suggestionCount_ == 3U
              ? 0 : 1;

    resetProjection_();
    dispatchInput_(&cursorManager, UI_KEY_RIGHT_SIG,
                   (char const *)0);
    failed += cursorManager.editPos == 2U
              && cursorManager.candidateCount == 0U
              && l_suggestionHideCount_ == 1U
              ? 0 : 1;

    SM_InputCmpsMngr wholePrefixManager;
    SM_InputCmpsMngr_ctor(&wholePrefixManager);
    SM_InputCmpsMngr_init(&wholePrefixManager);
    SM_InputCmpsMngr_setActive(&wholePrefixManager, true);
    dispatchAsciiText_(&wholePrefixManager, "/dis rest");
    dispatchInput_(&wholePrefixManager, UI_KEY_HOME_SIG,
                   (char const *)0);

    resetProjection_();
    dispatchInput_(&wholePrefixManager, UI_KEY_RIGHT_SIG,
                   (char const *)0);
    failed += wholePrefixManager.editPos == 1U
              && wholePrefixManager.candidateCount == 1U
              && wholePrefixManager.candidates[0]
                 == SM_INPUT_COMMAND_DISCONNECT
              && l_suggestionShowCount_ == 1U
              && l_suggestionCount_ == 1U
              ? 0 : 1;

    for (size_t editPos = 2U; editPos <= 4U; ++editPos) {
        resetProjection_();
        dispatchInput_(&wholePrefixManager, UI_KEY_RIGHT_SIG,
                       (char const *)0);
        failed += wholePrefixManager.editPos == editPos
                  && wholePrefixManager.candidateCount == 1U
                  && wholePrefixManager.candidates[0]
                     == SM_INPUT_COMMAND_DISCONNECT
                  && l_suggestionShowCount_ == 1U
                  && l_suggestionCount_ == 1U
                  ? 0 : 1;
    }

    SM_InputCmpsMngr spanManager;
    SM_InputCmpsMngr_ctor(&spanManager);
    SM_InputCmpsMngr_init(&spanManager);
    SM_InputCmpsMngr_setActive(&spanManager, true);
    dispatchAsciiText_(&spanManager, "/connect rest");
    dispatchInput_(&spanManager, UI_KEY_HOME_SIG,
                   (char const *)0);
    dispatchInput_(&spanManager, UI_KEY_RIGHT_SIG,
                   (char const *)0);
    dispatchInput_(&spanManager, UI_KEY_RIGHT_SIG,
                   (char const *)0);
    dispatchInput_(&spanManager, UI_KEY_RIGHT_SIG,
                   (char const *)0);
    dispatchInput_(&spanManager, UI_KEY_RIGHT_SIG,
                   (char const *)0);

    resetProjection_();
    dispatchInput_(&spanManager, UI_KEY_ENTER_SIG,
                   (char const *)0);
    failed += strcmp(spanManager.buffer, "$connect rest") == 0
              && spanManager.editPos == 8U
              && spanManager.tokenCount == 1U
              && spanManager.tokens[0].begin == 0U
              && spanManager.tokens[0].end == 8U
              && l_projectAllCount_ == 1U
              && l_suggestionHideCount_ == 1U
              ? 0 : 1;

    SM_InputCmpsMngr fullManager;
    SM_InputCmpsMngr_ctor(&fullManager);
    SM_InputCmpsMngr_init(&fullManager);
    SM_InputCmpsMngr_setActive(&fullManager, true);
    memset(fullManager.buffer, 'a', 247U);
    fullManager.buffer[247] = ' ';
    fullManager.buffer[248] = '\0';
    fullManager.length = 248U;
    fullManager.editPos = 248U;
    dispatchInput_(&fullManager, UI_INPUT_SIG, "/");

    resetProjection_();
    dispatchInput_(&fullManager, UI_KEY_ENTER_SIG, (char const *)0);
    failed += fullManager.length == 249U
              && fullManager.editPos == 249U
              && fullManager.buffer[248] == '/'
              && fullManager.candidateCount == 3U
              && fullManager.tokenCount == 0U
              && l_projectAllCount_ == 0U
              && l_suggestionHideCount_ == 0U
              ? 0 : 1;

    resetProjection_();
    SM_InputCmpsMngr_setActive(&fullManager, false);
    failed += fullManager.candidateCount == 0U
              && l_suggestionHideCount_ == 1U
              && l_hideCursorCount_ == 1U
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
