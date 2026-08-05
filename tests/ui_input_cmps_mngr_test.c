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
static unsigned l_limitReachedCount_;
static bool l_limitReached_;
static unsigned l_highlightTokenCount_;
static size_t l_highlightBegin_;
static size_t l_highlightEnd_;
static unsigned l_suggestionShowCount_;
static unsigned l_suggestionHideCount_;
static size_t l_suggestionCount_;
static size_t l_suggestionSelected_;
static struct InputComposer *l_projectedComposer_;
static char l_presentedText_[SM_INPUT_CMPS_MNGR_BUFFER_SIZE];
static size_t l_presentedLen_;
static size_t l_presentedEditPos_;
static size_t l_dirtyPos_;
static unsigned l_submissionCount_;
static unsigned l_rejectionCount_;
static SM_InputCmpsMngrSubmission l_submission_;
static char const *l_rejectionReason_;
static char l_submissionPort_[SM_INPUT_CMPS_MNGR_BUFFER_SIZE];
static char l_submissionBaudrate_[SM_INPUT_CMPS_MNGR_BUFFER_SIZE];
static char l_submissionDataBits_[SM_INPUT_CMPS_MNGR_BUFFER_SIZE];
static char l_submissionStopBits_[SM_INPUT_CMPS_MNGR_BUFFER_SIZE];
static char l_submissionParity_[SM_INPUT_CMPS_MNGR_BUFFER_SIZE];
static char l_submissionFlowControl_[SM_INPUT_CMPS_MNGR_BUFFER_SIZE];
static char l_submissionProtocol_[SM_INPUT_CMPS_MNGR_BUFFER_SIZE];

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

void InputComposer_setLimitReached(
    struct InputComposer * const composer,
    bool const reached)
{
    (void)composer;
    ++l_limitReachedCount_;
    l_limitReached_ = reached;
}

void InputComposer_highlightToken(
    struct InputComposer * const composer,
    char const * const text,
    size_t const len,
    size_t const editPos,
    size_t const begin,
    size_t const end)
{
    (void)composer;
    (void)text;
    (void)len;
    (void)editPos;
    ++l_highlightTokenCount_;
    l_highlightBegin_ = begin;
    l_highlightEnd_ = end;
}

void InputComposer_resize(struct InputComposer * const composer,
                          int const y,
                          unsigned const rows,
                          unsigned const cols)
{
    (void)composer;
    (void)y;
    (void)rows;
    (void)cols;
}

