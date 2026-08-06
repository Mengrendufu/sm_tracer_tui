//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
//============================================================================
//=== InputComposer Manager HSM -- input editing state owner
#include <string.h>
#include "sm_port.h"
#include "sm_hsm.h"
#include "dbc_assert.h"
#include "sm_input_cmps_mngr.h"
DBC_MODULE_NAME("sm_input_cmps_mngr")

//============================================================================
//=== Private command and event contracts

typedef struct {
    SM_InputCommand id;
    char const *name;
    char const *suggestion;
    char const *token;
} InputCommandDef;

static InputCommandDef const l_commands_[SM_INPUT_COMMAND_NUM] = {
    {SM_INPUT_COMMAND_BAUDRATE,
     "baudrate", "/baudrate", "$baudrate"},
    {SM_INPUT_COMMAND_CONNECT,
     "connect", "/connect", "$connect"},
    {SM_INPUT_COMMAND_DATA_BITS,
     "dataBits", "/dataBits", "$dataBits"},
    {SM_INPUT_COMMAND_DISCONNECT,
     "disconnect", "/disconnect", "$disconnect"},
    {SM_INPUT_COMMAND_FLOW_CONTROL,
     "flowControl", "/flowControl", "$flowControl"},
    {SM_INPUT_COMMAND_LOAD_PROTOCOL,
     "loadProtocol", "/loadProtocol", "$loadProtocol"},
    {SM_INPUT_COMMAND_PARITY,
     "parity", "/parity", "$parity"},
    {SM_INPUT_COMMAND_REFRESH,
     "refresh", "/refresh", "$refresh"},
    {SM_INPUT_COMMAND_REFRESH_PROTOCOLS,
     "refreshProtocols", "/refreshProtocols", "$refreshProtocols"},
    {SM_INPUT_COMMAND_STOP_BITS,
     "stopBits", "/stopBits", "$stopBits"},
};

static char const * const l_baudrateArgs_[] = {
    "2400", "4800", "9600", "115200", "921600"
};
static char const * const l_dataBitsArgs_[] = {
    "5", "6", "7", "8"
};
static char const * const l_stopBitsArgs_[] = {
    "1", "1.5", "2"
};
static char const * const l_parityArgs_[] = {
    "none", "odd", "even", "mark", "space"
};
static char const * const l_flowControlArgs_[] = {
    "none", "xon/xoff", "rts/cts", "dtr/dsr"
};

enum InputCmpsMngrSignals {
    INPUT_CMPS_MNGR_NULL_SIG = 0,
    INPUT_CMPS_MNGR_ACTIVATE_SIG,
    INPUT_CMPS_MNGR_DEACTIVATE_SIG,
    INPUT_CMPS_MNGR_TERM_INPUT_SIG,
    INPUT_CMPS_MNGR_SLASH_INPUT_SIG,
    INPUT_CMPS_MNGR_CANCEL_SIG,
    INPUT_CMPS_MNGR_SELECT_PREV_SIG,
    INPUT_CMPS_MNGR_SELECT_NEXT_SIG,
    INPUT_CMPS_MNGR_DELETE_PREV_SIG,
    INPUT_CMPS_MNGR_DELETE_TO_START_SIG,
    INPUT_CMPS_MNGR_DELETE_PREV_WORD_SIG,
    INPUT_CMPS_MNGR_MOVE_LEFT_SIG,
    INPUT_CMPS_MNGR_MOVE_RIGHT_SIG,
    INPUT_CMPS_MNGR_MOVE_PREV_WORD_SIG,
    INPUT_CMPS_MNGR_MOVE_NEXT_WORD_SIG,
    INPUT_CMPS_MNGR_MOVE_HOME_SIG,
    INPUT_CMPS_MNGR_MOVE_END_SIG,
    INPUT_CMPS_MNGR_COMPLETE_SIG,
    INPUT_CMPS_MNGR_CONFIRM_SIG,
    INPUT_CMPS_MNGR_ARGUMENT_CATALOG_UPDATED_SIG,
};

typedef struct {
    UI_Evt super;
    UI_Input input;
} InputCmpsMngrEvt;

typedef enum {
    INPUT_SUGGEST_AVAILABLE,
    INPUT_SUGGEST_NOT_AVAILABLE,
    INPUT_SUGGEST_CANDIDATES_AVAILABLE,
    INPUT_SUGGEST_ARGUMENT_CANDIDATES,
} InputSuggestionContext;

static UI_Signal SM_InputCmpsMngr_routeSignal_(UI_InputEvt const *e);
static size_t    SM_InputCmpsMngr_utf8CodepointSize_(
                        char const *text, size_t remaining);
static size_t    SM_InputCmpsMngr_inputSize_(UI_Input const *input);
static size_t    SM_InputCmpsMngr_prevPos_(
                        SM_InputCmpsMngr const *me, size_t pos);
static size_t    SM_InputCmpsMngr_nextPos_(
                        SM_InputCmpsMngr const *me, size_t pos);
static bool      SM_InputCmpsMngr_isWhitespaceAt_(
                        SM_InputCmpsMngr const *me, size_t pos);
static bool      SM_InputCmpsMngr_tokenEndingAt_(
                        SM_InputCmpsMngr const *me, size_t pos,
                        size_t *tokenIndex);
static bool      SM_InputCmpsMngr_tokenStartingAt_(
                        SM_InputCmpsMngr const *me, size_t pos,
                        size_t *tokenIndex);
static void      SM_InputCmpsMngr_validate_(SM_InputCmpsMngr const *me);
static void      SM_InputCmpsMngr_setLimitReached_(
                        SM_InputCmpsMngr *me, bool reached);
static void      SM_InputCmpsMngr_highlightTokens_(
                        SM_InputCmpsMngr *me);
static bool      SM_InputCmpsMngr_insertBytes_(
                        SM_InputCmpsMngr *me, char const *text,
                        size_t size, bool project);
static void      SM_InputCmpsMngr_removeRange_(
                        SM_InputCmpsMngr *me, size_t begin,
                        size_t end, bool project);
static bool      SM_InputCmpsMngr_insert_(
                        SM_InputCmpsMngr *me, UI_Input const *input);
static void      SM_InputCmpsMngr_deletePrev_(SM_InputCmpsMngr *me);
static void      SM_InputCmpsMngr_deleteToStart_(SM_InputCmpsMngr *me);
static void      SM_InputCmpsMngr_deletePrevWord_(SM_InputCmpsMngr *me);
static void      SM_InputCmpsMngr_moveLeft_(SM_InputCmpsMngr *me);
static void      SM_InputCmpsMngr_moveRight_(SM_InputCmpsMngr *me);
static void      SM_InputCmpsMngr_movePrevWord_(SM_InputCmpsMngr *me);
static void      SM_InputCmpsMngr_moveNextWord_(SM_InputCmpsMngr *me);
static void      SM_InputCmpsMngr_moveHome_(SM_InputCmpsMngr *me);
static void      SM_InputCmpsMngr_moveEnd_(SM_InputCmpsMngr *me);
static void      SM_InputCmpsMngr_filterCandidates_(
                        SM_InputCmpsMngr *me);
static void      SM_InputCmpsMngr_filterArgumentCandidates_(
                        SM_InputCmpsMngr *me,
                        SM_InputCommand command,
                        size_t prefixBegin,
                        size_t prefixEnd);
static InputSuggestionContext SM_InputCmpsMngr_suggestionContext_(
                        SM_InputCmpsMngr *me);
static void      SM_InputCmpsMngr_showSuggestions_(
                        SM_InputCmpsMngr *me);
static void      SM_InputCmpsMngr_showArgumentSuggestions_(
                        SM_InputCmpsMngr *me);
static bool      SM_InputCmpsMngr_acceptSuggestion_(
                        SM_InputCmpsMngr *me);
static bool      SM_InputCmpsMngr_acceptArgumentSuggestion_(
                        SM_InputCmpsMngr *me);
static bool      SM_InputCmpsMngr_isWhitespaceRange_(
                        SM_InputCmpsMngr const *me,
                        size_t begin,
                        size_t end);
static bool      SM_InputCmpsMngr_buildSubmission_(
                        SM_InputCmpsMngr const *me,
                        SM_InputCmpsMngrSubmission *submission,
                        char const **rejectionReason);
static void      SM_InputCmpsMngr_clear_(SM_InputCmpsMngr *me);

//============================================================================
//=== HSM states

// TOP-INIT
static SM_StatePtr SM_InputCmpsMngr_TOP_initial(SM_Hsm * const me) SM_HSM_RETT;

// inactive
static SM_RetState SM_InputCmpsMngr_inactive_(SM_Hsm * const me, UI_Evt const * const e) SM_HSM_RETT;
SM_HsmState SM_HSM_ROM SM_InputCmpsMngr_inactive = {
    SM_HSM_TOP,                                         // super
    (SM_InitHandler)0,                                  // init_
    (SM_ActionHandler)0,                                // entry_
    (SM_ActionHandler)0,                                // exit_
    (SM_StateHandler)&SM_InputCmpsMngr_inactive_        // handler
};

// active
static SM_StatePtr SM_InputCmpsMngr_active_init_(SM_Hsm * const me) SM_HSM_RETT;
static void        SM_InputCmpsMngr_active_entry_(SM_Hsm * const me) SM_HSM_RETT;
static void        SM_InputCmpsMngr_active_exit_(SM_Hsm * const me) SM_HSM_RETT;
static SM_RetState SM_InputCmpsMngr_active_(SM_Hsm * const me, UI_Evt const * const e) SM_HSM_RETT;
SM_HsmState SM_HSM_ROM SM_InputCmpsMngr_active = {
    SM_HSM_TOP,                                         // super
    (SM_InitHandler)&SM_InputCmpsMngr_active_init_,     // init_
    (SM_ActionHandler)&SM_InputCmpsMngr_active_entry_,  // entry_
    (SM_ActionHandler)&SM_InputCmpsMngr_active_exit_,   // exit_
    (SM_StateHandler)&SM_InputCmpsMngr_active_          // handler
};

// normal
static SM_StatePtr SM_InputCmpsMngr_normal_init_(SM_Hsm * const me) SM_HSM_RETT;
static SM_RetState SM_InputCmpsMngr_normal_(SM_Hsm * const me, UI_Evt const * const e) SM_HSM_RETT;
SM_HsmState SM_HSM_ROM SM_InputCmpsMngr_normal = {
    (SM_StatePtr)&SM_InputCmpsMngr_active,              // super
    (SM_InitHandler)&SM_InputCmpsMngr_normal_init_,     // init_
    (SM_ActionHandler)0,                                // entry_
    (SM_ActionHandler)0,                                // exit_
    (SM_StateHandler)&SM_InputCmpsMngr_normal_          // handler
};

// suggestAvailable
static SM_RetState SM_InputCmpsMngr_suggestAvailable_(SM_Hsm * const me, UI_Evt const * const e) SM_HSM_RETT;
SM_HsmState SM_HSM_ROM SM_InputCmpsMngr_suggestAvailable = {
    (SM_StatePtr)&SM_InputCmpsMngr_normal,              // super
    (SM_InitHandler)0,                                  // init_
    (SM_ActionHandler)0,                                // entry_
    (SM_ActionHandler)0,                                // exit_
    (SM_StateHandler)&SM_InputCmpsMngr_suggestAvailable_ // handler
};

