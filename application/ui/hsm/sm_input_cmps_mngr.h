//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#ifndef SM_INPUT_CMPS_MNGR_H_
#define SM_INPUT_CMPS_MNGR_H_

#include <stddef.h>
#include "sm_hsm.h"
#include "sm_ui_evt.h"
#include "widgets/command_suggestion.h"
#include "widgets/input_composer.h"

#define SM_INPUT_CMPS_MNGR_BUFFER_SIZE 256U
#define SM_INPUT_CMPS_MNGR_MAX_TOKENS  32U

//============================================================================
//=== InputComposer Manager HSM -- input editing state owner

typedef enum {
    SM_INPUT_COMMAND_CONNECT,
    SM_INPUT_COMMAND_DISCONNECT,
    SM_INPUT_COMMAND_REFRESH,
    SM_INPUT_COMMAND_NUM
} SM_InputCommand;

typedef struct {
    size_t begin;
    size_t end;
    SM_InputCommand command;
} SM_InputCmpsMngrToken;

typedef struct {
    SM_Hsm super;
    char buffer[SM_INPUT_CMPS_MNGR_BUFFER_SIZE]; // canonical UTF-8 text
    size_t length;
    size_t editPos; // UTF-8 byte offset, never a terminal cursor coordinate
    SM_InputCmpsMngrToken tokens[SM_INPUT_CMPS_MNGR_MAX_TOKENS];
    size_t tokenCount;
    SM_InputCommand candidates[SM_INPUT_COMMAND_NUM];
    size_t candidateCount;
    size_t selectedCandidate;
    size_t suggestionStart;
    size_t suggestionEnd;
    int inputY;
    unsigned cols;
    struct InputComposer composer;
    struct CommandSuggestion suggestion;
} SM_InputCmpsMngr;

void SM_InputCmpsMngr_ctor(SM_InputCmpsMngr *me);
void SM_InputCmpsMngr_init(SM_InputCmpsMngr * const me);
// One-way state-machine event dispatch; no action result is returned.
void SM_InputCmpsMngr_dispatchEvt(SM_InputCmpsMngr * const me,
                                  UI_InputEvt const * const e);
void SM_InputCmpsMngr_create(SM_InputCmpsMngr *me,
                             struct ncplane *parent,
                             void *owner,
                             int y,
                             unsigned cols,
                             InputComposer_ResizeCb resizeCb);
void SM_InputCmpsMngr_destroy(SM_InputCmpsMngr *me);
void SM_InputCmpsMngr_resize(SM_InputCmpsMngr *me,
                             int y,
                             unsigned cols);
void SM_InputCmpsMngr_setActive(SM_InputCmpsMngr *me, bool active);

#endif // SM_INPUT_CMPS_MNGR_H_
