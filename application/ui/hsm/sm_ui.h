//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#ifndef SM_UI_H_
#define SM_UI_H_

#include <stdbool.h>
#include "ui_evt.h"

struct notcurses;

typedef struct {
    void (*requestQuit)(void *ctx);
    void *ctx;
} SM_UI_HostOps;

void SM_UI_setup(struct notcurses *nc,
                 SM_UI_HostOps const *hostOps);
bool SM_UI_needsRender(void);
void SM_UI_flush(void);
void SM_UI_dispatchEvt(UI_Evt const *e);

#endif // SM_UI_H_