// suggestNotAvailable
static SM_RetState SM_InputCmpsMngr_suggestNotAvailable_(SM_Hsm * const me, UI_Evt const * const e) SM_HSM_RETT;
SM_HsmState SM_HSM_ROM SM_InputCmpsMngr_suggestNotAvailable = {
    (SM_StatePtr)&SM_InputCmpsMngr_normal,                // super
    (SM_InitHandler)0,                                    // init_
    (SM_ActionHandler)0,                                  // entry_
    (SM_ActionHandler)0,                                  // exit_
    (SM_StateHandler)&SM_InputCmpsMngr_suggestNotAvailable_ // handler
};

// commandSuggesting
static void        SM_InputCmpsMngr_commandSuggesting_entry_(SM_Hsm * const me) SM_HSM_RETT;
static void        SM_InputCmpsMngr_commandSuggesting_exit_(SM_Hsm * const me) SM_HSM_RETT;
static SM_RetState SM_InputCmpsMngr_commandSuggesting_(SM_Hsm * const me, UI_Evt const * const e) SM_HSM_RETT;
SM_HsmState SM_HSM_ROM SM_InputCmpsMngr_commandSuggesting = {
    (SM_StatePtr)&SM_InputCmpsMngr_active,                 // super
    (SM_InitHandler)0,                                     // init_
    (SM_ActionHandler)&SM_InputCmpsMngr_commandSuggesting_entry_, // entry_
    (SM_ActionHandler)&SM_InputCmpsMngr_commandSuggesting_exit_, // exit_
    (SM_StateHandler)&SM_InputCmpsMngr_commandSuggesting_  // handler
};

// argumentSuggesting
static void        SM_InputCmpsMngr_argumentSuggesting_entry_(SM_Hsm * const me) SM_HSM_RETT;
static void        SM_InputCmpsMngr_argumentSuggesting_exit_(SM_Hsm * const me) SM_HSM_RETT;
static SM_RetState SM_InputCmpsMngr_argumentSuggesting_(SM_Hsm * const me, UI_Evt const * const e) SM_HSM_RETT;
SM_HsmState SM_HSM_ROM SM_InputCmpsMngr_argumentSuggesting = {
    (SM_StatePtr)&SM_InputCmpsMngr_normal,                  // super
    (SM_InitHandler)0,                                      // init_
    (SM_ActionHandler)&SM_InputCmpsMngr_argumentSuggesting_entry_, // entry_
    (SM_ActionHandler)&SM_InputCmpsMngr_argumentSuggesting_exit_, // exit_
    (SM_StateHandler)&SM_InputCmpsMngr_argumentSuggesting_   // handler
};

//============================================================================
//=== HSM implementations

static SM_StatePtr SM_InputCmpsMngr_TOP_initial(
    SM_Hsm * const me) SM_HSM_RETT
{
    (void)me;
    return _SM_INIT(&SM_InputCmpsMngr_inactive);
}

static SM_RetState SM_InputCmpsMngr_inactive_(
    SM_Hsm * const me,
    UI_Evt const * const e) SM_HSM_RETT
{
    SM_InputCmpsMngr *manager = containerof(
        me, SM_InputCmpsMngr, super);

    switch (e->sig) {
        case INPUT_CMPS_MNGR_ACTIVATE_SIG: {
            InputSuggestionContext const context =
                SM_InputCmpsMngr_suggestionContext_(manager);

            if (context == INPUT_SUGGEST_CANDIDATES_AVAILABLE) {
                return _SM_TRAN(&SM_InputCmpsMngr_commandSuggesting);
            } else if (context == INPUT_SUGGEST_ARGUMENT_CANDIDATES) {
                return _SM_TRAN(
                    &SM_InputCmpsMngr_argumentSuggesting);
            } else if (context == INPUT_SUGGEST_AVAILABLE) {
                return _SM_TRAN(&SM_InputCmpsMngr_suggestAvailable);
            } else {
                return _SM_TRAN(&SM_InputCmpsMngr_suggestNotAvailable);
            }
        }

        case INPUT_CMPS_MNGR_DEACTIVATE_SIG: {
            return _SM_HANDLED();
        }

        default: {
            return _SM_SUPER();
        }
    }
}

static SM_StatePtr SM_InputCmpsMngr_active_init_(
    SM_Hsm * const me) SM_HSM_RETT
{
    (void)me;
    return _SM_INIT(&SM_InputCmpsMngr_normal);
}

static void SM_InputCmpsMngr_active_entry_(
    SM_Hsm * const me) SM_HSM_RETT
{
    SM_InputCmpsMngr *manager = containerof(
        me, SM_InputCmpsMngr, super);
    InputComposer_showCursor(&manager->composer, manager->buffer,
                             manager->length, manager->editPos);
    SM_InputCmpsMngr_highlightTokens_(manager);
}

static void SM_InputCmpsMngr_active_exit_(
    SM_Hsm * const me) SM_HSM_RETT
{
    SM_InputCmpsMngr *manager = containerof(
        me, SM_InputCmpsMngr, super);
    InputComposer_hideCursor(&manager->composer, manager->buffer,
                             manager->length, manager->editPos);
    SM_InputCmpsMngr_highlightTokens_(manager);
    manager->candidateCount = 0U;
    manager->selectedCandidate = 0U;
}

static SM_RetState SM_InputCmpsMngr_active_(
    SM_Hsm * const me,
    UI_Evt const * const e) SM_HSM_RETT
{
    SM_InputCmpsMngr *manager = containerof(
        me, SM_InputCmpsMngr, super);

    switch (e->sig) {
        case INPUT_CMPS_MNGR_DEACTIVATE_SIG: {
            return _SM_TRAN(&SM_InputCmpsMngr_inactive);
        }

        case INPUT_CMPS_MNGR_ACTIVATE_SIG: {
            return _SM_HANDLED();
        }

        case INPUT_CMPS_MNGR_CONFIRM_SIG: {
            SM_InputCmpsMngrSubmission submission;
            char const *rejectionReason;
            bool const valid = SM_InputCmpsMngr_buildSubmission_(
                manager, &submission, &rejectionReason);

            if (valid) {
                DBC_ASSERT(590,
                    manager->commandSink.submit
                        != (void (*)(
                            void *,
                            SM_InputCmpsMngrSubmission const *))0);
                (*manager->commandSink.submit)(
                    manager->commandSink.ctx, &submission);
                SM_InputCmpsMngr_clear_(manager);
                return _SM_TRAN(&SM_InputCmpsMngr_suggestAvailable);
            } else {
                if (manager->commandSink.reject
                    != (void (*)(void *, char const *))0)
                {
                    (*manager->commandSink.reject)(
                        manager->commandSink.ctx, rejectionReason);
                }
                return _SM_HANDLED();
            }
        }

        case INPUT_CMPS_MNGR_ARGUMENT_CATALOG_UPDATED_SIG: {
            InputSuggestionContext const context =
                SM_InputCmpsMngr_suggestionContext_(manager);

            if (context == INPUT_SUGGEST_CANDIDATES_AVAILABLE) {
                return _SM_TRAN(&SM_InputCmpsMngr_commandSuggesting);
            } else if (context == INPUT_SUGGEST_ARGUMENT_CANDIDATES) {
                return _SM_TRAN(
                    &SM_InputCmpsMngr_argumentSuggesting);
            } else if (context == INPUT_SUGGEST_AVAILABLE) {
                return _SM_TRAN(&SM_InputCmpsMngr_suggestAvailable);
            } else {
                return _SM_TRAN(&SM_InputCmpsMngr_suggestNotAvailable);
            }
        }

        default: {
            return _SM_SUPER();
        }
    }
}

static SM_StatePtr SM_InputCmpsMngr_normal_init_(
    SM_Hsm * const me) SM_HSM_RETT
{
    (void)me;
    return _SM_INIT(&SM_InputCmpsMngr_suggestAvailable);
}

