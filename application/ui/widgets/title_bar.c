//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#include <notcurses/notcurses.h>
#include "dbc_assert.h"
#include "widget_io_priv.h"
#include "title_bar.h"
DBC_MODULE_NAME("title_bar")

//============================================================================
static void TitleBar_draw_(struct TitleBar * const bar) {
    DBC_REQUIRE(100, bar != (struct TitleBar *)0);
    DBC_REQUIRE(101, bar->plane != (struct ncplane *)0);

    nccell base = NCCELL_TRIVIAL_INITIALIZER;
    nccell_set_bg_rgb8(&base, 24, 27, 31);
    nccell_load_char(bar->plane, &base, ' ');
    ncplane_set_base_cell(bar->plane, &base);
    nccell_release(bar->plane, &base);
    ncplane_erase(bar->plane);

    ncplane_set_bg_rgb8(bar->plane, 60, 60, 120);
    ncplane_off_styles(bar->plane, NCSTYLE_BOLD);
    ncplane_set_fg_rgb8(bar->plane, 170, 175, 215);
    (void)WidgetIO_putStrYx(bar->plane, 0, 0U, " sm_tracer_tui: ");

    ncplane_on_styles(bar->plane, NCSTYLE_BOLD);
    ncplane_set_fg_rgb8(bar->plane, 235, 235, 255);
    (void)WidgetIO_putStr(bar->plane, "v0.0.1");

    ncplane_off_styles(bar->plane, NCSTYLE_BOLD);
    ncplane_set_fg_rgb8(bar->plane, 140, 145, 185);
    (void)WidgetIO_putStr(bar->plane, " ");

    ncplane_on_styles(bar->plane, NCSTYLE_BOLD);
    ncplane_set_fg_rgb8(bar->plane, 170, 230, 210);
    (void)WidgetIO_putStr(bar->plane, "notcurses ");
    ncplane_off_styles(bar->plane, NCSTYLE_BOLD);
}

//============================================================================
void TitleBar_init(struct TitleBar * const bar) {
    DBC_REQUIRE(200, bar != (struct TitleBar *)0);
    bar->plane = (struct ncplane *)0;
}

void TitleBar_create(struct TitleBar * const bar,
                     struct ncplane * const parent,
                     void * const owner,
                     unsigned const cols,
                     TitleBar_ResizeCb const resizeCb)
{
    DBC_REQUIRE(210, bar != (struct TitleBar *)0);
    DBC_REQUIRE(211, parent != (struct ncplane *)0);

    ncplane_options nopts = {
        .y = 1, .x = 2, .rows = 1, .cols = cols, .name = "title",
        .userptr = owner, .resizecb = resizeCb,
    };
    bar->plane = ncplane_create(parent, &nopts);
    DBC_ENSURE(300, bar->plane != (struct ncplane *)0);
    TitleBar_draw_(bar);
}

void TitleBar_resize(struct TitleBar * const bar, unsigned const cols) {
    DBC_REQUIRE(220, bar != (struct TitleBar *)0);
    DBC_REQUIRE(221, bar->plane != (struct ncplane *)0);
    ncplane_resize_simple(bar->plane, 1, cols);
}
