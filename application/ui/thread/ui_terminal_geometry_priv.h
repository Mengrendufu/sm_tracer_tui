//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#ifndef UI_TERMINAL_GEOMETRY_PRIV_H_
#define UI_TERMINAL_GEOMETRY_PRIV_H_

#include <stdbool.h>

typedef struct {
    unsigned rows;
    unsigned cols;
} UI_TerminalGeometry;

static inline bool UI_TerminalGeometry_update(
    UI_TerminalGeometry * const previous,
    UI_TerminalGeometry const current)
{
    bool const changed = (previous->rows != current.rows)
                      || (previous->cols != current.cols);
    *previous = current;
    return changed;
}

#endif // UI_TERMINAL_GEOMETRY_PRIV_H_