static SM_RetState SM_InputCmpsMngr_normal_(
    SM_Hsm * const me,
    UI_Evt const * const e) SM_HSM_RETT
{
    SM_InputCmpsMngr *manager = containerof(me, SM_InputCmpsMngr, super);

    switch (e->sig) {
        case INPUT_CMPS_MNGR_SLASH_INPUT_SIG: {
            InputCmpsMngrEvt const * const inputEvt =
                (InputCmpsMngrEvt const *)e;
            if (SM_InputCmpsMngr_insert_(manager,
                                         &inputEvt->input))
            {
                InputSuggestionContext const context =
                    SM_InputCmpsMngr_suggestionContext_(manager);

                if (context == INPUT_SUGGEST_CANDIDATES_AVAILABLE) {
                    return _SM_TRAN(
                        &SM_InputCmpsMngr_commandSuggesting);
                } else if (
                    context == INPUT_SUGGEST_ARGUMENT_CANDIDATES)
                {
                    return _SM_TRAN(
                        &SM_InputCmpsMngr_argumentSuggesting);
                } else if (context == INPUT_SUGGEST_AVAILABLE) {
                    return _SM_TRAN(
                        &SM_InputCmpsMngr_suggestAvailable);
                } else {
                    return _SM_TRAN(
                        &SM_InputCmpsMngr_suggestNotAvailable);
                }
            } else {
                return _SM_HANDLED();
            }
        }

        case INPUT_CMPS_MNGR_TERM_INPUT_SIG: {
            InputCmpsMngrEvt const * const inputEvt =
                (InputCmpsMngrEvt const *)e;
            if (SM_InputCmpsMngr_insert_(manager,
                                         &inputEvt->input))
            {
                InputSuggestionContext const context =
                    SM_InputCmpsMngr_suggestionContext_(manager);

                if (context == INPUT_SUGGEST_CANDIDATES_AVAILABLE) {
                    return _SM_TRAN(
                        &SM_InputCmpsMngr_commandSuggesting);
                } else if (
                    context == INPUT_SUGGEST_ARGUMENT_CANDIDATES)
                {
                    return _SM_TRAN(
                        &SM_InputCmpsMngr_argumentSuggesting);
                } else if (context == INPUT_SUGGEST_AVAILABLE) {
                    return _SM_TRAN(&SM_InputCmpsMngr_suggestAvailable);
                } else {
                    return _SM_TRAN(
                        &SM_InputCmpsMngr_suggestNotAvailable);
                }
            } else {
                return _SM_HANDLED();
            }
        }

        case INPUT_CMPS_MNGR_DELETE_PREV_SIG: {
            SM_InputCmpsMngr_deletePrev_(manager);
            InputSuggestionContext const context =
                SM_InputCmpsMngr_suggestionContext_(manager);

            if (context == INPUT_SUGGEST_CANDIDATES_AVAILABLE) {
                return _SM_TRAN(&SM_InputCmpsMngr_commandSuggesting);
            } else if (context == INPUT_SUGGEST_ARGUMENT_CANDIDATES) {
                return _SM_TRAN(
                    &SM_InputCmpsMngr_argumentSuggesting);
            } else if (context == INPUT_SUGGEST_AVAILABLE) {
                return _SM_TRAN(&SM_InputCmpsMngr_suggestAvailable);
            } else {
                return _SM_TRAN(&SM_InputCmpsMngr_suggestNotAvailable);
            }
        }

        case INPUT_CMPS_MNGR_DELETE_TO_START_SIG: {
            SM_InputCmpsMngr_deleteToStart_(manager);
            InputSuggestionContext const context =
                SM_InputCmpsMngr_suggestionContext_(manager);

            if (context == INPUT_SUGGEST_CANDIDATES_AVAILABLE) {
                return _SM_TRAN(&SM_InputCmpsMngr_commandSuggesting);
            } else if (context == INPUT_SUGGEST_ARGUMENT_CANDIDATES) {
                return _SM_TRAN(
                    &SM_InputCmpsMngr_argumentSuggesting);
            } else if (context == INPUT_SUGGEST_AVAILABLE) {
                return _SM_TRAN(&SM_InputCmpsMngr_suggestAvailable);
            } else {
                return _SM_TRAN(&SM_InputCmpsMngr_suggestNotAvailable);
            }
        }

        case INPUT_CMPS_MNGR_DELETE_PREV_WORD_SIG: {
            SM_InputCmpsMngr_deletePrevWord_(manager);
            InputSuggestionContext const context =
                SM_InputCmpsMngr_suggestionContext_(manager);

            if (context == INPUT_SUGGEST_CANDIDATES_AVAILABLE) {
                return _SM_TRAN(&SM_InputCmpsMngr_commandSuggesting);
            } else if (context == INPUT_SUGGEST_ARGUMENT_CANDIDATES) {
                return _SM_TRAN(
                    &SM_InputCmpsMngr_argumentSuggesting);
            } else if (context == INPUT_SUGGEST_AVAILABLE) {
                return _SM_TRAN(&SM_InputCmpsMngr_suggestAvailable);
            } else {
                return _SM_TRAN(&SM_InputCmpsMngr_suggestNotAvailable);
            }
        }

        case INPUT_CMPS_MNGR_MOVE_LEFT_SIG: {
            SM_InputCmpsMngr_moveLeft_(manager);
            InputSuggestionContext const context =
                SM_InputCmpsMngr_suggestionContext_(manager);

            if (context == INPUT_SUGGEST_CANDIDATES_AVAILABLE) {
                return _SM_TRAN(&SM_InputCmpsMngr_commandSuggesting);
            } else if (context == INPUT_SUGGEST_ARGUMENT_CANDIDATES) {
                return _SM_TRAN(
                    &SM_InputCmpsMngr_argumentSuggesting);
            } else if (context == INPUT_SUGGEST_AVAILABLE) {
                return _SM_TRAN(&SM_InputCmpsMngr_suggestAvailable);
            } else {
                return _SM_TRAN(&SM_InputCmpsMngr_suggestNotAvailable);
            }
        }

        case INPUT_CMPS_MNGR_MOVE_RIGHT_SIG: {
            SM_InputCmpsMngr_moveRight_(manager);
            InputSuggestionContext const context =
                SM_InputCmpsMngr_suggestionContext_(manager);

            if (context == INPUT_SUGGEST_CANDIDATES_AVAILABLE) {
                return _SM_TRAN(&SM_InputCmpsMngr_commandSuggesting);
            } else if (context == INPUT_SUGGEST_ARGUMENT_CANDIDATES) {
                return _SM_TRAN(
                    &SM_InputCmpsMngr_argumentSuggesting);
            } else if (context == INPUT_SUGGEST_AVAILABLE) {
                return _SM_TRAN(&SM_InputCmpsMngr_suggestAvailable);
            } else {
                return _SM_TRAN(&SM_InputCmpsMngr_suggestNotAvailable);
            }
        }

        case INPUT_CMPS_MNGR_MOVE_PREV_WORD_SIG: {
            SM_InputCmpsMngr_movePrevWord_(manager);
            InputSuggestionContext const context =
                SM_InputCmpsMngr_suggestionContext_(manager);

            if (context == INPUT_SUGGEST_CANDIDATES_AVAILABLE) {
                return _SM_TRAN(&SM_InputCmpsMngr_commandSuggesting);
            } else if (context == INPUT_SUGGEST_ARGUMENT_CANDIDATES) {
                return _SM_TRAN(
                    &SM_InputCmpsMngr_argumentSuggesting);
            } else if (context == INPUT_SUGGEST_AVAILABLE) {
                return _SM_TRAN(&SM_InputCmpsMngr_suggestAvailable);
            } else {
                return _SM_TRAN(&SM_InputCmpsMngr_suggestNotAvailable);
            }
        }

        case INPUT_CMPS_MNGR_MOVE_NEXT_WORD_SIG: {
            SM_InputCmpsMngr_moveNextWord_(manager);
            InputSuggestionContext const context =
                SM_InputCmpsMngr_suggestionContext_(manager);

            if (context == INPUT_SUGGEST_CANDIDATES_AVAILABLE) {
                return _SM_TRAN(&SM_InputCmpsMngr_commandSuggesting);
            } else if (context == INPUT_SUGGEST_ARGUMENT_CANDIDATES) {
                return _SM_TRAN(
                    &SM_InputCmpsMngr_argumentSuggesting);
            } else if (context == INPUT_SUGGEST_AVAILABLE) {
                return _SM_TRAN(&SM_InputCmpsMngr_suggestAvailable);
            } else {
                return _SM_TRAN(&SM_InputCmpsMngr_suggestNotAvailable);
            }
        }

        case INPUT_CMPS_MNGR_MOVE_HOME_SIG: {
            SM_InputCmpsMngr_moveHome_(manager);
            InputSuggestionContext const context =
                SM_InputCmpsMngr_suggestionContext_(manager);

            if (context == INPUT_SUGGEST_CANDIDATES_AVAILABLE) {
                return _SM_TRAN(&SM_InputCmpsMngr_commandSuggesting);
            } else if (context == INPUT_SUGGEST_ARGUMENT_CANDIDATES) {
                return _SM_TRAN(
                    &SM_InputCmpsMngr_argumentSuggesting);
            } else if (context == INPUT_SUGGEST_AVAILABLE) {
                return _SM_TRAN(&SM_InputCmpsMngr_suggestAvailable);
            } else {
                return _SM_TRAN(&SM_InputCmpsMngr_suggestNotAvailable);
            }
        }

        case INPUT_CMPS_MNGR_MOVE_END_SIG: {
            SM_InputCmpsMngr_moveEnd_(manager);
            InputSuggestionContext const context =
                SM_InputCmpsMngr_suggestionContext_(manager);

            if (context == INPUT_SUGGEST_CANDIDATES_AVAILABLE) {
                return _SM_TRAN(&SM_InputCmpsMngr_commandSuggesting);
            } else if (context == INPUT_SUGGEST_ARGUMENT_CANDIDATES) {
                return _SM_TRAN(
                    &SM_InputCmpsMngr_argumentSuggesting);
            } else if (context == INPUT_SUGGEST_AVAILABLE) {
                return _SM_TRAN(&SM_InputCmpsMngr_suggestAvailable);
            } else {
                return _SM_TRAN(&SM_InputCmpsMngr_suggestNotAvailable);
            }
        }

        case INPUT_CMPS_MNGR_CANCEL_SIG:
        case INPUT_CMPS_MNGR_SELECT_PREV_SIG:
        case INPUT_CMPS_MNGR_SELECT_NEXT_SIG:
        case INPUT_CMPS_MNGR_COMPLETE_SIG: {
            return _SM_HANDLED();
        }

        default: {
            return _SM_SUPER();
        }
    }
}

static SM_RetState SM_InputCmpsMngr_suggestAvailable_(
    SM_Hsm * const me,
    UI_Evt const * const e) SM_HSM_RETT
{
    SM_InputCmpsMngr *manager = containerof(
        me, SM_InputCmpsMngr, super);

    switch (e->sig) {
        case INPUT_CMPS_MNGR_SLASH_INPUT_SIG: {
            InputCmpsMngrEvt const * const inputEvt =
                (InputCmpsMngrEvt const *)e;
            if (SM_InputCmpsMngr_insert_(manager, &inputEvt->input)) {
                InputSuggestionContext const context =
                    SM_InputCmpsMngr_suggestionContext_(manager);

                if (context == INPUT_SUGGEST_CANDIDATES_AVAILABLE) {
                    return _SM_TRAN(&SM_InputCmpsMngr_commandSuggesting);
                } else {
                    return _SM_TRAN(
                        &SM_InputCmpsMngr_suggestNotAvailable);
                }
            } else {
                return _SM_HANDLED();
            }
        }

        default: {
            return _SM_SUPER();
        }
    }
}

static SM_RetState SM_InputCmpsMngr_suggestNotAvailable_(
    SM_Hsm * const me,
    UI_Evt const * const e) SM_HSM_RETT
{
    (void)me;
    (void)e;
    return _SM_SUPER();
}

static void SM_InputCmpsMngr_commandSuggesting_entry_(
    SM_Hsm * const me) SM_HSM_RETT
{
    SM_InputCmpsMngr *manager = containerof(
        me, SM_InputCmpsMngr, super);
    SM_InputCmpsMngr_showSuggestions_(manager);
}

static void SM_InputCmpsMngr_commandSuggesting_exit_(
    SM_Hsm * const me) SM_HSM_RETT
{
    SM_InputCmpsMngr *manager = containerof(
        me, SM_InputCmpsMngr, super);
    CommandSuggestion_hide(&manager->suggestion);
}

