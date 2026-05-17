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
#include <time.h>
#include "ui_evt.h"

struct notcurses;

void SM_UI_setup(struct notcurses *nc);
bool SM_UI_shouldQuit(void);
bool SM_UI_needsRender(void);
struct timespec SM_UI_lastRender(void);
void SM_UI_onRenderFrame(struct timespec const *now);
struct notcurses *SM_UI_nc(void);
void SM_UI_dispatchEvt(UI_Evt const *e);

#endif // SM_UI_H_
