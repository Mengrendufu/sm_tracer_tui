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
//=== Private event contract

enum InputCmpsMngrSignals {
    INPUT_CMPS_MNGR_NULL_SIG = 0,
    INPUT_CMPS_MNGR_TERM_INPUT_SIG,
    INPUT_CMPS_MNGR_DELETE_PREV_SIG,
    INPUT_CMPS_MNGR_DELETE_TO_START_SIG,
    INPUT_CMPS_MNGR_DELETE_PREV_WORD_SIG,
    INPUT_CMPS_MNGR_MOVE_LEFT_SIG,
    INPUT_CMPS_MNGR_MOVE_RIGHT_SIG,
    INPUT_CMPS_MNGR_MOVE_HOME_SIG,
    INPUT_CMPS_MNGR_MOVE_END_SIG,
    INPUT_CMPS_MNGR_CONFIRM_SIG,
};

typedef struct {
    UI_Evt super;
    UI_Input input;
} InputCmpsMngrEvt;

static UI_Signal SM_InputCmpsMngr_routeSignal_(UI_Signal sig);
static size_t    SM_InputCmpsMngr_utf8CodepointSize_(
                        char const *text, size_t remaining);
static size_t    SM_InputCmpsMngr_inputSize_(UI_Input const *input);
static size_t    SM_InputCmpsMngr_prevPos_(
                        SM_InputCmpsMngr const *me, size_t pos);
static bool      SM_InputCmpsMngr_isWhitespaceAt_(
                        SM_InputCmpsMngr const *me, size_t pos);
static size_t    SM_InputCmpsMngr_nextPos_(
                        SM_InputCmpsMngr const *me);
static void      SM_InputCmpsMngr_insert_(
                        SM_InputCmpsMngr *me, UI_Input const *input);
static void      SM_InputCmpsMngr_deletePrev_(SM_InputCmpsMngr *me);
static void      SM_InputCmpsMngr_deleteToStart_(SM_InputCmpsMngr *me);
static void      SM_InputCmpsMngr_deletePrevWord_(SM_InputCmpsMngr *me);
static void      SM_InputCmpsMngr_moveLeft_(SM_InputCmpsMngr *me);
static void      SM_InputCmpsMngr_moveRight_(SM_InputCmpsMngr *me);
static void      SM_InputCmpsMngr_moveHome_(SM_InputCmpsMngr *me);
static void      SM_InputCmpsMngr_moveEnd_(SM_InputCmpsMngr *me);

//============================================================================
//=== States

static SM_StatePtr SM_InputCmpsMngr_TOP_initial(SM_Hsm *me) SM_HSM_RETT;

static void        SM_InputCmpsMngr_idle_entry_(SM_Hsm *me) SM_HSM_RETT;
static SM_RetState SM_InputCmpsMngr_idle_(SM_Hsm * const me, UI_Evt const * const e) SM_HSM_RETT;
SM_HsmState SM_HSM_ROM SM_InputCmpsMngr_idle = {
    (SM_StatePtr)0,                                  // super (top)
    (SM_InitHandler)0,                               // init_ (leaf)
    (SM_ActionHandler)&SM_InputCmpsMngr_idle_entry_, // entry_
    (SM_ActionHandler)0,                             // exit_
    (SM_StateHandler)&SM_InputCmpsMngr_idle_         // handler_
};

//============================================================================
//=== HSM implementations

static SM_StatePtr SM_InputCmpsMngr_TOP_initial(SM_Hsm * const me) SM_HSM_RETT {
    (void)me;
    return _SM_INIT(&SM_InputCmpsMngr_idle);
}

static void SM_InputCmpsMngr_idle_entry_(SM_Hsm * const me) SM_HSM_RETT {
    (void)me;
}

