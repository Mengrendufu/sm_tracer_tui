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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <notcurses/notcurses.h>
#include "widgets/text_buffer_view.h"

static int resizeCb_(struct ncplane * const plane) {
    (void)plane;
    return 0;
}

static bool cellFgEquals_(struct ncplane * const plane,
                          int const y,
                          unsigned const expectedR,
                          unsigned const expectedG,
                          unsigned const expectedB)
{
    uint64_t channels = 0U;
    char * const contents = ncplane_at_yx(
        plane, y, 0, (uint16_t *)0, &channels);
    free(contents);

    unsigned r;
    unsigned g;
    unsigned b;
    (void)ncchannels_fg_rgb8(channels, &r, &g, &b);
    return ncchannels_fg_rgb_p(channels)
           && r == expectedR && g == expectedG && b == expectedB;
}

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

    struct notcurses_options const opts = {
        .flags = NCOPTION_SUPPRESS_BANNERS
               | NCOPTION_NO_ALTERNATE_SCREEN
               | NCOPTION_NO_FONT_CHANGES
    };
    FILE * const output = tmpfile();
    assert(output != (FILE *)0);
    struct notcurses * const nc = notcurses_core_init(&opts, output);
    assert(nc != (struct notcurses *)0);

    struct TextBufferView view;
    TextBufferView_init(&view);
    TextBufferView_create(&view, notcurses_stdplane(nc), &view,
                          0, 4U, 40U, 0U, &resizeCb_, &resizeCb_);
    char const lines[] =
        "[SYS_INFO]> Protocol loaded.\n"
        "[220]==ledOff==\n";
    TextBufferView_pushText(&view, lines, sizeof(lines) - 1U);
    TextBufferView_refresh(&view);

    assert(cellFgEquals_(view.contentPlane, 0, 105U, 185U, 170U));
    assert(cellFgEquals_(view.contentPlane, 1, 200U, 220U, 200U));

    assert(notcurses_stop(nc) == 0);
    assert(fclose(output) == 0);
    return 0;
}