static SM_RetState SM_InputCmpsMngr_commandSuggesting_(
    SM_Hsm * const me,
    UI_Evt const * const e) SM_HSM_RETT
{
    SM_InputCmpsMngr *manager = containerof(
        me, SM_InputCmpsMngr, super);

    switch (e->sig) {
        case INPUT_CMPS_MNGR_TERM_INPUT_SIG:
        case INPUT_CMPS_MNGR_SLASH_INPUT_SIG: {
            InputCmpsMngrEvt const * const inputEvt =
                (InputCmpsMngrEvt const *)e;
            if (SM_InputCmpsMngr_insert_(manager, &inputEvt->input)) {
                InputSuggestionContext const context =
                    SM_InputCmpsMngr_suggestionContext_(manager);

                if (context == INPUT_SUGGEST_CANDIDATES_AVAILABLE) {
                    SM_InputCmpsMngr_showSuggestions_(manager);
                    return _SM_HANDLED();
                } else if (
                    context == INPUT_SUGGEST_ARGUMENT_CANDIDATES)
                {
                    return _SM_TRAN(
                        &SM_InputCmpsMngr_argumentSuggesting);
                } else if (context == INPUT_SUGGEST_AVAILABLE) {
                    return _SM_TRAN(&SM_InputCmpsMngr_suggestAvailable);
                } else {
                    return _SM_TRAN(
                        &SM_InputCmpsMngr_suggestNotAvailable);
                }
            } else {
                return _SM_HANDLED();
            }
        }

        case INPUT_CMPS_MNGR_SELECT_PREV_SIG: {
            manager->selectedCandidate =
                (manager->selectedCandidate == 0U)
                ? (manager->candidateCount - 1U)
                : (manager->selectedCandidate - 1U);
            SM_InputCmpsMngr_showSuggestions_(manager);
            return _SM_HANDLED();
        }

        case INPUT_CMPS_MNGR_SELECT_NEXT_SIG: {
            ++manager->selectedCandidate;
            if (manager->selectedCandidate >= manager->candidateCount) {
                manager->selectedCandidate = 0U;
                SM_InputCmpsMngr_showSuggestions_(manager);
                return _SM_HANDLED();
            } else {
                SM_InputCmpsMngr_showSuggestions_(manager);
                return _SM_HANDLED();
            }
        }

        case INPUT_CMPS_MNGR_DELETE_PREV_SIG: {
            SM_InputCmpsMngr_deletePrev_(manager);
            InputSuggestionContext const context =
                SM_InputCmpsMngr_suggestionContext_(manager);

            if (context == INPUT_SUGGEST_CANDIDATES_AVAILABLE) {
                SM_InputCmpsMngr_showSuggestions_(manager);
                return _SM_HANDLED();
            } else if (context == INPUT_SUGGEST_ARGUMENT_CANDIDATES) {
                return _SM_TRAN(
                    &SM_InputCmpsMngr_argumentSuggesting);
            } else if (context == INPUT_SUGGEST_AVAILABLE) {
                return _SM_TRAN(&SM_InputCmpsMngr_suggestAvailable);
            } else {
                return _SM_TRAN(&SM_InputCmpsMngr_suggestNotAvailable);
            }
        }

        case INPUT_CMPS_MNGR_COMPLETE_SIG:
        case INPUT_CMPS_MNGR_CONFIRM_SIG: {
            if (SM_InputCmpsMngr_acceptSuggestion_(manager)) {
                InputSuggestionContext const context =
                    SM_InputCmpsMngr_suggestionContext_(manager);

                if (context == INPUT_SUGGEST_CANDIDATES_AVAILABLE) {
                    return _SM_TRAN(
                        &SM_InputCmpsMngr_commandSuggesting);
                } else if (
                    context == INPUT_SUGGEST_ARGUMENT_CANDIDATES)
                {
                    return _SM_TRAN(
                        &SM_InputCmpsMngr_argumentSuggesting);
                } else if (context == INPUT_SUGGEST_AVAILABLE) {
                    return _SM_TRAN(
                        &SM_InputCmpsMngr_suggestAvailable);
                } else {
                    return _SM_TRAN(
                        &SM_InputCmpsMngr_suggestNotAvailable);
                }
            } else {
                return _SM_HANDLED();
            }
        }

        case INPUT_CMPS_MNGR_CANCEL_SIG: {
            manager->candidateCount = 0U;
            manager->selectedCandidate = 0U;
            bool const available =
                (manager->tokenCount < SM_INPUT_CMPS_MNGR_MAX_TOKENS)
                && ((manager->editPos == 0U)
                    || (manager->buffer[manager->editPos - 1U] == ' '));

            if (available) {
                return _SM_TRAN(&SM_InputCmpsMngr_suggestAvailable);
            } else {
                return _SM_TRAN(
                    &SM_InputCmpsMngr_suggestNotAvailable);
            }
        }

        case INPUT_CMPS_MNGR_DELETE_TO_START_SIG: {
            SM_InputCmpsMngr_deleteToStart_(manager);
            InputSuggestionContext const context =
                SM_InputCmpsMngr_suggestionContext_(manager);

            if (context == INPUT_SUGGEST_CANDIDATES_AVAILABLE) {
                SM_InputCmpsMngr_showSuggestions_(manager);
                return _SM_HANDLED();
            } else if (context == INPUT_SUGGEST_ARGUMENT_CANDIDATES) {
                return _SM_TRAN(
                    &SM_InputCmpsMngr_argumentSuggesting);
            } else if (context == INPUT_SUGGEST_AVAILABLE) {
                return _SM_TRAN(&SM_InputCmpsMngr_suggestAvailable);
            } else {
                return _SM_TRAN(&SM_InputCmpsMngr_suggestNotAvailable);
            }
        }

        case INPUT_CMPS_MNGR_DELETE_PREV_WORD_SIG: {
            SM_InputCmpsMngr_deletePrevWord_(manager);
            InputSuggestionContext const context =
                SM_InputCmpsMngr_suggestionContext_(manager);

            if (context == INPUT_SUGGEST_CANDIDATES_AVAILABLE) {
                SM_InputCmpsMngr_showSuggestions_(manager);
                return _SM_HANDLED();
            } else if (context == INPUT_SUGGEST_ARGUMENT_CANDIDATES) {
                return _SM_TRAN(
                    &SM_InputCmpsMngr_argumentSuggesting);
            } else if (context == INPUT_SUGGEST_AVAILABLE) {
                return _SM_TRAN(&SM_InputCmpsMngr_suggestAvailable);
            } else {
                return _SM_TRAN(&SM_InputCmpsMngr_suggestNotAvailable);
            }
        }

        case INPUT_CMPS_MNGR_MOVE_LEFT_SIG: {
            SM_InputCmpsMngr_moveLeft_(manager);
            InputSuggestionContext const context =
                SM_InputCmpsMngr_suggestionContext_(manager);

            if (context == INPUT_SUGGEST_CANDIDATES_AVAILABLE) {
                SM_InputCmpsMngr_showSuggestions_(manager);
                return _SM_HANDLED();
            } else if (context == INPUT_SUGGEST_ARGUMENT_CANDIDATES) {
                return _SM_TRAN(
                    &SM_InputCmpsMngr_argumentSuggesting);
            } else if (context == INPUT_SUGGEST_AVAILABLE) {
                return _SM_TRAN(&SM_InputCmpsMngr_suggestAvailable);
            } else {
                return _SM_TRAN(&SM_InputCmpsMngr_suggestNotAvailable);
            }
        }

        case INPUT_CMPS_MNGR_MOVE_RIGHT_SIG: {
            SM_InputCmpsMngr_moveRight_(manager);
            InputSuggestionContext const context =
                SM_InputCmpsMngr_suggestionContext_(manager);

            if (context == INPUT_SUGGEST_CANDIDATES_AVAILABLE) {
                SM_InputCmpsMngr_showSuggestions_(manager);
                return _SM_HANDLED();
            } else if (context == INPUT_SUGGEST_ARGUMENT_CANDIDATES) {
                return _SM_TRAN(
                    &SM_InputCmpsMngr_argumentSuggesting);
            } else if (context == INPUT_SUGGEST_AVAILABLE) {
                return _SM_TRAN(&SM_InputCmpsMngr_suggestAvailable);
            } else {
                return _SM_TRAN(&SM_InputCmpsMngr_suggestNotAvailable);
            }
        }

        case INPUT_CMPS_MNGR_MOVE_PREV_WORD_SIG: {
            SM_InputCmpsMngr_movePrevWord_(manager);
            InputSuggestionContext const context =
                SM_InputCmpsMngr_suggestionContext_(manager);

            if (context == INPUT_SUGGEST_CANDIDATES_AVAILABLE) {
                SM_InputCmpsMngr_showSuggestions_(manager);
                return _SM_HANDLED();
            } else if (context == INPUT_SUGGEST_ARGUMENT_CANDIDATES) {
                return _SM_TRAN(
                    &SM_InputCmpsMngr_argumentSuggesting);
            } else if (context == INPUT_SUGGEST_AVAILABLE) {
                return _SM_TRAN(&SM_InputCmpsMngr_suggestAvailable);
            } else {
                return _SM_TRAN(&SM_InputCmpsMngr_suggestNotAvailable);
            }
        }

        case INPUT_CMPS_MNGR_MOVE_NEXT_WORD_SIG: {
            SM_InputCmpsMngr_moveNextWord_(manager);
            InputSuggestionContext const context =
                SM_InputCmpsMngr_suggestionContext_(manager);

            if (context == INPUT_SUGGEST_CANDIDATES_AVAILABLE) {
                SM_InputCmpsMngr_showSuggestions_(manager);
                return _SM_HANDLED();
            } else if (context == INPUT_SUGGEST_ARGUMENT_CANDIDATES) {
                return _SM_TRAN(
                    &SM_InputCmpsMngr_argumentSuggesting);
            } else if (context == INPUT_SUGGEST_AVAILABLE) {
                return _SM_TRAN(&SM_InputCmpsMngr_suggestAvailable);
            } else {
                return _SM_TRAN(&SM_InputCmpsMngr_suggestNotAvailable);
            }
        }

        case INPUT_CMPS_MNGR_MOVE_HOME_SIG: {
            SM_InputCmpsMngr_moveHome_(manager);
            InputSuggestionContext const context =
                SM_InputCmpsMngr_suggestionContext_(manager);

            if (context == INPUT_SUGGEST_CANDIDATES_AVAILABLE) {
                SM_InputCmpsMngr_showSuggestions_(manager);
                return _SM_HANDLED();
            } else if (context == INPUT_SUGGEST_ARGUMENT_CANDIDATES) {
                return _SM_TRAN(
                    &SM_InputCmpsMngr_argumentSuggesting);
            } else if (context == INPUT_SUGGEST_AVAILABLE) {
                return _SM_TRAN(&SM_InputCmpsMngr_suggestAvailable);
            } else {
                return _SM_TRAN(&SM_InputCmpsMngr_suggestNotAvailable);
            }
        }

        case INPUT_CMPS_MNGR_MOVE_END_SIG: {
            SM_InputCmpsMngr_moveEnd_(manager);
            InputSuggestionContext const context =
                SM_InputCmpsMngr_suggestionContext_(manager);

            if (context == INPUT_SUGGEST_CANDIDATES_AVAILABLE) {
                SM_InputCmpsMngr_showSuggestions_(manager);
                return _SM_HANDLED();
            } else if (context == INPUT_SUGGEST_ARGUMENT_CANDIDATES) {
                return _SM_TRAN(
                    &SM_InputCmpsMngr_argumentSuggesting);
            } else if (context == INPUT_SUGGEST_AVAILABLE) {
                return _SM_TRAN(&SM_InputCmpsMngr_suggestAvailable);
            } else {
                return _SM_TRAN(&SM_InputCmpsMngr_suggestNotAvailable);
            }
        }

        default: {
            return _SM_SUPER();
        }
    }
}

static void SM_InputCmpsMngr_argumentSuggesting_entry_(
    SM_Hsm * const me) SM_HSM_RETT
{
    SM_InputCmpsMngr *manager = containerof(
        me, SM_InputCmpsMngr, super);
    SM_InputCmpsMngr_showArgumentSuggestions_(manager);
}

static void SM_InputCmpsMngr_argumentSuggesting_exit_(
    SM_Hsm * const me) SM_HSM_RETT
{
    SM_InputCmpsMngr *manager = containerof(
        me, SM_InputCmpsMngr, super);
    CommandSuggestion_hide(&manager->suggestion);
}

static SM_RetState SM_InputCmpsMngr_argumentSuggesting_(
    SM_Hsm * const me,
    UI_Evt const * const e) SM_HSM_RETT
{
    SM_InputCmpsMngr *manager = containerof(
        me, SM_InputCmpsMngr, super);

    switch (e->sig) {
        case INPUT_CMPS_MNGR_SELECT_PREV_SIG: {
            manager->selectedCandidate =
                (manager->selectedCandidate == 0U)
                ? (manager->candidateCount - 1U)
                : (manager->selectedCandidate - 1U);
            SM_InputCmpsMngr_showArgumentSuggestions_(manager);
            return _SM_HANDLED();
        }

        case INPUT_CMPS_MNGR_SELECT_NEXT_SIG: {
            ++manager->selectedCandidate;
            if (manager->selectedCandidate >= manager->candidateCount) {
                manager->selectedCandidate = 0U;
                SM_InputCmpsMngr_showArgumentSuggestions_(manager);
                return _SM_HANDLED();
            } else {
                SM_InputCmpsMngr_showArgumentSuggestions_(manager);
                return _SM_HANDLED();
            }
        }

        case INPUT_CMPS_MNGR_COMPLETE_SIG: {
            if (SM_InputCmpsMngr_acceptArgumentSuggestion_(manager)) {
                InputSuggestionContext const context =
                    SM_InputCmpsMngr_suggestionContext_(manager);

                if (context == INPUT_SUGGEST_CANDIDATES_AVAILABLE) {
                    return _SM_TRAN(
                        &SM_InputCmpsMngr_commandSuggesting);
                } else if (
                    context == INPUT_SUGGEST_ARGUMENT_CANDIDATES)
                {
                    return _SM_TRAN(
                        &SM_InputCmpsMngr_argumentSuggesting);
                } else if (context == INPUT_SUGGEST_AVAILABLE) {
                    return _SM_TRAN(
                        &SM_InputCmpsMngr_suggestAvailable);
                } else {
                    return _SM_TRAN(
                        &SM_InputCmpsMngr_suggestNotAvailable);
                }
            } else {
                return _SM_HANDLED();
            }
        }

        case INPUT_CMPS_MNGR_CANCEL_SIG: {
            manager->candidateCount = 0U;
            manager->selectedCandidate = 0U;
            return _SM_TRAN(&SM_InputCmpsMngr_suggestNotAvailable);
        }

        default: {
            return _SM_SUPER();
        }
    }
}

