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
#include <string.h>
#include "text_buffer_view.h"

int main(void) {
    struct TextArea ta;
    TextArea_init(&ta);

    assert(ta.lineTotal == 0U);
    assert(ta.lineHead == 0U);
    assert(ta.scrollOff == 0);

    assert(TextArea_push(&ta, "one\ntwo\nthree", 13U) == 3U);
    assert(TextArea_total(&ta) == 3U);
    assert(strcmp(TextArea_lineAt(&ta, 0U), "one") == 0);
    assert(strcmp(TextArea_lineAt(&ta, 1U), "two") == 0);
    assert(strcmp(TextArea_lineAt(&ta, 2U), "three") == 0);

    assert(TextArea_firstVisible(&ta, 2U) == 1U);
    TextArea_scrollBy(&ta, 1, 2U);
    assert(TextArea_scrollOffset(&ta) == 1);
    assert(TextArea_firstVisible(&ta, 2U) == 0U);
    TextArea_scrollBy(&ta, -5, 2U);
    assert(TextArea_scrollOffset(&ta) == 0);

    TextArea_clear(&ta);
    assert(ta.lineTotal == 0U);
    assert(ta.lineHead == 0U);
    assert(ta.scrollOff == 0);
    return 0;
}
