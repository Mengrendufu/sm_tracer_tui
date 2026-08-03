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
#define INPUT_COMPOSER_MAX_ROWS_    4U

#define INPUT_COMPOSER_WARNING_R_ 220U
#define INPUT_COMPOSER_WARNING_G_  90U
#define INPUT_COMPOSER_WARNING_B_ 100U

typedef struct {
    unsigned totalRows;
    unsigned cursorRow;
    unsigned cursorCol;
} InputComposer_Layout_;

//============================================================================
//=== Projection internals

static size_t InputComposer_loadEgc_(
    struct InputComposer const * const composer,
    char const * const text,
    unsigned * const cols)
{
    nccell cell = NCCELL_TRIVIAL_INITIALIZER;
    int const loaded = nccell_load(composer->plane, &cell, text);
    DBC_ASSERT(500, loaded > 0);
    *cols = nccell_cols(&cell);
    nccell_release(composer->plane, &cell);
    return (size_t)loaded;
}

static unsigned InputComposer_contentCols_(
    struct InputComposer const * const composer)
{
    unsigned const cols = ncplane_dim_x(composer->plane);
    DBC_ASSERT(510, cols > INPUT_COMPOSER_PROMPT_COLS_);
    return cols - INPUT_COMPOSER_PROMPT_COLS_;
}

static void InputComposer_advance_(unsigned * const row,
                                   unsigned * const col,
                                   unsigned const egcCols,
                                   unsigned const contentCols)
{
    if ((*col > 0U) && ((*col + egcCols) > contentCols)) {
        ++(*row);
        *col = 0U;
    }

    if (egcCols >= contentCols) {
        *col = contentCols;
    } else {
        *col += egcCols;
    }
}

static unsigned InputComposer_prefixCols_(char const * const text,
                                          size_t const bytes)
{
    if (bytes == 0U) {
        return 0U;
    }

    char prefix[bytes + 1U];
    memcpy(prefix, text, bytes);
    prefix[bytes] = '\0';
    int const columns = ncstrwidth(prefix, (int *)0, (int *)0);
    DBC_ASSERT(503, columns >= 0);
    return (unsigned)columns;
}

static InputComposer_Layout_ InputComposer_layout_(
    struct InputComposer const * const composer,
    char const * const text,
    size_t const len,
    size_t const editPos,
    unsigned const contentCols)
{
    unsigned row = 0U;
    unsigned col = 0U;
    InputComposer_Layout_ layout = {0};
    bool cursorLocated = false;

    size_t offset = 0U;
    while (offset < len) {
        unsigned egcCols;
        size_t const size = InputComposer_loadEgc_(
            composer, &text[offset], &egcCols);
        DBC_ASSERT(501, size <= (len - offset));

        if ((col > 0U) && ((col + egcCols) > contentCols)) {
            ++row;
            col = 0U;
        }
        if ((editPos >= offset) && (editPos < (offset + size))) {
            layout.cursorRow = row;
            layout.cursorCol = col + InputComposer_prefixCols_(
                &text[offset], editPos - offset);
            if (layout.cursorCol >= contentCols) {
                ++layout.cursorRow;
                layout.cursorCol = 0U;
            }
            cursorLocated = true;
        }

        InputComposer_advance_(&row, &col, egcCols, contentCols);
        offset += size;
    }

    if (editPos == len) {
        InputComposer_advance_(&row, &col, 1U, contentCols);
        layout.cursorRow = row;
        layout.cursorCol = (col == 0U) ? 0U : (col - 1U);
        cursorLocated = true;
    }

    DBC_ASSERT(502, cursorLocated);
    (void)cursorLocated;
    layout.totalRows = row + 1U;
    if (layout.totalRows <= layout.cursorRow) {
        layout.totalRows = layout.cursorRow + 1U;
    }
    return layout;
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
    if ((composer->viewRow != 0U) && !composer->limitReached) {
        return;
    }

    ncplane_set_bg_rgb8(composer->plane, 38, 38, 42);
    if (composer->limitReached) {
        ncplane_set_fg_rgb8(composer->plane,
                            INPUT_COMPOSER_WARNING_R_,
                            INPUT_COMPOSER_WARNING_G_,
                            INPUT_COMPOSER_WARNING_B_);
    } else {
        ncplane_set_fg_rgb8(composer->plane, 105, 185, 170);
    }
    ncplane_on_styles(composer->plane, NCSTYLE_BOLD);
    (void)WidgetIO_putStrYx(composer->plane, 0, 0U,
                            composer->limitReached ? "! " : "> ");
    ncplane_off_styles(composer->plane, NCSTYLE_BOLD);
    ncplane_set_fg_rgb8(composer->plane, 225, 230, 232);
}

