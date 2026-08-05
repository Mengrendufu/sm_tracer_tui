//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#include "dbc_assert.h"
#include "selection_viewport_priv.h"
DBC_MODULE_NAME("selection_viewport")

size_t SelectionViewport_update(size_t firstVisible,
                                size_t const selected,
                                size_t const itemCount,
                                size_t const visibleCount)
{
    DBC_REQUIRE(100, itemCount > 0U);
    DBC_REQUIRE(101, selected < itemCount);
    DBC_REQUIRE(102, (visibleCount > 0U)
                     && (visibleCount <= itemCount));

    size_t const maxFirst = itemCount - visibleCount;
    if (firstVisible > maxFirst) {
        firstVisible = maxFirst;
    }
    if (selected < firstVisible) {
        firstVisible = selected;
    } else if (selected >= firstVisible + visibleCount) {
        firstVisible = selected - visibleCount + 1U;
    }
    return firstVisible;
}
