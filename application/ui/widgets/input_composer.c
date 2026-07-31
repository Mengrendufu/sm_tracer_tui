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
//=== Projection internals

static size_t InputComposer_utf8CodepointSize_(
    char const * const text,
    size_t const remaining)
{
    unsigned char const lead = (unsigned char)text[0];
    size_t size = 0U;

    if (lead < 0x80U) {
        size = 1U;
    } else if ((lead & 0xE0U) == 0xC0U) {
        size = 2U;
    } else if ((lead & 0xF0U) == 0xE0U) {
        size = 3U;
    } else if ((lead & 0xF8U) == 0xF0U) {
        size = 4U;
    }

    if ((size == 0U) || (size > remaining)) {
        return 0U;
    }
    for (size_t i = 1U; i < size; ++i) {
        if (((unsigned char)text[i] & 0xC0U) != 0x80U) {
            return 0U;
        }
    }
    return size;
}

static size_t InputComposer_prevPos_(char const * const text,
                                     size_t const pos)
{
    size_t prev = pos;
    if (prev == 0U) {
        return 0U;
    }

    --prev;
    while ((prev > 0U)
           && (((unsigned char)text[prev] & 0xC0U) == 0x80U))
    {
        --prev;
    }
    return prev;
}

static unsigned InputComposer_width_(char const * const text,
                                     size_t const begin,
                                     size_t const end)
{
    unsigned width = 0U;
    size_t offset = begin;

    while (offset < end) {
        size_t const size = InputComposer_utf8CodepointSize_(
            &text[offset], end - offset);
        DBC_ASSERT(500, size > 0U);

        char egc[5] = {0};
        memcpy(egc, &text[offset], size);
        int const columns = ncstrwidth(egc, (int *)0, (int *)0);
        DBC_ASSERT(501, columns >= 0);
        width += (unsigned)columns;
        offset += size;
    }
    return width;
}

static unsigned InputComposer_contentCols_(
    struct InputComposer const * const composer)
{
    unsigned const cols = ncplane_dim_x(composer->plane);
    DBC_ASSERT(510, cols > INPUT_COMPOSER_PROMPT_COLS_);
    return cols - INPUT_COMPOSER_PROMPT_COLS_;
}

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
    ncplane_set_fg_rgb8(composer->plane, 225, 230, 232);
}

static unsigned InputComposer_cursorWidth_(
    struct InputComposer * const composer,
    char const * const text,
    size_t const len,
    size_t const editPos)
{
    if (editPos == len) {
        return 1U;
    }

    nccell cell = NCCELL_TRIVIAL_INITIALIZER;
    int const loaded = nccell_load(
        composer->plane, &cell, &text[editPos]);
    DBC_ASSERT(540, loaded > 0);
    (void)loaded;
    unsigned const width = nccell_cols(&cell);
    nccell_release(composer->plane, &cell);
    return width;
}

static bool InputComposer_updateView_(
    struct InputComposer * const composer,
    char const * const text,
    size_t const len,
    size_t const editPos)
{
    size_t const oldViewStart = composer->viewStart;
    unsigned const contentCols = InputComposer_contentCols_(composer);
    unsigned const cursorWidth = InputComposer_cursorWidth_(
        composer, text, len, editPos);

    if ((composer->viewStart > len)
        || ((composer->viewStart < len)
            && (((unsigned char)text[composer->viewStart] & 0xC0U)
                == 0x80U)))
    {
        composer->viewStart = 0U;
    }
    if (editPos < composer->viewStart) {
        composer->viewStart = editPos;
    }

    while ((composer->viewStart < editPos)
           && ((InputComposer_width_(text, composer->viewStart,
                                     editPos) + cursorWidth)
               > contentCols))
    {
        size_t const size = InputComposer_utf8CodepointSize_(
            &text[composer->viewStart], len - composer->viewStart);
        DBC_ASSERT(520, size > 0U);
        composer->viewStart += size;
    }

    while (composer->viewStart > 0U) {
        size_t const prev = InputComposer_prevPos_(
            text, composer->viewStart);
        if ((InputComposer_width_(text, prev, editPos) + cursorWidth)
            > contentCols)
        {
            break;
        }
        composer->viewStart = prev;
    }

    composer->cursorCol = InputComposer_width_(
        text, composer->viewStart, editPos);
    DBC_ASSERT(521, composer->cursorCol < contentCols);
    return composer->viewStart != oldViewStart;
}