//============================================================================
//=== UI event contract adapter

static UI_Signal SM_InputCmpsMngr_routeSignal_(
    UI_InputEvt const * const e)
{
    switch (e->super.sig) {
        case UI_KEY_ESC_SIG: {
            return INPUT_CMPS_MNGR_CANCEL_SIG;
        }

        case UI_KEY_UP_SIG:
        case UI_KEY_CTRL_P_SIG: {
            return INPUT_CMPS_MNGR_SELECT_PREV_SIG;
        }

        case UI_KEY_DOWN_SIG:
        case UI_KEY_CTRL_N_SIG: {
            return INPUT_CMPS_MNGR_SELECT_NEXT_SIG;
        }

        case UI_KEY_BACKSPACE_SIG: {
            return INPUT_CMPS_MNGR_DELETE_PREV_SIG;
        }

        case UI_KEY_CTRL_U_SIG: {
            return INPUT_CMPS_MNGR_DELETE_TO_START_SIG;
        }

        case UI_KEY_CTRL_W_SIG: {
            return INPUT_CMPS_MNGR_DELETE_PREV_WORD_SIG;
        }

        case UI_KEY_LEFT_SIG: {
            return INPUT_CMPS_MNGR_MOVE_LEFT_SIG;
        }

        case UI_KEY_RIGHT_SIG: {
            return INPUT_CMPS_MNGR_MOVE_RIGHT_SIG;
        }

        case UI_KEY_CTRL_LEFT_SIG: {
            return INPUT_CMPS_MNGR_MOVE_PREV_WORD_SIG;
        }

        case UI_KEY_CTRL_RIGHT_SIG: {
            return INPUT_CMPS_MNGR_MOVE_NEXT_WORD_SIG;
        }

        case UI_KEY_HOME_SIG: {
            return INPUT_CMPS_MNGR_MOVE_HOME_SIG;
        }

        case UI_KEY_END_SIG: {
            return INPUT_CMPS_MNGR_MOVE_END_SIG;
        }

        case UI_KEY_TAB_SIG: {
            return INPUT_CMPS_MNGR_COMPLETE_SIG;
        }

        case UI_KEY_ENTER_SIG: {
            return INPUT_CMPS_MNGR_CONFIRM_SIG;
        }

        case UI_INPUT_SIG: {
            size_t const inputSize =
                SM_InputCmpsMngr_inputSize_(&e->input);
            if ((inputSize == 1U) && (e->input.utf8[0] == '/')) {
                return INPUT_CMPS_MNGR_SLASH_INPUT_SIG;
            } else {
                return INPUT_CMPS_MNGR_TERM_INPUT_SIG;
            }
        }

        case UI_KEY_J_SIG:
        case UI_KEY_K_SIG: {
            return INPUT_CMPS_MNGR_TERM_INPUT_SIG;
        }

        default: {
            return INPUT_CMPS_MNGR_NULL_SIG;
        }
    }
}

//============================================================================
//=== Authoritative editing model

static size_t SM_InputCmpsMngr_utf8CodepointSize_(
    char const * const text,
    size_t const remaining)
{
    unsigned char const lead = (unsigned char)text[0];
    size_t size = 0U;

    if (lead < 0x80U) {
        size = 1U;
    } else if ((lead & 0xE0U) == 0xC0U) {
        size = 2U;
    } else if ((lead & 0xF0U) == 0xE0U) {
        size = 3U;
    } else if ((lead & 0xF8U) == 0xF0U) {
        size = 4U;
    }

    if ((size == 0U) || (size > remaining)) {
        return 0U;
    }
    for (size_t i = 1U; i < size; ++i) {
        if (((unsigned char)text[i] & 0xC0U) != 0x80U) {
            return 0U;
        }
    }
    return size;
}

static size_t SM_InputCmpsMngr_inputSize_(
    UI_Input const * const input)
{
    size_t size = 0U;
    while ((size < sizeof(input->utf8)) && (input->utf8[size] != '\0')) {
        ++size;
    }
    if ((size == 0U) || (size == sizeof(input->utf8))) {
        return 0U;
    }

    size_t const codepointSize = SM_InputCmpsMngr_utf8CodepointSize_(
        input->utf8, size);
    return codepointSize == size ? size : 0U;
}

static size_t SM_InputCmpsMngr_prevPos_(
    SM_InputCmpsMngr const * const me,
    size_t const currentPos)
{
    size_t pos = currentPos;
    if (pos == 0U) {
        return 0U;
    }

    --pos;
    while ((pos > 0U)
           && (((unsigned char)me->buffer[pos] & 0xC0U) == 0x80U))
    {
        --pos;
    }
    return pos;
}

static size_t SM_InputCmpsMngr_nextPos_(
    SM_InputCmpsMngr const * const me,
    size_t const currentPos)
{
    if (currentPos >= me->length) {
        return me->length;
    }

    size_t const size = SM_InputCmpsMngr_utf8CodepointSize_(
        &me->buffer[currentPos], me->length - currentPos);
    DBC_ASSERT(500, size > 0U);
    return currentPos + size;
}

static bool SM_InputCmpsMngr_isWhitespaceAt_(
    SM_InputCmpsMngr const * const me,
    size_t const pos)
{
    unsigned char const ch = (unsigned char)me->buffer[pos];
    return ch == ' '
           || ch == '\t'
           || ch == '\n'
           || ch == '\r'
           || ch == '\v'
           || ch == '\f';
}

static bool SM_InputCmpsMngr_tokenEndingAt_(
    SM_InputCmpsMngr const * const me,
    size_t const pos,
    size_t * const tokenIndex)
{
    for (size_t i = 0U; i < me->tokenCount; ++i) {
        if (me->tokens[i].end == pos) {
            *tokenIndex = i;
            return true;
        }
    }
    return false;
}

static bool SM_InputCmpsMngr_tokenStartingAt_(
    SM_InputCmpsMngr const * const me,
    size_t const pos,
    size_t * const tokenIndex)
{
    for (size_t i = 0U; i < me->tokenCount; ++i) {
        if (me->tokens[i].begin == pos) {
            *tokenIndex = i;
            return true;
        }
    }
    return false;
}

static void SM_InputCmpsMngr_validate_(
    SM_InputCmpsMngr const * const me)
{
    DBC_INVARIANT(600, me->length < sizeof(me->buffer));
    DBC_INVARIANT(601, me->buffer[me->length] == '\0');
    DBC_INVARIANT(602, me->editPos <= me->length);
    DBC_INVARIANT(603, me->tokenCount <= SM_INPUT_CMPS_MNGR_MAX_TOKENS);
    DBC_INVARIANT(611, (me->editPos == me->length)
                       || (((unsigned char)me->buffer[me->editPos] & 0xC0U)
                           != 0x80U));

    for (size_t i = 0U; i < me->tokenCount; ++i) {
        SM_InputCmpsMngrToken const * const token = &me->tokens[i];
        DBC_INVARIANT(604, token->begin < token->end);
        DBC_INVARIANT(605, token->end <= me->length);
        DBC_INVARIANT(606, token->command < SM_INPUT_COMMAND_NUM);
        DBC_INVARIANT(607, (me->editPos <= token->begin)
                           || (me->editPos >= token->end));
        DBC_INVARIANT(612, (token->begin == me->length)
                           || (((unsigned char)me->buffer[token->begin]
                                & 0xC0U) != 0x80U));
        DBC_INVARIANT(613, (token->end == me->length)
                           || (((unsigned char)me->buffer[token->end]
                                & 0xC0U) != 0x80U));
        if (i > 0U) {
            DBC_INVARIANT(608, me->tokens[i - 1U].end <= token->begin);
        }

        char const * const expected = l_commands_[token->command].token;
        size_t const expectedLen = strlen(expected);
        DBC_INVARIANT(609, (token->end - token->begin) == expectedLen);
        DBC_INVARIANT(610, memcmp(&me->buffer[token->begin], expected,
                                  expectedLen) == 0);
    }
}

static void SM_InputCmpsMngr_setLimitReached_(
    SM_InputCmpsMngr * const me,
    bool const reached)
{
    if (me->limitReached != reached) {
        me->limitReached = reached;
        InputComposer_setLimitReached(&me->composer, reached);
    }
}

static void SM_InputCmpsMngr_highlightTokens_(
    SM_InputCmpsMngr * const me)
{
    for (size_t i = 0U; i < me->tokenCount; ++i) {
        InputComposer_highlightToken(
            &me->composer, me->buffer, me->length, me->editPos,
            me->tokens[i].begin, me->tokens[i].end);
    }
}

static bool SM_InputCmpsMngr_insertBytes_(
    SM_InputCmpsMngr * const me,
    char const * const text,
    size_t const size,
    bool const project)
{
    if (size == 0U) {
        return false;
    }
    if ((me->length + size) >= sizeof(me->buffer)) {
        SM_InputCmpsMngr_setLimitReached_(me, true);
        InputComposer_projectAll(&me->composer, me->buffer,
                                 me->length, me->editPos);
        SM_InputCmpsMngr_highlightTokens_(me);
        return false;
    }

    size_t const dirtyPos = me->editPos;
    for (size_t i = 0U; i < me->tokenCount; ++i) {
        DBC_ASSERT(520, (me->editPos <= me->tokens[i].begin)
                        || (me->editPos >= me->tokens[i].end));
        if (me->tokens[i].begin >= me->editPos) {
            me->tokens[i].begin += size;
            me->tokens[i].end += size;
        }
    }

    memmove(&me->buffer[me->editPos + size],
            &me->buffer[me->editPos],
            me->length - me->editPos + 1U);
    memcpy(&me->buffer[me->editPos], text, size);
    me->length += size;
    me->editPos += size;
    SM_InputCmpsMngr_validate_(me);

    if (project) {
        InputComposer_projectFrom(&me->composer, me->buffer,
                                  me->length, me->editPos, dirtyPos);
        SM_InputCmpsMngr_highlightTokens_(me);
    }
    return true;
}

static void SM_InputCmpsMngr_removeRange_(
    SM_InputCmpsMngr * const me,
    size_t begin,
    size_t end,
    bool const project)
{
    DBC_REQUIRE(530, begin <= end);
    DBC_REQUIRE(531, end <= me->length);
    if (begin == end) {
        return;
    }

    for (size_t i = 0U; i < me->tokenCount; ++i) {
        if ((me->tokens[i].end > begin)
            && (me->tokens[i].begin < end))
        {
            if (me->tokens[i].begin < begin) {
                begin = me->tokens[i].begin;
            }
            if (me->tokens[i].end > end) {
                end = me->tokens[i].end;
            }
        }
    }

    size_t const removed = end - begin;
    memmove(&me->buffer[begin], &me->buffer[end],
            me->length - end + 1U);
    me->length -= removed;

    if (me->editPos >= end) {
        me->editPos -= removed;
    } else if (me->editPos > begin) {
        me->editPos = begin;
    }

    size_t kept = 0U;
    for (size_t i = 0U; i < me->tokenCount; ++i) {
        SM_InputCmpsMngrToken token = me->tokens[i];
        if ((token.begin >= begin) && (token.end <= end)) {
            continue;
        }
        if (token.begin >= end) {
            token.begin -= removed;
            token.end -= removed;
        }
        me->tokens[kept] = token;
        ++kept;
    }
    me->tokenCount = kept;
    SM_InputCmpsMngr_setLimitReached_(me, false);
    SM_InputCmpsMngr_validate_(me);

    if (project) {
        InputComposer_projectFrom(&me->composer, me->buffer,
                                  me->length, me->editPos, begin);
        SM_InputCmpsMngr_highlightTokens_(me);
    }
}