static void InputComposer_updateView_(
    struct InputComposer * const composer,
    char const * const text,
    size_t const len,
    size_t const editPos)
{
    unsigned const contentCols = InputComposer_contentCols_(composer);
    unsigned const visibleRows = ncplane_dim_y(composer->plane);
    InputComposer_Layout_ const layout = InputComposer_layout_(
        composer, text, len, editPos, contentCols);
    unsigned const maxViewRow =
        (layout.totalRows > visibleRows)
        ? (layout.totalRows - visibleRows) : 0U;

    if (composer->viewRow > maxViewRow) {
        composer->viewRow = maxViewRow;
    }
    if (layout.cursorRow < composer->viewRow) {
        composer->viewRow = layout.cursorRow;
    } else if (layout.cursorRow >= (composer->viewRow + visibleRows)) {
        composer->viewRow = layout.cursorRow - visibleRows + 1U;
    }

    composer->cursorRow = layout.cursorRow - composer->viewRow;
    composer->cursorCol = layout.cursorCol;
    DBC_ASSERT(520, composer->cursorRow < visibleRows);
    DBC_ASSERT(521, composer->cursorCol < contentCols);
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
            (int)composer->cursorRow,
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
    if (composer->limitReached) {
        nccell_set_bg_rgb8(&cursor,
                           INPUT_COMPOSER_WARNING_R_,
                           INPUT_COMPOSER_WARNING_G_,
                           INPUT_COMPOSER_WARNING_B_);
        nccell_set_fg_rgb8(&cursor, 255U, 255U, 255U);
    }

    int const columns = ncplane_putc_yx(
        composer->plane,
        (int)composer->cursorRow,
        (int)(INPUT_COMPOSER_PROMPT_COLS_ + composer->cursorCol),
        &cursor);
    DBC_ALLEGE(300, columns == (int)nccell_cols(&cursor));
    (void)columns;
    nccell_release(composer->plane, &cursor);
}

static void InputComposer_drawContent_(
    struct InputComposer * const composer,
    char const * const text,
    size_t const len)
{
    unsigned const contentCols = InputComposer_contentCols_(composer);
    unsigned const visibleRows = ncplane_dim_y(composer->plane);
    size_t offset = 0U;
    unsigned row = 0U;
    unsigned col = 0U;

    while (offset < len) {
        unsigned egcCols;
        size_t const size = InputComposer_loadEgc_(
            composer, &text[offset], &egcCols);
        DBC_ASSERT(530, size <= (len - offset));
        if ((col > 0U) && ((col + egcCols) > contentCols)) {
            ++row;
            col = 0U;
        }

        if ((row >= composer->viewRow)
            && ((row - composer->viewRow) < visibleRows)
            && (egcCols <= contentCols))
        {
            size_t bytes = 0U;
            int const columns = ncplane_putegc_yx(
                composer->plane,
                (int)(row - composer->viewRow),
                (int)(INPUT_COMPOSER_PROMPT_COLS_ + col),
                &text[offset],
                &bytes);
            DBC_ALLEGE(301, columns >= 0);
            DBC_ASSERT(532, bytes == size);
            (void)columns;
        }

        InputComposer_advance_(&row, &col, egcCols, contentCols);
        offset += size;
    }
}

static void InputComposer_drawAll_(
    struct InputComposer * const composer,
    char const * const text,
    size_t const len,
    size_t const editPos)
{
    InputComposer_drawPrompt_(composer);
    InputComposer_drawContent_(composer, text, len);
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
    composer->viewRow = 0U;
    composer->cursorRow = 0U;
    composer->cursorCol = 0U;
    composer->cursorVisible = false;
    composer->limitReached = false;
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
    composer->viewRow = 0U;
    composer->cursorRow = 0U;
    composer->cursorCol = 0U;
    composer->cursorVisible = false;
    composer->limitReached = false;
}

void InputComposer_setLimitReached(
    struct InputComposer * const composer,
    bool const reached)
{
    DBC_REQUIRE(225, composer != (struct InputComposer *)0);
    DBC_REQUIRE(226, composer->plane != (struct ncplane *)0);
    composer->limitReached = reached;
}

void InputComposer_resize(struct InputComposer * const composer,
                          int const y,
                          unsigned const rows,
                          unsigned const cols)
{
    DBC_REQUIRE(230, composer != (struct InputComposer *)0);
    DBC_REQUIRE(231, composer->plane != (struct ncplane *)0);
    DBC_REQUIRE(232, cols > INPUT_COMPOSER_PROMPT_COLS_);
    DBC_REQUIRE(233, (rows > 0U)
                     && (rows <= INPUT_COMPOSER_MAX_ROWS_));

    unsigned const oldRows = ncplane_dim_y(composer->plane);
    if (rows > oldRows) {
        DBC_ALLEGE(305, ncplane_move_yx(composer->plane, y, 2) == 0);
        DBC_ALLEGE(306, ncplane_resize_simple(
            composer->plane, rows, cols) == 0);
    } else {
        DBC_ALLEGE(307, ncplane_resize_simple(
            composer->plane, rows, cols) == 0);
        DBC_ALLEGE(308, ncplane_move_yx(composer->plane, y, 2) == 0);
    }
    InputComposer_drawPrompt_(composer);
}

unsigned InputComposer_preferredRows(
    struct InputComposer const * const composer,
    char const * const text,
    size_t const len,
    size_t const editPos,
    unsigned const cols)
{
    DBC_REQUIRE(234, composer != (struct InputComposer const *)0);
    DBC_REQUIRE(235, composer->plane != (struct ncplane *)0);
    InputComposer_validateProjection_(text, len, editPos);
    DBC_REQUIRE(236, cols > INPUT_COMPOSER_PROMPT_COLS_);

    InputComposer_Layout_ const layout = InputComposer_layout_(
        composer, text, len, editPos,
        cols - INPUT_COMPOSER_PROMPT_COLS_);
    return (layout.totalRows < INPUT_COMPOSER_MAX_ROWS_)
           ? layout.totalRows : INPUT_COMPOSER_MAX_ROWS_;
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
    InputComposer_updateView_(composer, text, len, editPos);
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
    InputComposer_updateView_(composer, text, len, editPos);
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

    composer->viewRow = 0U;
    InputComposer_updateView_(composer, text, len, editPos);
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

    (void)dirtyPos;
    InputComposer_updateView_(composer, text, len, editPos);
    InputComposer_drawAll_(composer, text, len, editPos);
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

    InputComposer_updateView_(composer, text, len, editPos);
    InputComposer_drawAll_(composer, text, len, editPos);
}
