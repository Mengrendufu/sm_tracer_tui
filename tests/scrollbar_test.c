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
#include "widgets/text_buffer_view.h"

int main(void) {
    ScrollBar bar;
    ScrollBar_init(&bar);
    assert(bar.enabled);
    assert(bar.style.thumb.r == 140U);
    assert(bar.style.track.b == 100U);

    ScrollBar_Thumb t = ScrollBar_computeThumb(100U, 10U, 0U);
    assert(t.visible);
    assert(t.height == 1U);
    assert(t.pos == 9U);
    assert(t.maxOff == 90);
    assert(ScrollBar_cellCh(&t, 9U) == 0x2588U);
    assert(ScrollBar_cellCh(&t, 8U) == 0x2591U);

    t = ScrollBar_computeThumb(5U, 10U, 0U);
    assert(!t.visible);
    assert(ScrollBar_cellCh(&t, 0U) == 0U);
    return 0;
}