static bool SM_InputCmpsMngr_insert_(
    SM_InputCmpsMngr * const me,
    UI_Input const * const input)
{
    size_t const inputSize = SM_InputCmpsMngr_inputSize_(input);
    return SM_InputCmpsMngr_insertBytes_(me, input->utf8,
                                         inputSize, true);
}

static void SM_InputCmpsMngr_deletePrev_(SM_InputCmpsMngr * const me) {
    if (me->editPos == 0U) {
        return;
    }

    size_t tokenIndex;
    if (SM_InputCmpsMngr_tokenEndingAt_(me, me->editPos, &tokenIndex)) {
        SM_InputCmpsMngr_removeRange_(me,
                                     me->tokens[tokenIndex].begin,
                                     me->tokens[tokenIndex].end,
                                     true);
        return;
    }

    size_t const prevPos = SM_InputCmpsMngr_prevPos_(me, me->editPos);
    SM_InputCmpsMngr_removeRange_(me, prevPos, me->editPos, true);
}

static void SM_InputCmpsMngr_deleteToStart_(
    SM_InputCmpsMngr * const me)
{
    SM_InputCmpsMngr_removeRange_(me, 0U, me->editPos, true);
}

static void SM_InputCmpsMngr_deletePrevWord_(
    SM_InputCmpsMngr * const me)
{
    size_t wordStart = me->editPos;

    while (wordStart > 0U) {
        size_t const prevPos = SM_InputCmpsMngr_prevPos_(me, wordStart);
        if (!SM_InputCmpsMngr_isWhitespaceAt_(me, prevPos)) {
            break;
        }
        wordStart = prevPos;
    }
    while (wordStart > 0U) {
        size_t const prevPos = SM_InputCmpsMngr_prevPos_(me, wordStart);
        if (SM_InputCmpsMngr_isWhitespaceAt_(me, prevPos)) {
            break;
        }
        wordStart = prevPos;
    }
    SM_InputCmpsMngr_removeRange_(me, wordStart, me->editPos, true);
}

static void SM_InputCmpsMngr_moveLeft_(SM_InputCmpsMngr * const me) {
    size_t nextPos;
    size_t tokenIndex;
    if (SM_InputCmpsMngr_tokenEndingAt_(me, me->editPos, &tokenIndex)) {
        nextPos = me->tokens[tokenIndex].begin;
    } else {
        nextPos = SM_InputCmpsMngr_prevPos_(me, me->editPos);
    }
    if (nextPos == me->editPos) {
        return;
    }

    me->editPos = nextPos;
    SM_InputCmpsMngr_validate_(me);
    InputComposer_moveCursor(&me->composer, me->buffer,
                             me->length, me->editPos);
    SM_InputCmpsMngr_highlightTokens_(me);
}

static void SM_InputCmpsMngr_moveRight_(SM_InputCmpsMngr * const me) {
    size_t nextPos;
    size_t tokenIndex;
    if (SM_InputCmpsMngr_tokenStartingAt_(me, me->editPos, &tokenIndex)) {
        nextPos = me->tokens[tokenIndex].end;
    } else {
        nextPos = SM_InputCmpsMngr_nextPos_(me, me->editPos);
    }
    if (nextPos == me->editPos) {
        return;
    }

    me->editPos = nextPos;
    SM_InputCmpsMngr_validate_(me);
    InputComposer_moveCursor(&me->composer, me->buffer,
                             me->length, me->editPos);
    SM_InputCmpsMngr_highlightTokens_(me);
}

static void SM_InputCmpsMngr_movePrevWord_(
    SM_InputCmpsMngr * const me)
{
    size_t nextPos = me->editPos;

    while (nextPos > 0U) {
        size_t const prevPos = SM_InputCmpsMngr_prevPos_(me, nextPos);
        if (!SM_InputCmpsMngr_isWhitespaceAt_(me, prevPos)) {
            break;
        }
        nextPos = prevPos;
    }

    size_t tokenIndex;
    if (SM_InputCmpsMngr_tokenEndingAt_(me, nextPos, &tokenIndex)) {
        nextPos = me->tokens[tokenIndex].begin;
    } else {
        while (nextPos > 0U) {
            size_t const prevPos =
                SM_InputCmpsMngr_prevPos_(me, nextPos);
            if (SM_InputCmpsMngr_isWhitespaceAt_(me, prevPos)) {
                break;
            }
            nextPos = prevPos;
        }
    }

    if (nextPos == me->editPos) {
        return;
    }

    me->editPos = nextPos;
    SM_InputCmpsMngr_validate_(me);
    InputComposer_moveCursor(&me->composer, me->buffer,
                             me->length, me->editPos);
    SM_InputCmpsMngr_highlightTokens_(me);
}

static void SM_InputCmpsMngr_moveNextWord_(
    SM_InputCmpsMngr * const me)
{
    size_t nextPos = me->editPos;
    size_t tokenIndex;

    if (SM_InputCmpsMngr_tokenStartingAt_(me, nextPos, &tokenIndex)) {
        nextPos = me->tokens[tokenIndex].end;
    } else {
        while ((nextPos < me->length)
               && !SM_InputCmpsMngr_isWhitespaceAt_(me, nextPos))
        {
            nextPos = SM_InputCmpsMngr_nextPos_(me, nextPos);
        }
    }

    while ((nextPos < me->length)
           && SM_InputCmpsMngr_isWhitespaceAt_(me, nextPos))
    {
        nextPos = SM_InputCmpsMngr_nextPos_(me, nextPos);
    }

    if (nextPos == me->editPos) {
        return;
    }

    me->editPos = nextPos;
    SM_InputCmpsMngr_validate_(me);
    InputComposer_moveCursor(&me->composer, me->buffer,
                             me->length, me->editPos);
    SM_InputCmpsMngr_highlightTokens_(me);
}

static void SM_InputCmpsMngr_moveHome_(SM_InputCmpsMngr * const me) {
    if (me->editPos == 0U) {
        return;
    }

    me->editPos = 0U;
    SM_InputCmpsMngr_validate_(me);
    InputComposer_moveCursor(&me->composer, me->buffer,
                             me->length, me->editPos);
    SM_InputCmpsMngr_highlightTokens_(me);
}

static void SM_InputCmpsMngr_moveEnd_(SM_InputCmpsMngr * const me) {
    if (me->editPos == me->length) {
        return;
    }

    me->editPos = me->length;
    SM_InputCmpsMngr_validate_(me);
    InputComposer_moveCursor(&me->composer, me->buffer,
                             me->length, me->editPos);
    SM_InputCmpsMngr_highlightTokens_(me);
}

//============================================================================
//=== Command suggestion and Token model

static void SM_InputCmpsMngr_filterCandidates_(
    SM_InputCmpsMngr * const me)
{
    size_t const prefixBegin = me->suggestionStart + 1U;
    DBC_ASSERT(560, prefixBegin <= me->editPos);
    DBC_ASSERT(561, me->editPos <= me->suggestionEnd);
    size_t const prefixLen = me->suggestionEnd - prefixBegin;

    me->candidateCount = 0U;
    me->selectedCandidate = 0U;
    for (size_t i = 0U; i < SM_INPUT_COMMAND_NUM; ++i) {
        size_t const nameLen = strlen(l_commands_[i].name);
        if ((prefixLen <= nameLen)
            && (memcmp(l_commands_[i].name,
                       &me->buffer[prefixBegin], prefixLen) == 0))
        {
            me->candidates[me->candidateCount] = l_commands_[i].id;
            ++me->candidateCount;
        }
    }
}

static bool SM_InputCmpsMngr_argumentMatches_(
    char const * const candidate,
    char const * const prefix,
    size_t const prefixLen)
{
    size_t const candidateLen = strlen(candidate);
    return (prefixLen <= candidateLen)
           && (memcmp(candidate, prefix, prefixLen) == 0);
}

static void SM_InputCmpsMngr_addArgumentCandidate_(
    SM_InputCmpsMngr * const me,
    char const * const candidate,
    char const * const prefix,
    size_t const prefixLen)
{
    if ((me->candidateCount
         < SM_INPUT_CMPS_MNGR_MAX_ARG_CANDIDATES)
        && SM_InputCmpsMngr_argumentMatches_(candidate, prefix,
                                             prefixLen))
    {
        me->argumentCandidates[me->candidateCount] = candidate;
        ++me->candidateCount;
    }
}

static void SM_InputCmpsMngr_filterArgumentCandidates_(
    SM_InputCmpsMngr * const me,
    SM_InputCommand const command,
    size_t const prefixBegin,
    size_t const prefixEnd)
{
    DBC_REQUIRE(562, prefixBegin <= prefixEnd);
    DBC_REQUIRE(563, prefixEnd <= me->length);

    me->candidateCount = 0U;
    me->selectedCandidate = 0U;
    char const * const prefix = &me->buffer[prefixBegin];
    size_t const prefixLen = prefixEnd - prefixBegin;

    if ((command == SM_INPUT_COMMAND_CONNECT)
        || (command == SM_INPUT_COMMAND_LOAD_PROTOCOL))
    {
        char const * const catalog =
            command == SM_INPUT_COMMAND_CONNECT
            ? me->portCatalog : me->protocolCatalog;
        size_t const catalogSize =
            command == SM_INPUT_COMMAND_CONNECT
            ? me->portCatalogSize : me->protocolCatalogSize;
        size_t offset = 0U;
        while ((catalog != (char const *)0)
               && (offset + 1U < catalogSize)
               && (catalog[offset] != '\0'))
        {
            char const * const value = &catalog[offset];
            size_t const remaining = catalogSize - offset;
            char const * const end =
                (char const *)memchr(value, '\0', remaining);
            DBC_ASSERT(564, end != (char const *)0);
            SM_InputCmpsMngr_addArgumentCandidate_(
                me, value, prefix, prefixLen);
            offset += (size_t)(end - value) + 1U;
        }
        return;
    }

    char const * const *values = (char const * const *)0;
    size_t valueCount = 0U;
    switch (command) {
        case SM_INPUT_COMMAND_BAUDRATE: {
            values = l_baudrateArgs_;
            valueCount = sizeof(l_baudrateArgs_)
                         / sizeof(l_baudrateArgs_[0]);
            break;
        }
        case SM_INPUT_COMMAND_DATA_BITS: {
            values = l_dataBitsArgs_;
            valueCount = sizeof(l_dataBitsArgs_)
                         / sizeof(l_dataBitsArgs_[0]);
            break;
        }
        case SM_INPUT_COMMAND_STOP_BITS: {
            values = l_stopBitsArgs_;
            valueCount = sizeof(l_stopBitsArgs_)
                         / sizeof(l_stopBitsArgs_[0]);
            break;
        }
        case SM_INPUT_COMMAND_PARITY: {
            values = l_parityArgs_;
            valueCount = sizeof(l_parityArgs_)
                         / sizeof(l_parityArgs_[0]);
            break;
        }
        case SM_INPUT_COMMAND_FLOW_CONTROL: {
            values = l_flowControlArgs_;
            valueCount = sizeof(l_flowControlArgs_)
                         / sizeof(l_flowControlArgs_[0]);
            break;
        }
        default: {
            break;
        }
    }

    for (size_t i = 0U; i < valueCount; ++i) {
        SM_InputCmpsMngr_addArgumentCandidate_(
            me, values[i], prefix, prefixLen);
    }
}

