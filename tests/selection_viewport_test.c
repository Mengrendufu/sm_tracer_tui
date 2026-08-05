//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#include <assert.h>
#include "widgets/selection_viewport_priv.h"

int main(void) {
    size_t first = 0U;

    first = SelectionViewport_update(first, 6U, 7U, 6U);
    assert(first == 1U);

    first = SelectionViewport_update(first, 5U, 7U, 6U);
    assert(first == 1U);

    first = SelectionViewport_update(first, 0U, 7U, 6U);
    assert(first == 0U);

    first = SelectionViewport_update(first, 0U, 4U, 4U);
    assert(first == 0U);
    return 0;
}
