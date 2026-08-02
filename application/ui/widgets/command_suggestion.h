//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#ifndef COMMAND_SUGGESTION_H_
#define COMMAND_SUGGESTION_H_

#include <stddef.h>

struct ncplane;

// Passive candidate projection owned by SM_InputCmpsMngr.
struct CommandSuggestion {
    struct ncplane *plane;
};

void CommandSuggestion_init(struct CommandSuggestion *suggestion);
void CommandSuggestion_create(struct CommandSuggestion *suggestion,
                              struct ncplane *parent);
void CommandSuggestion_destroy(struct CommandSuggestion *suggestion);
void CommandSuggestion_show(
    struct CommandSuggestion *suggestion,
    int inputY,
    unsigned maxCols,
    char const * const candidates[],
    size_t count,
    size_t selected);
void CommandSuggestion_hide(struct CommandSuggestion *suggestion);

#endif // COMMAND_SUGGESTION_H_