static InputSuggestionContext SM_InputCmpsMngr_suggestionContext_(
    SM_InputCmpsMngr * const me)
{
    me->candidateCount = 0U;
    me->selectedCandidate = 0U;
    me->suggestionStart = me->editPos;
    me->suggestionEnd = me->editPos;

    if (me->tokenCount < SM_INPUT_CMPS_MNGR_MAX_TOKENS) {
        size_t start = me->editPos;
        while ((start > 0U)
               && !SM_InputCmpsMngr_isWhitespaceAt_(me, start - 1U))
        {
            --start;
        }

        if ((start < me->editPos) && (me->buffer[start] == '/')) {
            size_t end = me->editPos;
            while ((end < me->length)
                   && !SM_InputCmpsMngr_isWhitespaceAt_(me, end))
            {
                ++end;
            }

            me->suggestionStart = start;
            me->suggestionEnd = end;
            SM_InputCmpsMngr_filterCandidates_(me);
            if (me->candidateCount > 0U) {
                return INPUT_SUGGEST_CANDIDATES_AVAILABLE;
            }
        }
    }

    for (size_t i = 0U; i < me->tokenCount; ++i) {
        SM_InputCmpsMngrToken const * const token = &me->tokens[i];
        size_t const regionEnd = (i + 1U < me->tokenCount)
            ? me->tokens[i + 1U].begin : me->length;
        bool const beforeNextToken = (i + 1U == me->tokenCount)
            || (me->editPos < regionEnd);

        if ((token->end >= me->editPos)
            || (me->editPos > regionEnd)
            || !beforeNextToken)
        {
            continue;
        }

        size_t wordStart = me->editPos;
        while ((wordStart > token->end)
               && !SM_InputCmpsMngr_isWhitespaceAt_(
                       me, wordStart - 1U))
        {
            --wordStart;
        }
        if ((wordStart <= token->end)
            || !SM_InputCmpsMngr_isWhitespaceRange_(
                    me, token->end, wordStart))
        {
            continue;
        }

        size_t wordEnd = me->editPos;
        while ((wordEnd < regionEnd)
               && !SM_InputCmpsMngr_isWhitespaceAt_(me, wordEnd))
        {
            ++wordEnd;
        }
        if (!SM_InputCmpsMngr_isWhitespaceRange_(
                me, wordEnd, regionEnd))
        {
            continue;
        }

        me->suggestionStart = wordStart;
        me->suggestionEnd = wordEnd;
        SM_InputCmpsMngr_filterArgumentCandidates_(
            me, token->command, wordStart, wordEnd);
        return me->candidateCount > 0U
               ? INPUT_SUGGEST_ARGUMENT_CANDIDATES
               : INPUT_SUGGEST_NOT_AVAILABLE;
    }

    if (me->tokenCount < SM_INPUT_CMPS_MNGR_MAX_TOKENS) {
        if ((me->editPos == 0U)
            || SM_InputCmpsMngr_isWhitespaceAt_(me,
                                                me->editPos - 1U))
        {
            return INPUT_SUGGEST_AVAILABLE;
        } else {
            return INPUT_SUGGEST_NOT_AVAILABLE;
        }
    }
    return INPUT_SUGGEST_NOT_AVAILABLE;
}

static void SM_InputCmpsMngr_showSuggestions_(
    SM_InputCmpsMngr * const me)
{
    DBC_REQUIRE(570, me->candidateCount > 0U);
    DBC_REQUIRE(571, me->selectedCandidate < me->candidateCount);

    char const *items[SM_INPUT_COMMAND_NUM];
    for (size_t i = 0U; i < me->candidateCount; ++i) {
        SM_InputCommand const command = me->candidates[i];
        DBC_ASSERT(572, command < SM_INPUT_COMMAND_NUM);
        items[i] = l_commands_[command].suggestion;
    }
    CommandSuggestion_show(&me->suggestion, me->inputY, me->cols,
                           items, me->candidateCount,
                           me->selectedCandidate);
}

static void SM_InputCmpsMngr_showArgumentSuggestions_(
    SM_InputCmpsMngr * const me)
{
    DBC_REQUIRE(573, me->candidateCount > 0U);
    DBC_REQUIRE(574, me->selectedCandidate < me->candidateCount);

    CommandSuggestion_show(
        &me->suggestion, me->inputY, me->cols,
        me->argumentCandidates, me->candidateCount,
        me->selectedCandidate);
}

static bool SM_InputCmpsMngr_acceptSuggestion_(
    SM_InputCmpsMngr * const me)
{
    DBC_REQUIRE(580, me->candidateCount > 0U);
    DBC_REQUIRE(581, me->selectedCandidate < me->candidateCount);

    SM_InputCommand const command =
        me->candidates[me->selectedCandidate];
    DBC_ASSERT(582, command < SM_INPUT_COMMAND_NUM);
    char const * const tokenText = l_commands_[command].token;
    size_t const tokenLen = strlen(tokenText);
    size_t const rawLen = me->suggestionEnd - me->suggestionStart;
    size_t const replacementLen = tokenLen + 1U;

    if (me->tokenCount >= SM_INPUT_CMPS_MNGR_MAX_TOKENS) {
        return false;
    }
    if ((me->length - rawLen + replacementLen) >= sizeof(me->buffer)) {
        SM_InputCmpsMngr_setLimitReached_(me, true);
        InputComposer_projectAll(&me->composer, me->buffer,
                                 me->length, me->editPos);
        SM_InputCmpsMngr_highlightTokens_(me);
        return false;
    }

    size_t const tokenBegin = me->suggestionStart;
    SM_InputCmpsMngr_removeRange_(me, tokenBegin,
                                 me->suggestionEnd, false);
    bool const inserted = SM_InputCmpsMngr_insertBytes_(
        me, tokenText, tokenLen, false);
    DBC_ASSERT(583, inserted);

    size_t insertAt = me->tokenCount;
    while ((insertAt > 0U)
           && (me->tokens[insertAt - 1U].begin > tokenBegin))
    {
        me->tokens[insertAt] = me->tokens[insertAt - 1U];
        --insertAt;
    }
    me->tokens[insertAt].begin = tokenBegin;
    me->tokens[insertAt].end = tokenBegin + tokenLen;
    me->tokens[insertAt].command = command;
    ++me->tokenCount;

    bool const spaceInserted = SM_InputCmpsMngr_insertBytes_(
        me, " ", 1U, false);
    DBC_ASSERT(584, spaceInserted);
    (void)spaceInserted;
    SM_InputCmpsMngr_validate_(me);

    InputComposer_projectAll(&me->composer, me->buffer,
                             me->length, me->editPos);
    SM_InputCmpsMngr_highlightTokens_(me);
    return true;
}

static bool SM_InputCmpsMngr_acceptArgumentSuggestion_(
    SM_InputCmpsMngr * const me)
{
    DBC_REQUIRE(585, me->candidateCount > 0U);
    DBC_REQUIRE(586, me->selectedCandidate < me->candidateCount);

    char const * const value =
        me->argumentCandidates[me->selectedCandidate];
    DBC_ASSERT(587, value != (char const *)0);
    size_t const valueLen = strlen(value);
    size_t const rawLen = me->suggestionEnd - me->suggestionStart;
    bool const hasSeparator = (me->suggestionEnd < me->length)
        && SM_InputCmpsMngr_isWhitespaceAt_(me, me->suggestionEnd);
    size_t const replacementLen = valueLen + (hasSeparator ? 0U : 1U);

    if ((me->length - rawLen + replacementLen) >= sizeof(me->buffer)) {
        SM_InputCmpsMngr_setLimitReached_(me, true);
        InputComposer_projectAll(&me->composer, me->buffer,
                                 me->length, me->editPos);
        SM_InputCmpsMngr_highlightTokens_(me);
        return false;
    }

    SM_InputCmpsMngr_removeRange_(me, me->suggestionStart,
                                 me->suggestionEnd, false);
    bool const inserted = SM_InputCmpsMngr_insertBytes_(
        me, value, valueLen, false);
    DBC_ASSERT(588, inserted);

    if ((me->editPos < me->length)
        && SM_InputCmpsMngr_isWhitespaceAt_(me, me->editPos))
    {
        me->editPos = SM_InputCmpsMngr_nextPos_(me, me->editPos);
    } else {
        bool const spaceInserted = SM_InputCmpsMngr_insertBytes_(
            me, " ", 1U, false);
        DBC_ASSERT(589, spaceInserted);
        (void)spaceInserted;
    }

    SM_InputCmpsMngr_validate_(me);
    InputComposer_projectAll(&me->composer, me->buffer,
                             me->length, me->editPos);
    SM_InputCmpsMngr_highlightTokens_(me);
    return true;
}

//============================================================================
//=== Command submission

static bool SM_InputCmpsMngr_isWhitespaceRange_(
    SM_InputCmpsMngr const * const me,
    size_t const begin,
    size_t const end)
{
    for (size_t i = begin; i < end; ++i) {
        if (!SM_InputCmpsMngr_isWhitespaceAt_(me, i)) {
            return false;
        }
    }
    return true;
}

static bool SM_InputCmpsMngr_parseArg_(
    SM_InputCmpsMngr const * const me,
    size_t begin,
    size_t end,
    SM_InputCmpsMngrArg * const arg)
{
    while ((begin < end) && SM_InputCmpsMngr_isWhitespaceAt_(me, begin)) {
        ++begin;
    }
    while ((end > begin)
           && SM_InputCmpsMngr_isWhitespaceAt_(me, end - 1U))
    {
        --end;
    }

    arg->text = (char const *)0;
    arg->len = 0U;
    arg->present = false;
    if (begin == end) {
        return true;
    }

    size_t wordEnd = begin;
    while ((wordEnd < end)
           && !SM_InputCmpsMngr_isWhitespaceAt_(me, wordEnd))
    {
        ++wordEnd;
    }
    if (!SM_InputCmpsMngr_isWhitespaceRange_(me, wordEnd, end)) {
        return false;
    }

    arg->text = &me->buffer[begin];
    arg->len = wordEnd - begin;
    arg->present = true;
    return true;
}

