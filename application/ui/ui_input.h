//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#ifndef UI_INPUT_H_
#define UI_INPUT_H_

#include <stdint.h>

// Normalized notcurses input snapshot shared inside the UI package.
typedef struct {
    uint32_t id;
    uint32_t modifiers;
    uint32_t type;
    char     utf8[5];
} UI_Input;

#endif // UI_INPUT_H_
