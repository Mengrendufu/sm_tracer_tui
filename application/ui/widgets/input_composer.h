//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#ifndef INPUT_COMPOSER_H_
#define INPUT_COMPOSER_H_

#include <stdbool.h>
#include <stddef.h>

struct ncplane;

// Passive projection owned by SM_InputCmpsMngr. Cursor rendering stays local.
struct InputComposer {
    struct ncplane *plane;
    unsigned         viewRow;
    unsigned         cursorRow;
    unsigned         cursorCol;
    bool             cursorVisible;
    bool             limitReached;
};

typedef int (*InputComposer_ResizeCb)(struct ncplane *plane);

void InputComposer_init(struct InputComposer *composer);
void InputComposer_create(struct InputComposer *composer,
                          struct ncplane *parent,
                          void *owner,
                          int y,
                          unsigned cols,
                          InputComposer_ResizeCb resizeCb);
void InputComposer_destroy(struct InputComposer *composer);
void InputComposer_setLimitReached(struct InputComposer *composer,
                                   bool reached);
void InputComposer_resize(struct InputComposer *composer,
                          int y,
                          unsigned rows,
                          unsigned cols);
unsigned InputComposer_preferredRows(struct InputComposer const *composer,
                                     char const *text,
                                     size_t len,
                                     size_t editPos,
                                     unsigned cols);
void InputComposer_showCursor(struct InputComposer *composer,
                              char const *text,
                              size_t len,
                              size_t editPos);
void InputComposer_hideCursor(struct InputComposer *composer,
                              char const *text,
                              size_t len,
                              size_t editPos);
void InputComposer_projectAll(struct InputComposer *composer,
                              char const *text,
                              size_t len,
                              size_t editPos);
void InputComposer_projectFrom(struct InputComposer *composer,
                               char const *text,
                               size_t len,
                               size_t editPos,
                               size_t dirtyPos);
void InputComposer_moveCursor(struct InputComposer *composer,
                              char const *text,
                              size_t len,
                              size_t editPos);

#endif // INPUT_COMPOSER_H_
