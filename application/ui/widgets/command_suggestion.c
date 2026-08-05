//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#include <stdbool.h>
#include <notcurses/notcurses.h>
#include "dbc_assert.h"
#include "selection_viewport_priv.h"
#include "command_suggestion.h"
DBC_MODULE_NAME("command_suggestion")

#define COMMAND_SUGGESTION_X_ 2
#define COMMAND_SUGGESTION_MAX_ROWS_ 6U

//============================================================================
static size_t CommandSuggestion_min_(size_t const lhs,
                                     size_t const rhs)
{
    return lhs < rhs ? lhs : rhs;
}

static void CommandSuggestion_drawRow_(
    struct CommandSuggestion const * const suggestion,
    unsigned const row,
    unsigned const cols,
    char const * const candidate,
    bool const selected)
{
    DBC_REQUIRE(100, suggestion != (struct CommandSuggestion const *)0);
    DBC_REQUIRE(101, suggestion->plane != (struct ncplane *)0);
    DBC_REQUIRE(102, candidate != (char const *)0);

    if (selected) {
        (void)ncplane_set_bg_rgb8(suggestion->plane, 80U, 80U, 160U);
        (void)ncplane_set_fg_rgb8(suggestion->plane, 255U, 255U, 255U);
    } else {
        (void)ncplane_set_bg_rgb8(suggestion->plane, 42U, 42U, 46U);
        (void)ncplane_set_fg_rgb8(suggestion->plane, 200U, 200U, 220U);
    }

    for (unsigned col = 0U; col < cols; ++col) {
        (void)ncplane_putchar_yx(suggestion->plane,
                                 (int)row, (int)col, ' ');
    }
    (void)ncplane_putstr_yx(suggestion->plane, (int)row, 1, candidate);
}

//============================================================================
void CommandSuggestion_init(
    struct CommandSuggestion * const suggestion)
{
    DBC_REQUIRE(200, suggestion != (struct CommandSuggestion *)0);
    suggestion->plane = (struct ncplane *)0;
    suggestion->firstVisible = 0U;
}

void CommandSuggestion_create(
    struct CommandSuggestion * const suggestion,
    struct ncplane * const parent)
{
    DBC_REQUIRE(210, suggestion != (struct CommandSuggestion *)0);
    DBC_REQUIRE(211, parent != (struct ncplane *)0);
    DBC_REQUIRE(212, suggestion->plane == (struct ncplane *)0);

    ncplane_options const opts = {
        .y = 0, .x = 0, .rows = 1U, .cols = 1U,
        .name = "command-suggestion",
    };
    suggestion->plane = ncplane_create(parent, &opts);
    DBC_ENSURE(300, suggestion->plane != (struct ncplane *)0);
    CommandSuggestion_hide(suggestion);
}

void CommandSuggestion_destroy(
    struct CommandSuggestion * const suggestion)
{
    DBC_REQUIRE(220, suggestion != (struct CommandSuggestion *)0);
    DBC_REQUIRE(221, suggestion->plane != (struct ncplane *)0);

    DBC_ALLEGE(301, ncplane_destroy(suggestion->plane) == 0);
    suggestion->plane = (struct ncplane *)0;
}

void CommandSuggestion_show(
    struct CommandSuggestion * const suggestion,
    int const inputY,
    unsigned const maxCols,
    char const * const candidates[],
    size_t const count,
    size_t const selected)
{
    DBC_REQUIRE(230, suggestion != (struct CommandSuggestion *)0);
    DBC_REQUIRE(231, suggestion->plane != (struct ncplane *)0);
    DBC_REQUIRE(232, candidates != (char const * const *)0);
    DBC_REQUIRE(233, count > 0U);
    DBC_REQUIRE(234, selected < count);

    int maxTextWidth = 0;
    for (size_t i = 0U; i < count; ++i) {
        DBC_REQUIRE(235, candidates[i] != (char const *)0);
        int const width = ncstrwidth(candidates[i], NULL, NULL);
        DBC_REQUIRE(236, width > 0);
        if (width > maxTextWidth) {
            maxTextWidth = width;
        }
    }

    struct ncplane * const parent = ncplane_parent(suggestion->plane);
    DBC_REQUIRE(237, parent != (struct ncplane *)0);

    unsigned parentRows;
    unsigned parentCols;
    ncplane_dim_yx(parent, &parentRows, &parentCols);
    if (inputY <= 0 || (unsigned)inputY >= parentRows
        || parentCols <= (unsigned)COMMAND_SUGGESTION_X_)
    {
        CommandSuggestion_hide(suggestion);
        return;
    }

    unsigned const availableCols =
        parentCols - (unsigned)COMMAND_SUGGESTION_X_;
    unsigned const colLimit = maxCols < availableCols
                              ? maxCols : availableCols;
    unsigned const naturalCols = (unsigned)maxTextWidth + 2U;
    unsigned const cols = naturalCols < colLimit
                          ? naturalCols : colLimit;
    if (cols < 3U) {
        CommandSuggestion_hide(suggestion);
        return;
    }

    size_t rows = CommandSuggestion_min_(
        count, COMMAND_SUGGESTION_MAX_ROWS_);
    rows = CommandSuggestion_min_(rows, (size_t)inputY);
    if (rows == 0U) {
        CommandSuggestion_hide(suggestion);
        return;
    }

    size_t const first = SelectionViewport_update(
        suggestion->firstVisible, selected, count, rows);
    suggestion->firstVisible = first;
    int const y = inputY - (int)rows;

    ncplane_erase(suggestion->plane);
    DBC_ALLEGE(302, ncplane_resize_simple(
        suggestion->plane, (unsigned)rows, cols) == 0);
    DBC_ALLEGE(303, ncplane_move_yx(
        suggestion->plane, y, COMMAND_SUGGESTION_X_) == 0);

    for (size_t row = 0U; row < rows; ++row) {
        size_t const candidate = first + row;
        CommandSuggestion_drawRow_(suggestion, (unsigned)row, cols,
                                   candidates[candidate],
                                   candidate == selected);
    }
    ncplane_move_top(suggestion->plane);
}

void CommandSuggestion_hide(
    struct CommandSuggestion * const suggestion)
{
    DBC_REQUIRE(240, suggestion != (struct CommandSuggestion *)0);
    DBC_REQUIRE(241, suggestion->plane != (struct ncplane *)0);

    suggestion->firstVisible = 0U;
    ncplane_erase(suggestion->plane);
    ncplane_move_bottom(suggestion->plane);
}