static SM_RetState SM_InputCmpsMngr_idle_(SM_Hsm * const me, UI_Evt const * const e) SM_HSM_RETT {
    SM_InputCmpsMngr *manager = containerof(me, SM_InputCmpsMngr, super);

    switch (e->sig) {
        case INPUT_CMPS_MNGR_CONFIRM_SIG: {
            return _SM_HANDLED();
        }

        case INPUT_CMPS_MNGR_TERM_INPUT_SIG: {
            InputCmpsMngrEvt const * const inputEvt =
                (InputCmpsMngrEvt const *)e;
            SM_InputCmpsMngr_insert_(manager, &inputEvt->input);
            return _SM_HANDLED();
        }

        case INPUT_CMPS_MNGR_DELETE_PREV_SIG: {
            SM_InputCmpsMngr_deletePrev_(manager);
            return _SM_HANDLED();
        }

        case INPUT_CMPS_MNGR_DELETE_TO_START_SIG: {
            SM_InputCmpsMngr_deleteToStart_(manager);
            return _SM_HANDLED();
        }

        case INPUT_CMPS_MNGR_DELETE_PREV_WORD_SIG: {
            SM_InputCmpsMngr_deletePrevWord_(manager);
            return _SM_HANDLED();
        }

        case INPUT_CMPS_MNGR_MOVE_LEFT_SIG: {
            SM_InputCmpsMngr_moveLeft_(manager);
            return _SM_HANDLED();
        }

        case INPUT_CMPS_MNGR_MOVE_RIGHT_SIG: {
            SM_InputCmpsMngr_moveRight_(manager);
            return _SM_HANDLED();
        }

        case INPUT_CMPS_MNGR_MOVE_HOME_SIG: {
            SM_InputCmpsMngr_moveHome_(manager);
            return _SM_HANDLED();
        }

        case INPUT_CMPS_MNGR_MOVE_END_SIG: {
            SM_InputCmpsMngr_moveEnd_(manager);
            return _SM_HANDLED();
        }

        default: {
            return _SM_SUPER();
        }
    }
}

//============================================================================
//=== UI event contract adapter