static void InputComposer_drawCursor_(
    struct InputComposer * const composer,
    size_t const len,
    size_t const editPos)
{
    if (!composer->cursorVisible) {
        return;
    }

    nccell cursor = NCCELL_TRIVIAL_INITIALIZER;
    bool hasProjectedEgc = false;
    if (editPos < len) {
        int const loaded = ncplane_at_yx_cell(
            composer->plane,
            0,
            (int)(INPUT_COMPOSER_PROMPT_COLS_
                  + composer->cursorCol),
            &cursor);
        DBC_ASSERT(541, loaded >= 0);
        if (loaded > 0) {
            DBC_ASSERT(542, !nccell_wide_right_p(&cursor));
            nccell_set_channels(
                &cursor, ncchannels_reverse(nccell_channels(&cursor)));
            hasProjectedEgc = true;
        }
        (void)loaded;
    }
    if (!hasProjectedEgc) {
        int const loaded = nccell_prime(
            composer->plane,
            &cursor,
            " ",
            NCSTYLE_NONE,
            ncchannels_reverse(ncplane_channels(composer->plane)));
        DBC_ASSERT(543, loaded > 0);
        (void)loaded;
    }

    int const columns = ncplane_putc_yx(
        composer->plane,
        0,
        (int)(INPUT_COMPOSER_PROMPT_COLS_ + composer->cursorCol),
        &cursor);
    DBC_ALLEGE(300, columns == (int)nccell_cols(&cursor));
    (void)columns;
    nccell_release(composer->plane, &cursor);
}

static void InputComposer_clearFrom_(
    struct InputComposer * const composer,
    unsigned const column)
{
    unsigned const contentCols = InputComposer_contentCols_(composer);
    if (column >= contentCols) {
        return;
    }

    DBC_ALLEGE(301, ncplane_erase_region(
        composer->plane,
        0,
        (int)(INPUT_COMPOSER_PROMPT_COLS_ + column),
        1,
        (int)(contentCols - column)) == 0);
}

static void InputComposer_drawFrom_(
    struct InputComposer * const composer,
    char const * const text,
    size_t const len,
    size_t const offset,
    unsigned const column)
{
    InputComposer_clearFrom_(composer, column);
    unsigned const contentCols = InputComposer_contentCols_(composer);
    size_t textOffset = offset;
    unsigned textColumn = column;

    while ((textOffset < len) && (textColumn < contentCols)) {
        nccell cell = NCCELL_TRIVIAL_INITIALIZER;
        int const loaded = nccell_load(
            composer->plane, &cell, &text[textOffset]);
        DBC_ASSERT(530, loaded > 0);
        unsigned const egcCols = nccell_cols(&cell);
        nccell_release(composer->plane, &cell);
        if ((textColumn + egcCols) > contentCols) {
            break;
        }

        size_t bytes = 0U;
        int const columns = ncplane_putegc_yx(
            composer->plane,
            0,
            (int)(INPUT_COMPOSER_PROMPT_COLS_ + textColumn),
            &text[textOffset],
            &bytes);
        DBC_ALLEGE(302, columns >= 0);
        DBC_ASSERT(531, bytes == (size_t)loaded);
        (void)loaded;
        textOffset += bytes;
        textColumn += (unsigned)columns;
    }
}

static void InputComposer_drawAll_(
    struct InputComposer * const composer,
    char const * const text,
    size_t const len,
    size_t const editPos)
{
    InputComposer_drawFrom_(composer, text, len,
                            composer->viewStart, 0U);
    InputComposer_drawCursor_(composer, len, editPos);
}

static void InputComposer_validateProjection_(
    char const * const text,
    size_t const len,
    size_t const editPos)
{
    DBC_REQUIRE(130, text != (char const *)0);
    DBC_REQUIRE(131, editPos <= len);
    DBC_REQUIRE(132, text[len] == '\0');
    DBC_REQUIRE(133, (editPos == len)
                     || (((unsigned char)text[editPos] & 0xC0U) != 0x80U));
    (void)text;
    (void)len;
    (void)editPos;
}

//============================================================================
//=== Lifecycle

void InputComposer_init(struct InputComposer * const composer) {
    DBC_REQUIRE(200, composer != (struct InputComposer *)0);

    composer->plane = (struct ncplane *)0;
    composer->viewStart = 0U;
    composer->cursorCol = 0U;
    composer->cursorVisible = false;
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
    DBC_ENSURE(303, composer->plane != (struct ncplane *)0);
    InputComposer_drawPrompt_(composer);
}

