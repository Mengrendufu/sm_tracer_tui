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
#include "ui_input.h"

struct ncplane;
struct ncreader;

// Owns its notcurses plane and reader for the UI subsystem lifetime.
struct InputComposer {
    struct ncplane  *plane;
    struct ncreader *reader;
    bool             active;
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
void InputComposer_resize(struct InputComposer *composer,
                          int y,
                          unsigned cols);
void InputComposer_setActive(struct InputComposer *composer, bool active);
void InputComposer_offerInput(struct InputComposer *composer,
                              UI_Input const *input);

#endif // INPUT_COMPOSER_H_