static bool SM_InputCmpsMngr_buildSubmission_(
    SM_InputCmpsMngr const * const me,
    SM_InputCmpsMngrSubmission * const submission,
    char const ** const rejectionReason)
{
    bool seen[SM_INPUT_COMMAND_NUM] = {false};
    bool hasConfig = false;
    bool hasConnect = false;
    bool hasDisconnect = false;
    bool hasRefresh = false;
    bool hasRefreshProtocols = false;
    bool hasLoadProtocol = false;

    memset(submission, 0, sizeof(*submission));
    *rejectionReason = "unknown command submission";
    if (me->tokenCount == 0U) {
        *rejectionReason = "no accepted command token";
        return false;
    }
    if (!SM_InputCmpsMngr_isWhitespaceRange_(
            me, 0U, me->tokens[0].begin))
    {
        *rejectionReason = "text appears before the first command";
        return false;
    }

    for (size_t i = 0U; i < me->tokenCount; ++i) {
        SM_InputCmpsMngrToken const * const token = &me->tokens[i];
        size_t const argEnd = (i + 1U < me->tokenCount)
            ? me->tokens[i + 1U].begin
            : me->length;
        SM_InputCmpsMngrArg arg;

        if ((i > 0U)
            && ((token->begin == 0U)
                || !SM_InputCmpsMngr_isWhitespaceAt_(
                        me, token->begin - 1U)))
        {
            *rejectionReason =
                "command tokens must be separated by whitespace";
            return false;
        }
        if ((token->end < argEnd)
            && !SM_InputCmpsMngr_isWhitespaceAt_(me, token->end))
        {
            *rejectionReason =
                "command tokens must be separated by whitespace";
            return false;
        }
        if (seen[token->command]) {
            *rejectionReason = "duplicate command";
            return false;
        }
        if (!SM_InputCmpsMngr_parseArg_(
                me, token->end, argEnd, &arg))
        {
            *rejectionReason =
                "each command accepts at most one argument";
            return false;
        }
        seen[token->command] = true;

        switch (token->command) {
            case SM_INPUT_COMMAND_BAUDRATE: {
                if (!arg.present) {
                    *rejectionReason = "command requires an argument";
                    return false;
                }
                submission->baudrate = arg;
                hasConfig = true;
                break;
            }
            case SM_INPUT_COMMAND_CONNECT: {
                submission->port = arg;
                hasConnect = true;
                break;
            }
            case SM_INPUT_COMMAND_DATA_BITS: {
                if (!arg.present) {
                    *rejectionReason = "command requires an argument";
                    return false;
                }
                submission->dataBits = arg;
                hasConfig = true;
                break;
            }
            case SM_INPUT_COMMAND_DISCONNECT: {
                if (arg.present) {
                    *rejectionReason =
                        "command does not accept an argument";
                    return false;
                }
                hasDisconnect = true;
                break;
            }
            case SM_INPUT_COMMAND_FLOW_CONTROL: {
                if (!arg.present) {
                    *rejectionReason = "command requires an argument";
                    return false;
                }
                submission->flowControl = arg;
                hasConfig = true;
                break;
            }
            case SM_INPUT_COMMAND_LOAD_PROTOCOL: {
                if (!arg.present) {
                    *rejectionReason = "command requires an argument";
                    return false;
                }
                submission->protocolPath = arg;
                hasLoadProtocol = true;
                break;
            }
            case SM_INPUT_COMMAND_PARITY: {
                if (!arg.present) {
                    *rejectionReason = "command requires an argument";
                    return false;
                }
                submission->parity = arg;
                hasConfig = true;
                break;
            }
            case SM_INPUT_COMMAND_REFRESH: {
                if (arg.present) {
                    *rejectionReason =
                        "command does not accept an argument";
                    return false;
                }
                hasRefresh = true;
                break;
            }
            case SM_INPUT_COMMAND_REFRESH_PROTOCOLS: {
                if (arg.present) {
                    *rejectionReason =
                        "command does not accept an argument";
                    return false;
                }
                hasRefreshProtocols = true;
                break;
            }
            case SM_INPUT_COMMAND_STOP_BITS: {
                if (!arg.present) {
                    *rejectionReason = "command requires an argument";
                    return false;
                }
                submission->stopBits = arg;
                hasConfig = true;
                break;
            }
            default: {
                *rejectionReason = "unsupported command";
                return false;
            }
        }
    }

    unsigned const actionCount = (hasConnect ? 1U : 0U)
                               + (hasDisconnect ? 1U : 0U)
                               + (hasRefresh ? 1U : 0U)
                               + (hasRefreshProtocols ? 1U : 0U);
    if (actionCount > 1U) {
        *rejectionReason = "lifecycle commands cannot be combined";
        return false;
    }
    if ((hasDisconnect || hasRefresh || hasRefreshProtocols)
        && (hasConfig || hasLoadProtocol))
    {
        *rejectionReason =
            "disconnect and refresh commands must be used alone";
        return false;
    }

    if (hasConnect) {
        submission->action = SM_INPUT_ACTION_CONNECT;
    } else if (hasDisconnect) {
        submission->action = SM_INPUT_ACTION_DISCONNECT;
    } else if (hasRefresh) {
        submission->action = SM_INPUT_ACTION_REFRESH;
    } else if (hasRefreshProtocols) {
        submission->action = SM_INPUT_ACTION_REFRESH_PROTOCOLS;
    } else if (hasConfig) {
        submission->action = SM_INPUT_ACTION_CONFIG;
    } else if (hasLoadProtocol) {
        submission->action = SM_INPUT_ACTION_LOAD_PROTOCOL;
    } else {
        *rejectionReason = "no executable command";
        return false;
    }
    return true;
}

static void SM_InputCmpsMngr_clear_(SM_InputCmpsMngr * const me) {
    me->buffer[0] = '\0';
    me->length = 0U;
    me->editPos = 0U;
    me->tokenCount = 0U;
    me->candidateCount = 0U;
    me->selectedCandidate = 0U;
    me->suggestionStart = 0U;
    me->suggestionEnd = 0U;
    SM_InputCmpsMngr_setLimitReached_(me, false);
    SM_InputCmpsMngr_validate_(me);
    InputComposer_projectAll(&me->composer, me->buffer,
                             me->length, me->editPos);
    SM_InputCmpsMngr_highlightTokens_(me);
}

//============================================================================
//=== Constructor / lifecycle

void SM_InputCmpsMngr_ctor(SM_InputCmpsMngr * const me) {
    DBC_REQUIRE(100, me != (SM_InputCmpsMngr *)0);

    *me = (SM_InputCmpsMngr){0};
    InputComposer_init(&me->composer);
    CommandSuggestion_init(&me->suggestion);
    SM_InputCmpsMngr_validate_(me);
}

void SM_InputCmpsMngr_init(SM_InputCmpsMngr * const me) {
    DBC_REQUIRE(200, me != (SM_InputCmpsMngr *)0);
    SM_Hsm_init_(&me->super,
                 (SM_InitHandler)SM_InputCmpsMngr_TOP_initial);
}

void SM_InputCmpsMngr_setCommandSink(
    SM_InputCmpsMngr * const me,
    SM_InputCmpsMngr_CommandSink const * const commandSink)
{
    DBC_REQUIRE(440, me != (SM_InputCmpsMngr *)0);
    DBC_REQUIRE(441,
        commandSink != (SM_InputCmpsMngr_CommandSink const *)0);
    DBC_REQUIRE(442,
        commandSink->submit
            != (void (*)(
                void *, SM_InputCmpsMngrSubmission const *))0);
    DBC_REQUIRE(451,
        commandSink->reject != (void (*)(void *, char const *))0);
    me->commandSink = *commandSink;
}

void SM_InputCmpsMngr_setPortCatalog(
    SM_InputCmpsMngr * const me,
    char const * const portCatalog,
    size_t const portCatalogSize)
{
    DBC_REQUIRE(443, me != (SM_InputCmpsMngr *)0);
    DBC_REQUIRE(444, ((portCatalog == (char const *)0)
                      && (portCatalogSize == 0U))
                     || ((portCatalog != (char const *)0)
                         && (portCatalogSize >= 2U)));
    if (portCatalog != (char const *)0) {
        DBC_REQUIRE(445, portCatalog[portCatalogSize - 1U] == '\0');
        DBC_REQUIRE(446, portCatalog[portCatalogSize - 2U] == '\0');
    }

    me->portCatalog = portCatalog;
    me->portCatalogSize = portCatalogSize;
    if (me->super.curr != (SM_StatePtr)0) {
        UI_Evt const event = {
            .sig = INPUT_CMPS_MNGR_ARGUMENT_CATALOG_UPDATED_SIG,
        };
        SM_Hsm_dispatch_(&me->super, &event);
    }
}

void SM_InputCmpsMngr_setProtocolCatalog(
    SM_InputCmpsMngr * const me,
    char const * const protocolCatalog,
    size_t const protocolCatalogSize)
{
    DBC_REQUIRE(447, me != (SM_InputCmpsMngr *)0);
    DBC_REQUIRE(448, ((protocolCatalog == (char const *)0)
                      && (protocolCatalogSize == 0U))
                     || ((protocolCatalog != (char const *)0)
                         && (protocolCatalogSize >= 2U)));
    if (protocolCatalog != (char const *)0) {
        DBC_REQUIRE(449,
            protocolCatalog[protocolCatalogSize - 1U] == '\0');
        DBC_REQUIRE(450,
            protocolCatalog[protocolCatalogSize - 2U] == '\0');
    }

    me->protocolCatalog = protocolCatalog;
    me->protocolCatalogSize = protocolCatalogSize;
    if (me->super.curr != (SM_StatePtr)0) {
        UI_Evt const event = {
            .sig = INPUT_CMPS_MNGR_ARGUMENT_CATALOG_UPDATED_SIG,
        };
        SM_Hsm_dispatch_(&me->super, &event);
    }
}

void SM_InputCmpsMngr_dispatchEvt(SM_InputCmpsMngr * const me,
                                  UI_InputEvt const * const e)
{
    DBC_REQUIRE(300, me != (SM_InputCmpsMngr *)0);
    DBC_REQUIRE(301, e != (UI_InputEvt const *)0);

    UI_Signal const sig = SM_InputCmpsMngr_routeSignal_(e);
    DBC_REQUIRE(302, sig != INPUT_CMPS_MNGR_NULL_SIG);

    InputCmpsMngrEvt const managerEvt = {
        .super.sig = sig,
        .input = e->input,
    };
    SM_Hsm_dispatch_(&me->super, &managerEvt.super);
}

void SM_InputCmpsMngr_create(
    SM_InputCmpsMngr * const me,
    struct ncplane * const parent,
    void * const owner,
    int const y,
    unsigned const cols,
    InputComposer_ResizeCb const resizeCb)
{
    DBC_REQUIRE(400, me != (SM_InputCmpsMngr *)0);
    me->inputY = y;
    me->cols = cols;
    InputComposer_create(&me->composer, parent, owner, y, cols, resizeCb);
    CommandSuggestion_create(&me->suggestion, parent);
    InputComposer_projectAll(&me->composer, me->buffer,
                             me->length, me->editPos);
    SM_InputCmpsMngr_highlightTokens_(me);
}

void SM_InputCmpsMngr_destroy(SM_InputCmpsMngr * const me) {
    DBC_REQUIRE(410, me != (SM_InputCmpsMngr *)0);
    CommandSuggestion_destroy(&me->suggestion);
    InputComposer_destroy(&me->composer);
}

void SM_InputCmpsMngr_resize(SM_InputCmpsMngr * const me,
                             int const y,
                             unsigned const rows,
                             unsigned const cols)
{
    DBC_REQUIRE(420, me != (SM_InputCmpsMngr *)0);
    me->inputY = y;
    me->cols = cols;
    InputComposer_resize(&me->composer, y, rows, cols);
    InputComposer_projectAll(&me->composer, me->buffer,
                             me->length, me->editPos);
    SM_InputCmpsMngr_highlightTokens_(me);
    if (me->super.curr == &SM_InputCmpsMngr_commandSuggesting) {
        SM_InputCmpsMngr_showSuggestions_(me);
    } else if (me->super.curr
               == &SM_InputCmpsMngr_argumentSuggesting)
    {
        SM_InputCmpsMngr_showArgumentSuggestions_(me);
    }
}

unsigned SM_InputCmpsMngr_preferredRows(
    SM_InputCmpsMngr const * const me,
    unsigned const cols)
{
    DBC_REQUIRE(421, me != (SM_InputCmpsMngr const *)0);
    return InputComposer_preferredRows(
        &me->composer,
        me->buffer, me->length, me->editPos, cols);
}

void SM_InputCmpsMngr_setActive(SM_InputCmpsMngr * const me,
                                bool const active)
{
    DBC_REQUIRE(430, me != (SM_InputCmpsMngr *)0);
    UI_Evt const e = {
        .sig = active ? INPUT_CMPS_MNGR_ACTIVATE_SIG
                      : INPUT_CMPS_MNGR_DEACTIVATE_SIG,
    };
    SM_Hsm_dispatch_(&me->super, &e);
}
