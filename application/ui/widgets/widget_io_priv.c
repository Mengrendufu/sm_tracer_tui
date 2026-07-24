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
DBC_MODULE_NAME("widget_io")

//============================================================================
static bool WidgetIO_canPutStr_(struct ncplane * const plane,
                                unsigned const x,
                                char const * const text)
{
    DBC_REQUIRE(100, plane != (struct ncplane *)0);
    DBC_REQUIRE(101, text != (char const *)0);

    int const textWidth = ncstrwidth(text, NULL, NULL);
    if (textWidth < 0) {
        return false;
    }

    unsigned cols;
    ncplane_dim_yx(plane, NULL, &cols);
    return x < cols && (unsigned)textWidth <= (cols - x);
}

bool WidgetIO_putStrYx(struct ncplane * const plane,
                       int const y,
                       unsigned const x,
                       char const * const text)
{
    DBC_REQUIRE(200, plane != (struct ncplane *)0);
    DBC_REQUIRE(201, text != (char const *)0);

    return WidgetIO_canPutStr_(plane, x, text)
           && ncplane_putstr_yx(plane, y, (int)x, text) >= 0;
}

bool WidgetIO_putStr(struct ncplane * const plane,
                     char const * const text)
{
    DBC_REQUIRE(210, plane != (struct ncplane *)0);
    DBC_REQUIRE(211, text != (char const *)0);

    unsigned y;
    unsigned x;
    ncplane_cursor_yx(plane, &y, &x);
    return WidgetIO_putStrYx(plane, (int)y, x, text);
}