unsigned InputComposer_preferredRows(
    struct InputComposer const * const composer,
    char const * const text,
    size_t const len,
    size_t const editPos,
    unsigned const cols)
{
    (void)composer;
    (void)text;
    (void)len;
    (void)editPos;
    (void)cols;
    return 1U;
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
    l_limitReachedCount_ = 0U;
    l_limitReached_ = false;
    l_highlightTokenCount_ = 0U;
    l_highlightBegin_ = 0U;
    l_highlightEnd_ = 0U;
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

static void copyArg_(SM_InputCmpsMngrArg const * const arg,
                     char * const dst,
                     size_t const dstSize)
{
    dst[0] = '\0';
    if (arg->present && (arg->len < dstSize)) {
        memcpy(dst, arg->text, arg->len);
        dst[arg->len] = '\0';
    }
}

static void recordSubmission_(
    void * const ctx,
    SM_InputCmpsMngrSubmission const * const submission)
{
    (void)ctx;
    ++l_submissionCount_;
    l_submission_ = *submission;
    copyArg_(&submission->port, l_submissionPort_,
             sizeof(l_submissionPort_));
    copyArg_(&submission->baudrate, l_submissionBaudrate_,
             sizeof(l_submissionBaudrate_));
    copyArg_(&submission->dataBits, l_submissionDataBits_,
             sizeof(l_submissionDataBits_));
    copyArg_(&submission->stopBits, l_submissionStopBits_,
             sizeof(l_submissionStopBits_));
    copyArg_(&submission->parity, l_submissionParity_,
             sizeof(l_submissionParity_));
    copyArg_(&submission->flowControl, l_submissionFlowControl_,
             sizeof(l_submissionFlowControl_));
    copyArg_(&submission->protocolPath, l_submissionProtocol_,
             sizeof(l_submissionProtocol_));
}

static void recordRejection_(void * const ctx,
                             char const * const reason)
{
    (void)ctx;
    ++l_rejectionCount_;
    l_rejectionReason_ = reason;
}

static void resetSubmission_(void) {
    l_submissionCount_ = 0U;
    l_rejectionCount_ = 0U;
    l_rejectionReason_ = (char const *)0;
    memset(&l_submission_, 0, sizeof(l_submission_));
    l_submissionPort_[0] = '\0';
    l_submissionBaudrate_[0] = '\0';
    l_submissionDataBits_[0] = '\0';
    l_submissionStopBits_[0] = '\0';
    l_submissionParity_[0] = '\0';
    l_submissionFlowControl_[0] = '\0';
    l_submissionProtocol_[0] = '\0';
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

static void acceptCommand_(SM_InputCmpsMngr * const manager,
                           char const * const command)
{
    dispatchInput_(manager, UI_INPUT_SIG, "/");
    dispatchAsciiText_(manager, command);
    dispatchInput_(manager, UI_KEY_ENTER_SIG, (char const *)0);
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
              && whitespaceManager.candidateCount == SM_INPUT_COMMAND_NUM
              && l_suggestionShowCount_ == 1U
              ? 0 : 1;

    SM_InputCmpsMngr tokenManager;
    SM_InputCmpsMngr_ctor(&tokenManager);
    SM_InputCmpsMngr_init(&tokenManager);
    SM_InputCmpsMngr_setActive(&tokenManager, true);

    resetProjection_();
    dispatchInput_(&tokenManager, UI_INPUT_SIG, "/");
    failed += strcmp(tokenManager.buffer, "/") == 0
              && tokenManager.candidateCount == SM_INPUT_COMMAND_NUM
              && tokenManager.selectedCandidate == 0U
              && l_suggestionShowCount_ == 1U
              && l_suggestionCount_ == SM_INPUT_COMMAND_NUM
              && l_suggestionSelected_ == 0U
              ? 0 : 1;

    resetProjection_();
    dispatchInput_(&tokenManager, UI_KEY_UP_SIG, (char const *)0);
    failed += tokenManager.selectedCandidate
                  == (SM_INPUT_COMMAND_NUM - 1U)
              && l_suggestionShowCount_ == 1U
              && l_suggestionSelected_
                  == (SM_INPUT_COMMAND_NUM - 1U)
              ? 0 : 1;
    dispatchInput_(&tokenManager, UI_KEY_DOWN_SIG, (char const *)0);
    failed += tokenManager.selectedCandidate == 0U ? 0 : 1;

    resetProjection_();
    dispatchInput_(&tokenManager, UI_INPUT_SIG, "c");
    dispatchInput_(&tokenManager, UI_KEY_ENTER_SIG, (char const *)0);
    failed += strcmp(tokenManager.buffer, "$connect ") == 0
              && tokenManager.length == 9U
              && tokenManager.editPos == 9U
              && tokenManager.tokenCount == 1U
              && tokenManager.tokens[0].begin == 0U
              && tokenManager.tokens[0].end == 8U
              && tokenManager.tokens[0].command
                 == SM_INPUT_COMMAND_CONNECT
              && l_projectAllCount_ == 1U
              && l_highlightTokenCount_ == 1U
              && l_highlightBegin_ == 0U
              && l_highlightEnd_ == 8U
              && l_suggestionHideCount_ == 1U
              ? 0 : 1;

    dispatchInput_(&tokenManager, UI_INPUT_SIG, "/");
    dispatchAsciiText_(&tokenManager, "dis");
    dispatchInput_(&tokenManager, UI_KEY_TAB_SIG, (char const *)0);
    failed += strcmp(tokenManager.buffer, "$connect $disconnect ") == 0
              && tokenManager.length == 21U
              && tokenManager.editPos == 21U
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
    failed += tokenManager.editPos == 21U ? 0 : 1;

    dispatchInput_(&tokenManager, UI_KEY_LEFT_SIG, (char const *)0);
    failed += tokenManager.editPos == 20U ? 0 : 1;
    dispatchInput_(&tokenManager, UI_KEY_RIGHT_SIG, (char const *)0);
    failed += tokenManager.editPos == 21U ? 0 : 1;

    dispatchInput_(&tokenManager, UI_KEY_BACKSPACE_SIG,
                   (char const *)0);
    failed += strcmp(tokenManager.buffer, "$connect $disconnect") == 0
              && tokenManager.length == 20U
              && tokenManager.editPos == 20U
              && tokenManager.tokenCount == 2U
              ? 0 : 1;

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

    SM_InputCmpsMngr pastedTokenManager;
    SM_InputCmpsMngr_ctor(&pastedTokenManager);
    SM_InputCmpsMngr_init(&pastedTokenManager);
    SM_InputCmpsMngr_setActive(&pastedTokenManager, true);
    resetProjection_();
    dispatchAsciiText_(&pastedTokenManager, "$connect");
    failed += strcmp(pastedTokenManager.buffer, "$connect") == 0
              && pastedTokenManager.tokenCount == 0U
              && l_highlightTokenCount_ == 0U
              ? 0 : 1;

    SM_InputCmpsMngr portArgManager;
    SM_InputCmpsMngr_ctor(&portArgManager);
    SM_InputCmpsMngr_init(&portArgManager);
    SM_InputCmpsMngr_setActive(&portArgManager, true);
    char const portCatalog[] = "COM3\0COM5\0";
    SM_InputCmpsMngr_setPortCatalog(
        &portArgManager, portCatalog, sizeof(portCatalog));

    resetProjection_();
    acceptCommand_(&portArgManager, "connect");
    failed += strcmp(portArgManager.buffer, "$connect ") == 0
              && portArgManager.candidateCount == 2U
              ? 0 : 1;

    dispatchAsciiText_(&portArgManager, "COM3");
    failed += portArgManager.candidateCount == 1U ? 0 : 1;
    dispatchInput_(&portArgManager, UI_KEY_TAB_SIG,
                   (char const *)0);
    failed += strcmp(portArgManager.buffer, "$connect COM3 ") == 0
              && portArgManager.tokenCount == 1U
              ? 0 : 1;

    dispatchInput_(&portArgManager, UI_KEY_LEFT_SIG,
                   (char const *)0);
    failed += portArgManager.candidateCount == 1U ? 0 : 1;
    dispatchInput_(&portArgManager, UI_KEY_LEFT_SIG,
                   (char const *)0);
    failed += portArgManager.candidateCount == 1U ? 0 : 1;
    dispatchInput_(&portArgManager, UI_KEY_RIGHT_SIG,
                   (char const *)0);
    dispatchInput_(&portArgManager, UI_KEY_BACKSPACE_SIG,
                   (char const *)0);
    failed += strcmp(portArgManager.buffer, "$connect COM ") == 0
              && portArgManager.candidateCount == 2U
              ? 0 : 1;
    dispatchInput_(&portArgManager, UI_INPUT_SIG, "3");
    dispatchInput_(&portArgManager, UI_KEY_TAB_SIG,
                   (char const *)0);
    failed += strcmp(portArgManager.buffer, "$connect COM3 ") == 0
              ? 0 : 1;

    SM_InputCmpsMngr fixedArgManager;
    SM_InputCmpsMngr_ctor(&fixedArgManager);
    SM_InputCmpsMngr_init(&fixedArgManager);
    SM_InputCmpsMngr_setActive(&fixedArgManager, true);

    resetProjection_();
    acceptCommand_(&fixedArgManager, "dataBits");
    failed += fixedArgManager.candidateCount == 4U
              ? 0 : 1;
    dispatchInput_(&fixedArgManager, UI_KEY_TAB_SIG,
                   (char const *)0);
    failed += strcmp(fixedArgManager.buffer, "$dataBits 5 ") == 0
              ? 0 : 1;

    acceptCommand_(&fixedArgManager, "stopBits");
    failed += fixedArgManager.candidateCount == 3U ? 0 : 1;
    dispatchInput_(&fixedArgManager, UI_KEY_ESC_SIG,
                   (char const *)0);
    acceptCommand_(&fixedArgManager, "parity");
    failed += fixedArgManager.candidateCount == 5U ? 0 : 1;
    dispatchInput_(&fixedArgManager, UI_KEY_ESC_SIG,
                   (char const *)0);
    acceptCommand_(&fixedArgManager, "flowControl");
    failed += fixedArgManager.candidateCount == 4U ? 0 : 1;

    SM_InputCmpsMngr updatedPortManager;
    SM_InputCmpsMngr_ctor(&updatedPortManager);
    SM_InputCmpsMngr_init(&updatedPortManager);
    SM_InputCmpsMngr_setActive(&updatedPortManager, true);
    SM_InputCmpsMngr_setPortCatalog(
        &updatedPortManager, portCatalog, sizeof(portCatalog));
    acceptCommand_(&updatedPortManager, "connect");
    char const updatedCatalog[] = "ttyS0\0";
    SM_InputCmpsMngr_setPortCatalog(
        &updatedPortManager, updatedCatalog,
        sizeof(updatedCatalog));
    failed += updatedPortManager.candidateCount == 1U ? 0 : 1;
    dispatchInput_(&updatedPortManager, UI_KEY_TAB_SIG,
                   (char const *)0);
    failed += strcmp(updatedPortManager.buffer,
                     "$connect ttyS0 ") == 0
              ? 0 : 1;

    SM_InputCmpsMngr linuxPortManager;
    SM_InputCmpsMngr_ctor(&linuxPortManager);
    SM_InputCmpsMngr_init(&linuxPortManager);
    SM_InputCmpsMngr_setActive(&linuxPortManager, true);
    char const linuxPortCatalog[] = "/dev/ttyUSB0\0";
    SM_InputCmpsMngr_setPortCatalog(
        &linuxPortManager, linuxPortCatalog,
        sizeof(linuxPortCatalog));
    acceptCommand_(&linuxPortManager, "connect");
    dispatchAsciiText_(&linuxPortManager, "/dev/ttyUSB0");
    failed += linuxPortManager.candidateCount == 1U ? 0 : 1;
    dispatchInput_(&linuxPortManager, UI_KEY_TAB_SIG,
                   (char const *)0);
    failed += strcmp(linuxPortManager.buffer,
                     "$connect /dev/ttyUSB0 ") == 0
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
    dispatchInput_(&matchManager, UI_INPUT_SIG, "r");
    failed += strcmp(matchManager.buffer, "/r") == 0
              && matchManager.candidateCount == 2U
              && matchManager.candidates[0]
                 == SM_INPUT_COMMAND_REFRESH
              && matchManager.candidates[1]
                 == SM_INPUT_COMMAND_REFRESH_PROTOCOLS
              && l_suggestionShowCount_ == 1U
              && l_suggestionCount_ == 2U
              ? 0 : 1;

    resetProjection_();
    dispatchInput_(&matchManager, UI_KEY_BACKSPACE_SIG,
                   (char const *)0);
    failed += strcmp(matchManager.buffer, "/") == 0
              && matchManager.candidateCount == SM_INPUT_COMMAND_NUM
              && l_suggestionShowCount_ == 1U
              && l_suggestionCount_ == SM_INPUT_COMMAND_NUM
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
              && matchManager.candidateCount == SM_INPUT_COMMAND_NUM
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
    dispatchInput_(&recoveryManager, UI_INPUT_SIG, "r");
    failed += strcmp(recoveryManager.buffer, "/r") == 0
              && recoveryManager.candidateCount == 2U
              && recoveryManager.candidates[0]
                 == SM_INPUT_COMMAND_REFRESH
              && recoveryManager.candidates[1]
                 == SM_INPUT_COMMAND_REFRESH_PROTOCOLS
              && l_suggestionShowCount_ == 2U
              && l_suggestionCount_ == 2U
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
              && cursorManager.candidateCount == SM_INPUT_COMMAND_NUM
              && l_suggestionShowCount_ == 1U
              && l_suggestionCount_ == SM_INPUT_COMMAND_NUM
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
    failed += strcmp(spanManager.buffer, "$connect  rest") == 0
              && spanManager.editPos == 9U
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
              && fullManager.candidateCount == SM_INPUT_COMMAND_NUM
              && fullManager.tokenCount == 0U
              && fullManager.limitReached
              && l_limitReachedCount_ == 1U
              && l_limitReached_
              && l_projectAllCount_ == 1U
              && l_suggestionHideCount_ == 0U
              ? 0 : 1;

    SM_InputCmpsMngr tokenSpaceCapacityManager;
    SM_InputCmpsMngr_ctor(&tokenSpaceCapacityManager);
    SM_InputCmpsMngr_init(&tokenSpaceCapacityManager);
    SM_InputCmpsMngr_setActive(&tokenSpaceCapacityManager, true);
    memset(tokenSpaceCapacityManager.buffer, 'a', 246U);
    tokenSpaceCapacityManager.buffer[246] = ' ';
    tokenSpaceCapacityManager.buffer[247] = '\0';
    tokenSpaceCapacityManager.length = 247U;
    tokenSpaceCapacityManager.editPos = 247U;
    dispatchInput_(&tokenSpaceCapacityManager, UI_INPUT_SIG, "/");
    dispatchInput_(&tokenSpaceCapacityManager, UI_INPUT_SIG, "c");

    resetProjection_();
    dispatchInput_(&tokenSpaceCapacityManager, UI_KEY_ENTER_SIG,
                   (char const *)0);
    failed += tokenSpaceCapacityManager.length == 249U
              && tokenSpaceCapacityManager.editPos == 249U
              && tokenSpaceCapacityManager.buffer[247] == '/'
              && tokenSpaceCapacityManager.buffer[248] == 'c'
              && tokenSpaceCapacityManager.tokenCount == 0U
              && tokenSpaceCapacityManager.limitReached
              && l_limitReachedCount_ == 1U
              && l_projectAllCount_ == 1U
              ? 0 : 1;

    SM_InputCmpsMngr capacityManager;
    SM_InputCmpsMngr_ctor(&capacityManager);
    SM_InputCmpsMngr_init(&capacityManager);
    SM_InputCmpsMngr_setActive(&capacityManager, true);
    memset(capacityManager.buffer, 'a',
           SM_INPUT_CMPS_MNGR_BUFFER_SIZE - 1U);
    capacityManager.buffer[SM_INPUT_CMPS_MNGR_BUFFER_SIZE - 1U] = '\0';
    capacityManager.length = SM_INPUT_CMPS_MNGR_BUFFER_SIZE - 1U;
    capacityManager.editPos = capacityManager.length;

    resetProjection_();
    dispatchInput_(&capacityManager, UI_INPUT_SIG, "b");
    failed += capacityManager.limitReached
              && l_limitReachedCount_ == 1U
              && l_limitReached_
              && l_projectAllCount_ == 1U
              ? 0 : 1;

    resetProjection_();
    dispatchInput_(&capacityManager, UI_KEY_BACKSPACE_SIG,
                   (char const *)0);
    failed += !capacityManager.limitReached
              && l_limitReachedCount_ == 1U
              && !l_limitReached_
              && l_projectFromCount_ == 1U
              ? 0 : 1;

    resetProjection_();
    SM_InputCmpsMngr_setActive(&fullManager, false);
    failed += fullManager.candidateCount == 0U
              && l_suggestionHideCount_ == 1U
              && l_hideCursorCount_ == 1U
              ? 0 : 1;

    SM_InputCmpsMngr_CommandSink const commandSink = {
        .submit = &recordSubmission_,
        .reject = &recordRejection_,
        .ctx = (void *)0,
    };

    SM_InputCmpsMngr freeBaudManager;
    SM_InputCmpsMngr_ctor(&freeBaudManager);
    SM_InputCmpsMngr_setCommandSink(&freeBaudManager, &commandSink);
    SM_InputCmpsMngr_init(&freeBaudManager);
    SM_InputCmpsMngr_setActive(&freeBaudManager, true);
    acceptCommand_(&freeBaudManager, "baudrate");
    failed += freeBaudManager.candidateCount == 5U ? 0 : 1;
    dispatchAsciiText_(&freeBaudManager, "57600");
    resetSubmission_();
    dispatchInput_(&freeBaudManager, UI_KEY_ENTER_SIG,
                   (char const *)0);
    failed += l_submissionCount_ == 1U
              && l_submission_.action == SM_INPUT_ACTION_CONFIG
              && strcmp(l_submissionBaudrate_, "57600") == 0
              ? 0 : 1;

    SM_InputCmpsMngr defaultConnectManager;
    SM_InputCmpsMngr_ctor(&defaultConnectManager);
    SM_InputCmpsMngr_setCommandSink(
        &defaultConnectManager, &commandSink);
    SM_InputCmpsMngr_init(&defaultConnectManager);
    SM_InputCmpsMngr_setActive(&defaultConnectManager, true);
    SM_InputCmpsMngr_setPortCatalog(
        &defaultConnectManager, portCatalog, sizeof(portCatalog));
    acceptCommand_(&defaultConnectManager, "connect");
    resetSubmission_();
    resetProjection_();
    dispatchInput_(&defaultConnectManager, UI_KEY_ENTER_SIG,
                   (char const *)0);
    failed += l_submissionCount_ == 1U
              && l_submission_.action == SM_INPUT_ACTION_CONNECT
              && !l_submission_.port.present
              && l_suggestionHideCount_ == 1U
              ? 0 : 1;
    acceptCommand_(&defaultConnectManager, "connect");
    failed += defaultConnectManager.candidateCount == 2U ? 0 : 1;

    SM_InputCmpsMngr connectManager;
    SM_InputCmpsMngr_ctor(&connectManager);
    SM_InputCmpsMngr_setCommandSink(&connectManager, &commandSink);
    SM_InputCmpsMngr_init(&connectManager);
    SM_InputCmpsMngr_setActive(&connectManager, true);
    acceptCommand_(&connectManager, "connect");
    resetSubmission_();
    resetProjection_();
    dispatchInput_(&connectManager, UI_KEY_ENTER_SIG,
                   (char const *)0);
    failed += l_submissionCount_ == 1U
              && l_submission_.action == SM_INPUT_ACTION_CONNECT
              && !l_submission_.port.present
              && !l_submission_.baudrate.present
              && connectManager.length == 0U
              && connectManager.tokenCount == 0U
              && l_projectAllCount_ == 1U
              ? 0 : 1;

    SM_InputCmpsMngr overrideManager;
    SM_InputCmpsMngr_ctor(&overrideManager);
    SM_InputCmpsMngr_setCommandSink(&overrideManager, &commandSink);
    SM_InputCmpsMngr_init(&overrideManager);
    SM_InputCmpsMngr_setActive(&overrideManager, true);
    acceptCommand_(&overrideManager, "connect");
    dispatchAsciiText_(&overrideManager, " com3 ");
    acceptCommand_(&overrideManager, "baudrate");
    dispatchAsciiText_(&overrideManager, " 9600 ");
    acceptCommand_(&overrideManager, "dataBits");
    dispatchAsciiText_(&overrideManager, " 7 ");
    acceptCommand_(&overrideManager, "stopBits");
    dispatchAsciiText_(&overrideManager, " 2 ");
    resetSubmission_();
    dispatchInput_(&overrideManager, UI_KEY_ENTER_SIG,
                   (char const *)0);
    failed += l_submissionCount_ == 1U
              && l_submission_.action == SM_INPUT_ACTION_CONNECT
              && strcmp(l_submissionPort_, "com3") == 0
              && strcmp(l_submissionBaudrate_, "9600") == 0
              && strcmp(l_submissionDataBits_, "7") == 0
              && strcmp(l_submissionStopBits_, "2") == 0
              && overrideManager.length == 0U
              ? 0 : 1;

    SM_InputCmpsMngr removedProtocolManager;
    SM_InputCmpsMngr_ctor(&removedProtocolManager);
    SM_InputCmpsMngr_init(&removedProtocolManager);
    SM_InputCmpsMngr_setActive(&removedProtocolManager, true);
    dispatchAsciiText_(&removedProtocolManager, "/protocol");
    failed += removedProtocolManager.candidateCount == 0U ? 0 : 1;

    SM_InputCmpsMngr configManager;
    SM_InputCmpsMngr_ctor(&configManager);
    SM_InputCmpsMngr_setCommandSink(&configManager, &commandSink);
    SM_InputCmpsMngr_init(&configManager);
    SM_InputCmpsMngr_setActive(&configManager, true);
    acceptCommand_(&configManager, "baudrate");
    dispatchAsciiText_(&configManager, " 115200 ");
    acceptCommand_(&configManager, "parity");
    dispatchAsciiText_(&configManager, " none ");
    acceptCommand_(&configManager, "flowControl");
    dispatchAsciiText_(&configManager, " none");
    resetSubmission_();
    dispatchInput_(&configManager, UI_KEY_ENTER_SIG,
                   (char const *)0);
    failed += l_submissionCount_ == 1U
              && l_submission_.action == SM_INPUT_ACTION_CONFIG
              && strcmp(l_submissionBaudrate_, "115200") == 0
              && strcmp(l_submissionParity_, "none") == 0
              && strcmp(l_submissionFlowControl_, "none") == 0
              && configManager.length == 0U
              ? 0 : 1;

    SM_InputCmpsMngr disconnectManager;
    SM_InputCmpsMngr_ctor(&disconnectManager);
    SM_InputCmpsMngr_setCommandSink(&disconnectManager, &commandSink);
    SM_InputCmpsMngr_init(&disconnectManager);
    SM_InputCmpsMngr_setActive(&disconnectManager, true);
    acceptCommand_(&disconnectManager, "disconnect");
    resetSubmission_();
    dispatchInput_(&disconnectManager, UI_KEY_ENTER_SIG,
                   (char const *)0);
    failed += l_submissionCount_ == 1U
              && l_submission_.action == SM_INPUT_ACTION_DISCONNECT
              && disconnectManager.length == 0U
              ? 0 : 1;

    SM_InputCmpsMngr refreshManager;
    SM_InputCmpsMngr_ctor(&refreshManager);
    SM_InputCmpsMngr_setCommandSink(&refreshManager, &commandSink);
    SM_InputCmpsMngr_init(&refreshManager);
    SM_InputCmpsMngr_setActive(&refreshManager, true);
    acceptCommand_(&refreshManager, "refresh");
    resetSubmission_();
    dispatchInput_(&refreshManager, UI_KEY_ENTER_SIG,
                   (char const *)0);
    failed += l_submissionCount_ == 1U
              && l_submission_.action == SM_INPUT_ACTION_REFRESH
              && refreshManager.length == 0U
              ? 0 : 1;

    SM_InputCmpsMngr refreshProtocolsManager;
    SM_InputCmpsMngr_ctor(&refreshProtocolsManager);
    SM_InputCmpsMngr_setCommandSink(
        &refreshProtocolsManager, &commandSink);
    SM_InputCmpsMngr_init(&refreshProtocolsManager);
    SM_InputCmpsMngr_setActive(&refreshProtocolsManager, true);
    acceptCommand_(&refreshProtocolsManager, "refreshProtocols");
    resetSubmission_();
    dispatchInput_(&refreshProtocolsManager, UI_KEY_ENTER_SIG,
                   (char const *)0);
    failed += l_submissionCount_ == 1U
              && l_submission_.action
                 == SM_INPUT_ACTION_REFRESH_PROTOCOLS
              && refreshProtocolsManager.length == 0U
              ? 0 : 1;

    SM_InputCmpsMngr loadProtocolManager;
    SM_InputCmpsMngr_ctor(&loadProtocolManager);
    SM_InputCmpsMngr_setCommandSink(
        &loadProtocolManager, &commandSink);
    SM_InputCmpsMngr_init(&loadProtocolManager);
    SM_InputCmpsMngr_setActive(&loadProtocolManager, true);
    char const loadProtocolCatalog[] = "blinky_c51.json\0";
    SM_InputCmpsMngr_setProtocolCatalog(
        &loadProtocolManager, loadProtocolCatalog,
        sizeof(loadProtocolCatalog));
    acceptCommand_(&loadProtocolManager, "loadProtocol");
    failed += loadProtocolManager.candidateCount == 1U ? 0 : 1;
    dispatchAsciiText_(&loadProtocolManager, " blinky_c51.json");
    resetSubmission_();
    dispatchInput_(&loadProtocolManager, UI_KEY_ENTER_SIG,
                   (char const *)0);
    failed += l_submissionCount_ == 1U
              && l_submission_.action == SM_INPUT_ACTION_LOAD_PROTOCOL
              && strcmp(l_submissionProtocol_, "blinky_c51.json") == 0
              && loadProtocolManager.length == 0U
              ? 0 : 1;

    SM_InputCmpsMngr loadAndConnectManager;
    SM_InputCmpsMngr_ctor(&loadAndConnectManager);
    SM_InputCmpsMngr_setCommandSink(
        &loadAndConnectManager, &commandSink);
    SM_InputCmpsMngr_init(&loadAndConnectManager);
    SM_InputCmpsMngr_setActive(&loadAndConnectManager, true);
    acceptCommand_(&loadAndConnectManager, "loadProtocol");
    dispatchAsciiText_(&loadAndConnectManager, " blinky_c51.json ");
    acceptCommand_(&loadAndConnectManager, "connect");
    dispatchAsciiText_(&loadAndConnectManager, " /dev/ttyUSB0");
    resetSubmission_();
    dispatchInput_(&loadAndConnectManager, UI_KEY_ENTER_SIG,
                   (char const *)0);
    failed += l_submissionCount_ == 1U
              && l_submission_.action == SM_INPUT_ACTION_CONNECT
              && strcmp(l_submissionProtocol_, "blinky_c51.json") == 0
              && strcmp(l_submissionPort_, "/dev/ttyUSB0") == 0
              && loadAndConnectManager.length == 0U
              ? 0 : 1;

    SM_InputCmpsMngr missingProtocolManager;
    SM_InputCmpsMngr_ctor(&missingProtocolManager);
    SM_InputCmpsMngr_setCommandSink(
        &missingProtocolManager, &commandSink);
    SM_InputCmpsMngr_init(&missingProtocolManager);
    SM_InputCmpsMngr_setActive(&missingProtocolManager, true);
    acceptCommand_(&missingProtocolManager, "loadProtocol");
    resetSubmission_();
    dispatchInput_(&missingProtocolManager, UI_KEY_ENTER_SIG,
                   (char const *)0);
    failed += l_submissionCount_ == 0U
              && missingProtocolManager.tokenCount == 1U
              && l_rejectionCount_ == 1U
              && strcmp(l_rejectionReason_,
                        "command requires an argument") == 0
              ? 0 : 1;

    SM_InputCmpsMngr mixedManager;
    SM_InputCmpsMngr_ctor(&mixedManager);
    SM_InputCmpsMngr_setCommandSink(&mixedManager, &commandSink);
    SM_InputCmpsMngr_init(&mixedManager);
    SM_InputCmpsMngr_setActive(&mixedManager, true);
    acceptCommand_(&mixedManager, "connect");
    acceptCommand_(&mixedManager, "disconnect");
    resetSubmission_();
    dispatchInput_(&mixedManager, UI_KEY_ENTER_SIG,
                   (char const *)0);
    failed += l_submissionCount_ == 0U
              && strcmp(mixedManager.buffer,
                        "$connect $disconnect ") == 0
              && mixedManager.tokenCount == 2U
              && l_rejectionCount_ == 1U
              && strcmp(l_rejectionReason_,
                        "lifecycle commands cannot be combined") == 0
              ? 0 : 1;

    SM_InputCmpsMngr invalidArgsManager;
    SM_InputCmpsMngr_ctor(&invalidArgsManager);
    SM_InputCmpsMngr_setCommandSink(&invalidArgsManager, &commandSink);
    SM_InputCmpsMngr_init(&invalidArgsManager);
    SM_InputCmpsMngr_setActive(&invalidArgsManager, true);
    acceptCommand_(&invalidArgsManager, "disconnect");
    dispatchAsciiText_(&invalidArgsManager, "now");
    resetSubmission_();
    dispatchInput_(&invalidArgsManager, UI_KEY_ENTER_SIG,
                   (char const *)0);
    failed += l_submissionCount_ == 0U
              && strcmp(invalidArgsManager.buffer,
                        "$disconnect now") == 0
              && l_rejectionCount_ == 1U
              && strcmp(l_rejectionReason_,
                        "command does not accept an argument") == 0
              ? 0 : 1;

    SM_InputCmpsMngr reorderedManager;
    SM_InputCmpsMngr_ctor(&reorderedManager);
    SM_InputCmpsMngr_setCommandSink(&reorderedManager, &commandSink);
    SM_InputCmpsMngr_init(&reorderedManager);
    SM_InputCmpsMngr_setActive(&reorderedManager, true);
    acceptCommand_(&reorderedManager, "baudrate");
    dispatchAsciiText_(&reorderedManager, " 57600 ");
    acceptCommand_(&reorderedManager, "connect");
    dispatchAsciiText_(&reorderedManager, " ttyUSB0");
    resetSubmission_();
    dispatchInput_(&reorderedManager, UI_KEY_ENTER_SIG,
                   (char const *)0);
    failed += l_submissionCount_ == 1U
              && l_submission_.action == SM_INPUT_ACTION_CONNECT
              && strcmp(l_submissionPort_, "ttyUSB0") == 0
              && strcmp(l_submissionBaudrate_, "57600") == 0
              ? 0 : 1;

    SM_InputCmpsMngr duplicateManager;
    SM_InputCmpsMngr_ctor(&duplicateManager);
    SM_InputCmpsMngr_setCommandSink(&duplicateManager, &commandSink);
    SM_InputCmpsMngr_init(&duplicateManager);
    SM_InputCmpsMngr_setActive(&duplicateManager, true);
    acceptCommand_(&duplicateManager, "connect");
    acceptCommand_(&duplicateManager, "connect");
    resetSubmission_();
    dispatchInput_(&duplicateManager, UI_KEY_ENTER_SIG,
                   (char const *)0);
    failed += l_submissionCount_ == 0U
              && duplicateManager.tokenCount == 2U
              && l_rejectionCount_ == 1U
              && strcmp(l_rejectionReason_, "duplicate command") == 0
              ? 0 : 1;

    SM_InputCmpsMngr missingValueManager;
    SM_InputCmpsMngr_ctor(&missingValueManager);
    SM_InputCmpsMngr_setCommandSink(&missingValueManager, &commandSink);
    SM_InputCmpsMngr_init(&missingValueManager);
    SM_InputCmpsMngr_setActive(&missingValueManager, true);
    acceptCommand_(&missingValueManager, "baudrate");
    resetSubmission_();
    dispatchInput_(&missingValueManager, UI_KEY_ENTER_SIG,
                   (char const *)0);
    failed += l_submissionCount_ == 0U
              && missingValueManager.tokenCount == 1U
              && l_rejectionCount_ == 1U
              && strcmp(l_rejectionReason_,
                        "command requires an argument") == 0
              ? 0 : 1;

    SM_InputCmpsMngr refreshConfigManager;
    SM_InputCmpsMngr_ctor(&refreshConfigManager);
    SM_InputCmpsMngr_setCommandSink(&refreshConfigManager, &commandSink);
    SM_InputCmpsMngr_init(&refreshConfigManager);
    SM_InputCmpsMngr_setActive(&refreshConfigManager, true);
    acceptCommand_(&refreshConfigManager, "refresh");
    acceptCommand_(&refreshConfigManager, "parity");
    dispatchAsciiText_(&refreshConfigManager, " odd");
    resetSubmission_();
    dispatchInput_(&refreshConfigManager, UI_KEY_ENTER_SIG,
                   (char const *)0);
    failed += l_submissionCount_ == 0U
              && refreshConfigManager.tokenCount == 2U
              && l_rejectionCount_ == 1U
              && strcmp(
                    l_rejectionReason_,
                    "disconnect and refresh commands must be used alone")
                 == 0
              ? 0 : 1;

    SM_InputCmpsMngr joinedTokenManager;
    SM_InputCmpsMngr_ctor(&joinedTokenManager);
    SM_InputCmpsMngr_setCommandSink(&joinedTokenManager, &commandSink);
    SM_InputCmpsMngr_init(&joinedTokenManager);
    SM_InputCmpsMngr_setActive(&joinedTokenManager, true);
    acceptCommand_(&joinedTokenManager, "connect");
    acceptCommand_(&joinedTokenManager, "baudrate");
    dispatchInput_(&joinedTokenManager, UI_KEY_HOME_SIG,
                   (char const *)0);
    dispatchInput_(&joinedTokenManager, UI_KEY_RIGHT_SIG,
                   (char const *)0);
    dispatchInput_(&joinedTokenManager, UI_KEY_RIGHT_SIG,
                   (char const *)0);
    dispatchInput_(&joinedTokenManager, UI_KEY_BACKSPACE_SIG,
                   (char const *)0);
    dispatchInput_(&joinedTokenManager, UI_KEY_END_SIG,
                   (char const *)0);
    dispatchAsciiText_(&joinedTokenManager, " 9600");
    resetSubmission_();
    dispatchInput_(&joinedTokenManager, UI_KEY_ENTER_SIG,
                   (char const *)0);
    failed += l_submissionCount_ == 0U
              && joinedTokenManager.tokenCount == 2U
              && l_rejectionCount_ == 1U
              && strcmp(
                    l_rejectionReason_,
                    "command tokens must be separated by whitespace") == 0
              ? 0 : 1;

    SM_InputCmpsMngr joinedArgManager;
    SM_InputCmpsMngr_ctor(&joinedArgManager);
    SM_InputCmpsMngr_setCommandSink(&joinedArgManager, &commandSink);
    SM_InputCmpsMngr_init(&joinedArgManager);
    SM_InputCmpsMngr_setActive(&joinedArgManager, true);
    acceptCommand_(&joinedArgManager, "connect");
    dispatchInput_(&joinedArgManager, UI_KEY_BACKSPACE_SIG,
                   (char const *)0);
    dispatchAsciiText_(&joinedArgManager, "com3");
    resetSubmission_();
    dispatchInput_(&joinedArgManager, UI_KEY_ENTER_SIG,
                   (char const *)0);
    failed += l_submissionCount_ == 0U
              && joinedArgManager.tokenCount == 1U
              && l_rejectionCount_ == 1U
              && strcmp(
                    l_rejectionReason_,
                    "command tokens must be separated by whitespace") == 0
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
