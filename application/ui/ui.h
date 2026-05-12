//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#ifndef UI_H_
#define UI_H_

#include "ui_evt.h"
#include <notcurses/notcurses.h>

//============================================================================
//=== UI module — main thread event loop, notcurses rendering

// Initialize UI planes, eventfd, tick registration (call before SST starts)
void UI_prepare(struct notcurses *nc);

// Run the UI event loop (blocks until quit)
void UI_loop(void);

#endif // UI_H_
