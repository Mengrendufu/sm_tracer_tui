//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#ifndef SELECTION_VIEWPORT_PRIV_H_
#define SELECTION_VIEWPORT_PRIV_H_

#include <stddef.h>

size_t SelectionViewport_update(size_t firstVisible,
                                size_t selected,
                                size_t itemCount,
                                size_t visibleCount);

#endif // SELECTION_VIEWPORT_PRIV_H_
