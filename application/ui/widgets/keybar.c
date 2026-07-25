//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#include <stdint.h>
#include <notcurses/notcurses.h>
#include "dbc_assert.h"
#include "widget_io_priv.h"
#include "keybar.h"
DBC_MODULE_NAME("keybar")

typedef struct {
    char const *key;
    char const *desc;
} Keybar_Item_;

//============================================================================
static void Keybar_set_(struct Keybar * const keybar,
                        Keybar_Item_ const * const items,
                        uint32_t const nItems)
{
    DBC_REQUIRE(100, keybar != (struct Keybar *)0);
    DBC_REQUIRE(101, keybar->plane != (struct ncplane *)0);
    DBC_REQUIRE(102, items != (Keybar_Item_ const *)0);

    nccell base = NCCELL_TRIVIAL_INITIALIZER;
    nccell_set_bg_rgb8(&base, 24, 27, 31);
    nccell_load_char(keybar->plane, &base, ' ');
    ncplane_set_base_cell(keybar->plane, &base);
    nccell_release(keybar->plane, &base);
    ncplane_erase(keybar->plane);

    ncplane_set_bg_rgb8(keybar->plane, 50, 50, 80);
    for (uint32_t i = 0U; i < nItems; ++i) {
        ncplane_set_fg_rgb8(keybar->plane, 230, 200, 100);
        ncplane_on_styles(keybar->plane, NCSTYLE_BOLD);
        if (!WidgetIO_putStr(keybar->plane, items[i].key)) {
            ncplane_off_styles(keybar->plane, NCSTYLE_BOLD);
            break;
        }
        ncplane_off_styles(keybar->plane, NCSTYLE_BOLD);
        ncplane_set_fg_rgb8(keybar->plane, 160, 160, 180);
        if (!WidgetIO_putStr(keybar->plane, items[i].desc)) {
            break;
        }
    }
    (void)WidgetIO_putStr(keybar->plane, "  ");
}

//============================================================================
void Keybar_init(struct Keybar * const keybar) {
    DBC_REQUIRE(200, keybar != (struct Keybar *)0);
    keybar->plane = (struct ncplane *)0;
}

void Keybar_create(struct Keybar * const keybar,
                   struct ncplane * const parent,
                   void * const owner,
                   int const y,
                   unsigned const cols,
                   Keybar_ResizeCb const resizeCb)
{
    DBC_REQUIRE(210, keybar != (struct Keybar *)0);
    DBC_REQUIRE(211, parent != (struct ncplane *)0);

    ncplane_options nopts = {
        .y = y, .x = 2, .rows = 1, .cols = cols, .name = "keybar",
        .userptr = owner, .resizecb = resizeCb,
    };
    keybar->plane = ncplane_create(parent, &nopts);
    DBC_ENSURE(300, keybar->plane != (struct ncplane *)0);
    Keybar_showMainHints(keybar);
}

void Keybar_resize(struct Keybar * const keybar,
                   int const y,
                   unsigned const cols)
{
    DBC_REQUIRE(220, keybar != (struct Keybar *)0);
    DBC_REQUIRE(221, keybar->plane != (struct ncplane *)0);

    ncplane_move_yx(keybar->plane, y, 2);
    ncplane_resize_simple(keybar->plane, 1, cols);
}

void Keybar_showMainHints(struct Keybar * const keybar) {
    static Keybar_Item_ const items[] = {
        { "  ctrl+/", " open menu" },
    };
    Keybar_set_(keybar, items, sizeof(items) / sizeof(items[0]));
}

void Keybar_showMenuHints(struct Keybar * const keybar) {
    static Keybar_Item_ const items[] = {
        { "  ctrl+/", " close menu" },
        { "  j/k \xe2\x86\x91\xe2\x86\x93", " navigate" },
        { "  enter", " select" },
    };
    Keybar_set_(keybar, items, sizeof(items) / sizeof(items[0]));
}
