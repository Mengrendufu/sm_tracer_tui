//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#include <string.h>
#include <notcurses/notcurses.h>
#include "dbc_assert.h"
#include "widget_io_priv.h"
#include "input_composer.h"
DBC_MODULE_NAME("input_composer")

#define INPUT_COMPOSER_PROMPT_COLS_ 2U

//============================================================================
//=== Component IO

static void InputComposer_setBase_(struct ncplane * const plane) {
    DBC_REQUIRE(100, plane != (struct ncplane *)0);

    nccell base = NCCELL_TRIVIAL_INITIALIZER;
    nccell_set_bg_rgb8(&base, 38, 38, 42);
    nccell_set_fg_rgb8(&base, 225, 230, 232);
    nccell_load_char(plane, &base, ' ');
    ncplane_set_base_cell(plane, &base);
    nccell_release(plane, &base);
    ncplane_erase(plane);
}

static void InputComposer_drawPrompt_(
    struct InputComposer * const composer)
{
    DBC_REQUIRE(110, composer != (struct InputComposer *)0);
    DBC_REQUIRE(111, composer->plane != (struct ncplane *)0);

    InputComposer_setBase_(composer->plane);
    ncplane_set_bg_rgb8(composer->plane, 38, 38, 42);
    ncplane_set_fg_rgb8(composer->plane, 105, 185, 170);
    ncplane_on_styles(composer->plane, NCSTYLE_BOLD);
    (void)WidgetIO_putStrYx(composer->plane, 0, 0U, "> ");
    ncplane_off_styles(composer->plane, NCSTYLE_BOLD);
}

static void InputComposer_syncCursor_(
    struct InputComposer * const composer)
{
    DBC_REQUIRE(120, composer != (struct InputComposer *)0);
    DBC_REQUIRE(121, composer->reader != (struct ncreader *)0);

    if (!composer->active) {
        return;
    }

    struct ncplane * const readerPlane =
        ncreader_plane(composer->reader);
    int planeY;
    int planeX;
    unsigned cursorY;
    unsigned cursorX;
    ncplane_abs_yx(readerPlane, &planeY, &planeX);
    ncplane_cursor_yx(readerPlane, &cursorY, &cursorX);
    (void)notcurses_cursor_enable(
        ncplane_notcurses(readerPlane),
        planeY + (int)cursorY,
        planeX + (int)cursorX);
}

//============================================================================
//=== Lifecycle

void InputComposer_init(struct InputComposer * const composer) {
    DBC_REQUIRE(200, composer != (struct InputComposer *)0);

    composer->plane  = (struct ncplane *)0;
    composer->reader = (struct ncreader *)0;
    composer->active = false;
}

void InputComposer_create(
    struct InputComposer * const composer,
    struct ncplane * const parent,
    void * const owner,
    int const y,
    unsigned const cols,
    InputComposer_ResizeCb const resizeCb)
{
    DBC_REQUIRE(210, composer != (struct InputComposer *)0);
    DBC_REQUIRE(211, parent != (struct ncplane *)0);
    DBC_REQUIRE(212, cols > INPUT_COMPOSER_PROMPT_COLS_);

    ncplane_options inputOpts = {
        .y = y, .x = 2, .rows = 1, .cols = cols,
        .name = "input-composer", .userptr = owner,
        .resizecb = resizeCb,
    };
    composer->plane = ncplane_create(parent, &inputOpts);
    DBC_ENSURE(300, composer->plane != (struct ncplane *)0);
    InputComposer_drawPrompt_(composer);

    ncplane_options readerOpts = {
        .y = 0, .x = (int)INPUT_COMPOSER_PROMPT_COLS_,
        .rows = 1, .cols = cols - INPUT_COMPOSER_PROMPT_COLS_,
        .name = "input-reader",
    };
    struct ncplane * const readerPlane =
        ncplane_create(composer->plane, &readerOpts);
    DBC_ENSURE(301, readerPlane != (struct ncplane *)0);
    InputComposer_setBase_(readerPlane);

    ncreader_options const readerOptsCfg = {
        .tchannels = NCCHANNELS_INITIALIZER(
            225, 230, 232,
            38, 38, 42),
        .tattrword = 0U,
        .flags = NCREADER_OPTION_HORSCROLL,
    };
    composer->reader = ncreader_create(readerPlane, &readerOptsCfg);
    DBC_ENSURE(302, composer->reader != (struct ncreader *)0);
}

void InputComposer_destroy(struct InputComposer * const composer) {
    DBC_REQUIRE(250, composer != (struct InputComposer *)0);
    DBC_REQUIRE(251, composer->plane != (struct ncplane *)0);
    DBC_REQUIRE(252, composer->reader != (struct ncreader *)0);

    if (composer->active) {
        InputComposer_setActive(composer, false);
    }

    // ncreader owns and destroys its bound child plane.
    ncreader_destroy(composer->reader, (char **)0);
    composer->reader = (struct ncreader *)0;

    DBC_ALLEGE(303, ncplane_destroy(composer->plane) == 0);
    composer->plane = (struct ncplane *)0;
    composer->active = false;
}

void InputComposer_resize(struct InputComposer * const composer,
                          int const y,
                          unsigned const cols)
{
    DBC_REQUIRE(220, composer != (struct InputComposer *)0);
    DBC_REQUIRE(221, composer->plane != (struct ncplane *)0);
    DBC_REQUIRE(222, composer->reader != (struct ncreader *)0);
    DBC_REQUIRE(223, cols > INPUT_COMPOSER_PROMPT_COLS_);

    ncplane_move_yx(composer->plane, y, 2);
    ncplane_resize_simple(composer->plane, 1U, cols);
    struct ncplane * const readerPlane =
        ncreader_plane(composer->reader);
    ncplane_resize_simple(
        readerPlane, 1U, cols - INPUT_COMPOSER_PROMPT_COLS_);
    InputComposer_drawPrompt_(composer);
    InputComposer_syncCursor_(composer);
}

void InputComposer_setActive(struct InputComposer * const composer,
                             bool const active)
{
    DBC_REQUIRE(230, composer != (struct InputComposer *)0);
    DBC_REQUIRE(231, composer->reader != (struct ncreader *)0);

    if (composer->active == active) {
        return;
    }

    composer->active = active;
    if (active) {
        InputComposer_syncCursor_(composer);
    } else {
        (void)notcurses_cursor_disable(
            ncplane_notcurses(ncreader_plane(composer->reader)));
    }
}

void InputComposer_offerInput(
    struct InputComposer * const composer,
    UI_Input const * const input)
{
    DBC_REQUIRE(240, composer != (struct InputComposer *)0);
    DBC_REQUIRE(241, composer->reader != (struct ncreader *)0);
    DBC_REQUIRE(242, input != (UI_Input const *)0);
    DBC_REQUIRE(243, composer->active);

    ncinput nativeInput = {
        .id = input->id,
        .y = -1,
        .x = -1,
        .evtype = (ncintype_e)input->type,
        .modifiers = input->modifiers,
        .ypx = -1,
        .xpx = -1,
    };
    memcpy(nativeInput.utf8, input->utf8, sizeof(nativeInput.utf8));
    (void)ncreader_offer_input(composer->reader, &nativeInput);
    InputComposer_syncCursor_(composer);
}