void InputComposer_destroy(struct InputComposer * const composer) {
    DBC_REQUIRE(220, composer != (struct InputComposer *)0);
    DBC_REQUIRE(221, composer->plane != (struct ncplane *)0);

    DBC_ALLEGE(304, ncplane_destroy(composer->plane) == 0);
    composer->plane = (struct ncplane *)0;
    composer->viewStart = 0U;
    composer->cursorCol = 0U;
    composer->cursorVisible = false;
}

void InputComposer_resize(struct InputComposer * const composer,
                          int const y,
                          unsigned const cols)
{
    DBC_REQUIRE(230, composer != (struct InputComposer *)0);
    DBC_REQUIRE(231, composer->plane != (struct ncplane *)0);
    DBC_REQUIRE(232, cols > INPUT_COMPOSER_PROMPT_COLS_);

    DBC_ALLEGE(305, ncplane_move_yx(composer->plane, y, 2) == 0);
    DBC_ALLEGE(306, ncplane_resize_simple(
        composer->plane, 1U, cols) == 0);
    InputComposer_drawPrompt_(composer);
}

void InputComposer_showCursor(
    struct InputComposer * const composer,
    char const * const text,
    size_t const len,
    size_t const editPos)
{
    DBC_REQUIRE(240, composer != (struct InputComposer *)0);
    DBC_REQUIRE(241, composer->plane != (struct ncplane *)0);
    InputComposer_validateProjection_(text, len, editPos);

    composer->cursorVisible = true;
    (void)InputComposer_updateView_(composer, text, len, editPos);
    InputComposer_drawAll_(composer, text, len, editPos);
}

void InputComposer_hideCursor(
    struct InputComposer * const composer,
    char const * const text,
    size_t const len,
    size_t const editPos)
{
    DBC_REQUIRE(245, composer != (struct InputComposer *)0);
    DBC_REQUIRE(246, composer->plane != (struct ncplane *)0);
    InputComposer_validateProjection_(text, len, editPos);

    composer->cursorVisible = false;
    (void)InputComposer_updateView_(composer, text, len, editPos);
    InputComposer_drawAll_(composer, text, len, editPos);
}

//============================================================================
//=== Projection contract

void InputComposer_projectAll(
    struct InputComposer * const composer,
    char const * const text,
    size_t const len,
    size_t const editPos)
{
    DBC_REQUIRE(250, composer != (struct InputComposer *)0);
    DBC_REQUIRE(251, composer->plane != (struct ncplane *)0);
    InputComposer_validateProjection_(text, len, editPos);

    composer->viewStart = 0U;
    (void)InputComposer_updateView_(composer, text, len, editPos);
    InputComposer_drawAll_(composer, text, len, editPos);
}

void InputComposer_projectFrom(
    struct InputComposer * const composer,
    char const * const text,
    size_t const len,
    size_t const editPos,
    size_t const dirtyPos)
{
    DBC_REQUIRE(260, composer != (struct InputComposer *)0);
    DBC_REQUIRE(261, composer->plane != (struct ncplane *)0);
    InputComposer_validateProjection_(text, len, editPos);
    DBC_REQUIRE(262, dirtyPos <= len);
    DBC_REQUIRE(263, dirtyPos <= editPos);
    DBC_REQUIRE(264, (dirtyPos == len)
                     || (((unsigned char)text[dirtyPos] & 0xC0U) != 0x80U));

    bool const invalidatesView = dirtyPos < composer->viewStart;
    if (invalidatesView) {
        composer->viewStart = 0U;
    }
    bool const viewChanged = InputComposer_updateView_(
        composer, text, len, editPos);
    if (invalidatesView || viewChanged
        || (dirtyPos < composer->viewStart))
    {
        InputComposer_drawAll_(composer, text, len, editPos);
        return;
    }

    unsigned const column = InputComposer_width_(
        text, composer->viewStart, dirtyPos);
    InputComposer_drawFrom_(composer, text, len, dirtyPos, column);
    InputComposer_drawCursor_(composer, len, editPos);
}

void InputComposer_moveCursor(
    struct InputComposer * const composer,
    char const * const text,
    size_t const len,
    size_t const editPos)
{
    DBC_REQUIRE(270, composer != (struct InputComposer *)0);
    DBC_REQUIRE(271, composer->plane != (struct ncplane *)0);
    InputComposer_validateProjection_(text, len, editPos);

    (void)InputComposer_updateView_(composer, text, len, editPos);
    InputComposer_drawAll_(composer, text, len, editPos);
}