static UI_Signal SM_InputCmpsMngr_routeSignal_(UI_Signal const sig) {
    switch (sig) {
        case UI_KEY_ENTER_SIG: {
            return INPUT_CMPS_MNGR_CONFIRM_SIG;
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

        case UI_KEY_HOME_SIG: {
            return INPUT_CMPS_MNGR_MOVE_HOME_SIG;
        }

        case UI_KEY_END_SIG: {
            return INPUT_CMPS_MNGR_MOVE_END_SIG;
        }

        case UI_INPUT_SIG:
        case UI_KEY_ESC_SIG:
        case UI_KEY_UP_SIG:
        case UI_KEY_DOWN_SIG:
        case UI_KEY_J_SIG:
        case UI_KEY_K_SIG:
        case UI_KEY_CTRL_N_SIG:
        case UI_KEY_CTRL_P_SIG: {
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

static size_t SM_InputCmpsMngr_inputSize_(UI_Input const * const input) {
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

static size_t SM_InputCmpsMngr_nextPos_(
    SM_InputCmpsMngr const * const me)
{
    if (me->editPos >= me->length) {
        return me->length;
    }

    size_t const size = SM_InputCmpsMngr_utf8CodepointSize_(
        &me->buffer[me->editPos], me->length - me->editPos);
    DBC_ASSERT(500, size > 0U);
    return me->editPos + size;
}

static void SM_InputCmpsMngr_insert_(
    SM_InputCmpsMngr * const me,
    UI_Input const * const input)
{
    size_t const inputSize = SM_InputCmpsMngr_inputSize_(input);
    if (inputSize == 0U) {
        return;
    }
    if ((me->length + inputSize) >= sizeof(me->buffer)) {
        return;
    }

    size_t const dirtyPos = me->editPos;
    memmove(&me->buffer[me->editPos + inputSize],
            &me->buffer[me->editPos],
            me->length - me->editPos + 1U);
    memcpy(&me->buffer[me->editPos], input->utf8, inputSize);
    me->length += inputSize;
    me->editPos += inputSize;

    InputComposer_projectFrom(&me->composer, me->buffer,
                              me->length, me->editPos, dirtyPos);
}

static void SM_InputCmpsMngr_deletePrev_(SM_InputCmpsMngr * const me) {
    if (me->editPos == 0U) {
        return;
    }

    size_t const prevPos = SM_InputCmpsMngr_prevPos_(me, me->editPos);
    size_t const removed = me->editPos - prevPos;
    memmove(&me->buffer[prevPos], &me->buffer[me->editPos],
            me->length - me->editPos + 1U);
    me->length -= removed;
    me->editPos = prevPos;
    InputComposer_projectFrom(&me->composer, me->buffer,
                              me->length, me->editPos, prevPos);
}

static void SM_InputCmpsMngr_deleteToStart_(
    SM_InputCmpsMngr * const me)
{
    if (me->editPos == 0U) {
        return;
    }

    memmove(me->buffer, &me->buffer[me->editPos],
            me->length - me->editPos + 1U);
    me->length -= me->editPos;
    me->editPos = 0U;
    InputComposer_projectFrom(&me->composer, me->buffer,
                              me->length, me->editPos, 0U);
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
    if (wordStart == me->editPos) {
        return;
    }

    size_t const removed = me->editPos - wordStart;
    memmove(&me->buffer[wordStart], &me->buffer[me->editPos],
            me->length - me->editPos + 1U);
    me->length -= removed;
    me->editPos = wordStart;
    InputComposer_projectFrom(&me->composer, me->buffer,
                              me->length, me->editPos, wordStart);
}

static void SM_InputCmpsMngr_moveLeft_(SM_InputCmpsMngr * const me) {
    size_t const prevPos = SM_InputCmpsMngr_prevPos_(me, me->editPos);
    if (prevPos == me->editPos) {
        return;
    }

    me->editPos = prevPos;
    InputComposer_moveCursor(&me->composer, me->buffer,
                             me->length, me->editPos);
}

static void SM_InputCmpsMngr_moveRight_(SM_InputCmpsMngr * const me) {
    size_t const nextPos = SM_InputCmpsMngr_nextPos_(me);
    if (nextPos == me->editPos) {
        return;
    }

    me->editPos = nextPos;
    InputComposer_moveCursor(&me->composer, me->buffer,
                             me->length, me->editPos);
}

static void SM_InputCmpsMngr_moveHome_(SM_InputCmpsMngr * const me) {
    if (me->editPos == 0U) {
        return;
    }

    me->editPos = 0U;
    InputComposer_moveCursor(&me->composer, me->buffer,
                             me->length, me->editPos);
}

static void SM_InputCmpsMngr_moveEnd_(SM_InputCmpsMngr * const me) {
    if (me->editPos == me->length) {
        return;
    }

    me->editPos = me->length;
    InputComposer_moveCursor(&me->composer, me->buffer,
                             me->length, me->editPos);
}

//============================================================================
//=== Constructor / lifecycle

void SM_InputCmpsMngr_ctor(SM_InputCmpsMngr * const me) {
    DBC_REQUIRE(100, me != (SM_InputCmpsMngr *)0);

    me->buffer[0] = '\0';
    me->length = 0U;
    me->editPos = 0U;
    InputComposer_init(&me->composer);
}

void SM_InputCmpsMngr_init(SM_InputCmpsMngr * const me) {
    DBC_REQUIRE(200, me != (SM_InputCmpsMngr *)0);
    SM_Hsm_init_(
        &me->super,
        (SM_InitHandler)SM_InputCmpsMngr_TOP_initial);
}

void SM_InputCmpsMngr_dispatchEvt(SM_InputCmpsMngr * const me,
                                  UI_InputEvt const * const e)
{
    DBC_REQUIRE(300, me != (SM_InputCmpsMngr *)0);
    DBC_REQUIRE(301, e != (UI_InputEvt const *)0);

    UI_Signal const sig = SM_InputCmpsMngr_routeSignal_(e->super.sig);
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
    InputComposer_create(&me->composer, parent, owner, y, cols, resizeCb);
    InputComposer_projectAll(&me->composer, me->buffer,
                             me->length, me->editPos);
}

void SM_InputCmpsMngr_destroy(SM_InputCmpsMngr * const me) {
    DBC_REQUIRE(410, me != (SM_InputCmpsMngr *)0);
    InputComposer_destroy(&me->composer);
}

void SM_InputCmpsMngr_resize(SM_InputCmpsMngr * const me,
                             int const y,
                             unsigned const cols)
{
    DBC_REQUIRE(420, me != (SM_InputCmpsMngr *)0);
    InputComposer_resize(&me->composer, y, cols);
    InputComposer_projectAll(&me->composer, me->buffer,
                             me->length, me->editPos);
}

void SM_InputCmpsMngr_setActive(SM_InputCmpsMngr * const me,
                                bool const active)
{
    DBC_REQUIRE(430, me != (SM_InputCmpsMngr *)0);
    if (active) {
        InputComposer_showCursor(&me->composer, me->buffer,
                                 me->length, me->editPos);
    } else {
        InputComposer_hideCursor(&me->composer, me->buffer,
                                 me->length, me->editPos);
    }
}
