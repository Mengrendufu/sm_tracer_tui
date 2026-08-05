//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#include <stdio.h>
#include <string.h>
#include <notcurses/notcurses.h>
#include "dbc_assert.h"
#include "text_buffer_view.h"
DBC_MODULE_NAME("text_buffer_view")

#define TEXT_BUFFER_SYS_INFO_PREFIX_ "[SYS_INFO]> "

//============================================================================
//--- Private declarations

// Component IO helpers:
// - frame IO owns frame plane style and border rendering.
// - content IO owns content plane style, text repaint, and scrollbar repaint.
static struct ncplane *TextBufferView_frame_create_(
                           struct ncplane *stdPlane,
                           void *owner,
                           int y,
                           unsigned rows,
                           unsigned cols,
                           uint64_t borderCh,
                           TextBufferView_ResizeCb resizeCb);
static void            TextBufferView_frame_setBase_(
                           struct ncplane *framePlane);
static void            TextBufferView_frame_drawBorder_(
                           struct ncplane *framePlane,
                           unsigned rows,
                           unsigned cols,
                           uint64_t borderCh);
static void            TextBufferView_plane_setMainStyle_(
                           struct ncplane *plane);
static void            TextBufferView_plane_setMainBase_(
                           struct ncplane *plane);
static struct ncplane *TextBufferView_content_create_(
                           struct ncplane *framePlane,
                           void *owner,
                           unsigned rows,
                           unsigned cols,
                           TextBufferView_ResizeCb resizeCb);
static void            TextBufferView_content_setBase_(
                           struct ncplane *contentPlane);
static void            TextBufferView_content_clear_(
                           struct ncplane *contentPlane);
static void            TextBufferView_content_drawText_(
                           struct TextBufferView *view,
                           uint32_t rows,
                           uint32_t cols);
static void            TextBufferView_content_drawScrollBar_(
                           struct ncplane *contentPlane,
                           int x,
                           ScrollBar const *bar,
                           ScrollBar_Thumb const *thumb);
static bool            TextBufferView_content_canPutStr_(
                           struct ncplane *contentPlane,
                           unsigned x,
                           char const *text);
static bool            TextBufferView_content_putStrYx_(
                           struct ncplane *contentPlane,
                           int y,
                           unsigned x,
                           char const *text);

// Interaction handlers:
// - create wires frame and content as one view component.
// - refresh couples TextArea data, content repaint, and ScrollBar thumb data.
static void            TextBufferView_frame_content_create_(
                           struct TextBufferView *view,
                           struct ncplane *stdPlane,
                           void *owner,
                           int y,
                           unsigned rows,
                           unsigned cols,
                           uint64_t borderCh,
                           TextBufferView_ResizeCb frameCb,
                           TextBufferView_ResizeCb contentCb);
static void            TextBufferView_content_text_scrollBar_refresh_(
                           struct TextBufferView *view,
                           struct ncplane *contentPlane);

//============================================================================
//=== TextArea internals

static int32_t TextArea_maxScroll_(struct TextArea const * const ta,
                                   uint32_t const visibleRows)
{
    DBC_REQUIRE(100, ta != (struct TextArea const *)0);
    return (ta->lineTotal > visibleRows)
           ? (int32_t)(ta->lineTotal - visibleRows)
           : 0;
}

static void TextArea_clampScroll_(struct TextArea * const ta,
                                  uint32_t const visibleRows)
{
    DBC_REQUIRE(110, ta != (struct TextArea *)0);

    int32_t const maxScroll = TextArea_maxScroll_(ta, visibleRows);
    if (ta->scrollOff > maxScroll) {
        ta->scrollOff = maxScroll;
    }
    if (ta->scrollOff < 0) {
        ta->scrollOff = 0;
    }
}

//============================================================================
//=== TextArea operations

void TextArea_init(struct TextArea * const ta) {
    DBC_REQUIRE(200, ta != (struct TextArea *)0);

    ta->lineTotal = 0U;
    ta->lineHead  = 0U;
    ta->scrollOff = 0;
}

void TextArea_clear(struct TextArea * const ta) {
    DBC_REQUIRE(210, ta != (struct TextArea *)0);
    TextArea_init(ta);
}

uint32_t TextArea_push(struct TextArea * const ta,
                       char const * const text, size_t const len)
{
    DBC_REQUIRE(220, ta != (struct TextArea *)0);
    DBC_REQUIRE(221, text != (char const *)0);

    if (len == 0U) {
        return 0U;
    }

    uint32_t written = 0U;
    char const *p = text;
    char const * const end = text + len;

    while (p < end) {
        size_t const span = (size_t)(end - p);
        char const * const nl = (char const *)memchr(p, '\n', span);
        size_t const segLen = nl ? (size_t)(nl - p) : (size_t)(end - p);

        uint32_t slot;
        if (ta->lineTotal >= TEXT_AREA_CAP_) {
            slot = ta->lineHead;
            ta->lineHead = (ta->lineHead + 1U) % TEXT_AREA_CAP_;
        } else {
            slot = (ta->lineHead + ta->lineTotal) % TEXT_AREA_CAP_;
            ++ta->lineTotal;
        }

        size_t const copyLen = (segLen < TEXT_LINE_W_ - 1U)
                               ? segLen
                               : (TEXT_LINE_W_ - 1U);
        memcpy(ta->lineBuf[slot], p, copyLen);
        ta->lineBuf[slot][copyLen] = '\0';
        ++written;

        p = nl ? (nl + 1) : end;
    }

    return written;
}

void TextArea_preserveScrollOnAppend(struct TextArea * const ta,
                                     uint32_t const addedLines)
{
    DBC_REQUIRE(230, ta != (struct TextArea *)0);

    if (ta->scrollOff > 0) {
        ta->scrollOff += (int32_t)addedLines;
    }
}

void TextArea_scrollBy(struct TextArea * const ta, int32_t const delta,
                       uint32_t const visibleRows)
{
    DBC_REQUIRE(240, ta != (struct TextArea *)0);

    ta->scrollOff += delta;
    TextArea_clampScroll_(ta, visibleRows);
}

uint32_t TextArea_firstVisible(struct TextArea const * const ta,
                               uint32_t const visibleRows)
{
    DBC_REQUIRE(250, ta != (struct TextArea const *)0);

    int32_t const maxScroll = TextArea_maxScroll_(ta, visibleRows);
    int32_t scrollOff = ta->scrollOff;
    if (scrollOff > maxScroll) {
        scrollOff = maxScroll;
    }
    if (scrollOff < 0) {
        scrollOff = 0;
    }

    return (ta->lineTotal > visibleRows)
           ? ta->lineTotal - visibleRows - (uint32_t)scrollOff
           : 0U;
}

char const *TextArea_lineAt(struct TextArea const * const ta,
                            uint32_t const logicalIdx)
{
    DBC_REQUIRE(260, ta != (struct TextArea const *)0);
    DBC_REQUIRE(261, logicalIdx < ta->lineTotal);

    uint32_t const physIdx = (ta->lineHead + logicalIdx) % TEXT_AREA_CAP_;
    return ta->lineBuf[physIdx];
}

uint32_t TextArea_total(struct TextArea const * const ta) {
    DBC_REQUIRE(270, ta != (struct TextArea const *)0);
    return ta->lineTotal;
}

int32_t TextArea_scrollOffset(struct TextArea const * const ta) {
    DBC_REQUIRE(280, ta != (struct TextArea const *)0);
    return ta->scrollOff;
}

//============================================================================
//=== ScrollBar operations

void ScrollBar_init(ScrollBar * const bar) {
    DBC_REQUIRE(300, bar != (ScrollBar *)0);

    bar->enabled = true;
    bar->style.thumb.r = 140U;
    bar->style.thumb.g = 140U;
    bar->style.thumb.b = 200U;
    bar->style.track.r = 60U;
    bar->style.track.g = 60U;
    bar->style.track.b = 100U;
}

ScrollBar_Thumb ScrollBar_computeThumb(uint32_t const nTotal,
                                        uint32_t const nVisible,
                                        uint32_t scrollOff)
{
    ScrollBar_Thumb t = {
        .height = 0,
        .pos = 0,
        .visible = false,
        .maxOff = 0
    };

    if (nTotal <= nVisible) { return t; }

    int32_t const maxOff = (int32_t)(nTotal - nVisible);
    if ((int32_t)scrollOff > maxOff) { scrollOff = (uint32_t)maxOff; }

    uint32_t thumb = (uint64_t)nVisible * nVisible / (uint64_t)nTotal;
    if (thumb < 1U) { thumb = 1U; }
    if (thumb > nVisible) { thumb = nVisible; }

    uint32_t const slop = nVisible - thumb;
    uint32_t const pos = (slop > 0)
        ? (uint32_t)((uint64_t)(maxOff - (int32_t)scrollOff)
                     * slop / (uint64_t)maxOff)
        : 0U;

    t.height  = thumb;
    t.pos     = pos;
    t.visible = true;
    t.maxOff  = maxOff;
    return t;
}

uint32_t ScrollBar_cellCh(ScrollBar_Thumb const * const thumb,
                          uint32_t const row)
{
    DBC_REQUIRE(310, thumb != (ScrollBar_Thumb const *)0);

    bool const inThumb = thumb->visible
                         && row >= thumb->pos
                         && row < thumb->pos + thumb->height;
    if (inThumb) {
        return 0x2588U;  // full block
    }
    return thumb->maxOff > 0 ? 0x2591U : 0;  // light shade or space
}

//============================================================================
//=== Component IO helpers
//
// These helpers operate on frame/content/scrollbar primitives only. They do
// not know HSM transitions, UI dirty state, or the parent SM_UI display
// graph.

static struct ncplane *TextBufferView_frame_create_(
    struct ncplane * const stdPlane,
    void * const owner,
    int const y,
    unsigned const rows,
    unsigned const cols,
    uint64_t const borderCh,
    TextBufferView_ResizeCb const resizeCb)
{
    DBC_REQUIRE(410, stdPlane != (struct ncplane *)0);

    ncplane_options nopts = {
        .y = y, .x = 2, .rows = rows, .cols = cols, .name = "main",
        .userptr = owner, .resizecb = resizeCb,
    };
    struct ncplane * const framePlane = ncplane_create(stdPlane, &nopts);
    DBC_ENSURE(450, framePlane != (struct ncplane *)0);

    TextBufferView_frame_setBase_(framePlane);
    TextBufferView_frame_drawBorder_(framePlane, rows, cols, borderCh);
    return framePlane;
}

static void TextBufferView_frame_setBase_(
    struct ncplane * const framePlane)
{
    DBC_REQUIRE(411, framePlane != (struct ncplane *)0);
    TextBufferView_plane_setMainBase_(framePlane);
}

static void TextBufferView_frame_drawBorder_(
    struct ncplane * const framePlane,
    unsigned const rows,
    unsigned const cols,
    uint64_t const borderCh)
{
    DBC_REQUIRE(412, framePlane != (struct ncplane *)0);
    ncplane_ascii_box(framePlane, 0, borderCh, rows, cols, 0);
}

static void TextBufferView_plane_setMainStyle_(
    struct ncplane * const plane)
{
    DBC_REQUIRE(424, plane != (struct ncplane *)0);

    ncplane_set_bg_rgb8(plane, 32, 32, 34);
    ncplane_set_fg_rgb8(plane, 200, 220, 200);
}

static void TextBufferView_plane_setMainBase_(
    struct ncplane * const plane)
{
    DBC_REQUIRE(425, plane != (struct ncplane *)0);

    TextBufferView_plane_setMainStyle_(plane);

    nccell base = NCCELL_TRIVIAL_INITIALIZER;
    nccell_set_bg_rgb8(&base, 32, 32, 34);
    nccell_set_fg_rgb8(&base, 200, 220, 200);
    nccell_load_char(plane, &base, ' ');
    ncplane_set_base_cell(plane, &base);
    nccell_release(plane, &base);
}

static struct ncplane *TextBufferView_content_create_(
    struct ncplane * const framePlane,
    void * const owner,
    unsigned const rows,
    unsigned const cols,
    TextBufferView_ResizeCb const resizeCb)
{
    DBC_REQUIRE(413, framePlane != (struct ncplane *)0);

    unsigned const contentRows = (rows > 0U) ? rows : 1U;
    unsigned const contentCols = (cols > 1U) ? (cols - 1U) : 1U;
    ncplane_options nopts = {
        .y = 0, .x = 1, .rows = contentRows, .cols = contentCols,
        .name = "mainContent",
        .userptr = owner, .resizecb = resizeCb,
    };
    struct ncplane * const contentPlane = ncplane_create(framePlane, &nopts);
    DBC_ENSURE(451, contentPlane != (struct ncplane *)0);

    TextBufferView_content_setBase_(contentPlane);
    return contentPlane;
}

static void TextBufferView_content_setBase_(
    struct ncplane * const contentPlane)
{
    DBC_REQUIRE(414, contentPlane != (struct ncplane *)0);
    TextBufferView_plane_setMainBase_(contentPlane);
}

static void TextBufferView_content_clear_(
    struct ncplane * const contentPlane)
{
    DBC_REQUIRE(415, contentPlane != (struct ncplane *)0);

    TextBufferView_plane_setMainStyle_(contentPlane);
    ncplane_erase(contentPlane);
}

static bool TextBufferView_content_canPutStr_(
    struct ncplane * const contentPlane,
    unsigned const x,
    char const * const text)
{
    DBC_REQUIRE(426, contentPlane != (struct ncplane *)0);
    DBC_REQUIRE(427, text != (char const *)0);

    unsigned cols;
    ncplane_dim_yx(contentPlane, NULL, &cols);

    int const width = ncstrwidth(text, NULL, NULL);
    if (width < 0) {
        return false;
    }

    return x < cols && (unsigned)width <= (cols - x);
}

static bool TextBufferView_content_putStrYx_(
    struct ncplane * const contentPlane,
    int const y,
    unsigned const x,
    char const * const text)
{
    DBC_REQUIRE(428, contentPlane != (struct ncplane *)0);
    DBC_REQUIRE(429, text != (char const *)0);

    if (!TextBufferView_content_canPutStr_(contentPlane, x, text)) {
        return false;
    }

    return ncplane_putstr_yx(contentPlane, y, (int)x, text) >= 0;
}

static void TextBufferView_content_drawText_(
    struct TextBufferView * const view,
    uint32_t const rows,
    uint32_t const cols)
{
    DBC_REQUIRE(416, view != (struct TextBufferView *)0);
    DBC_REQUIRE(417, view->contentPlane != (struct ncplane *)0);

    TextArea_scrollBy(&view->textArea, 0, rows);
    uint32_t const start = TextArea_firstVisible(&view->textArea, rows);

    for (uint32_t i = 0U; i < rows; ++i) {
        uint32_t const bufIdx = start + i;
        if (cols > 1U && bufIdx < TextArea_total(&view->textArea)) {
            char const * const line =
                TextArea_lineAt(&view->textArea, bufIdx);
            char textLine[TEXT_LINE_W_];
            int const tw = (int)(cols - 1U);
            (void)snprintf(textLine, sizeof(textLine), "%-*.*s",
                           tw, tw, line);
            TextBufferView_plane_setMainStyle_(view->contentPlane);
            if (strncmp(line, TEXT_BUFFER_SYS_INFO_PREFIX_,
                        sizeof(TEXT_BUFFER_SYS_INFO_PREFIX_) - 1U) == 0)
            {
                ncplane_set_fg_rgb8(view->contentPlane, 105, 185, 170);
            }
            (void)TextBufferView_content_putStrYx_(
                view->contentPlane, (int)i, 0U, textLine);
        }
    }
}

static void TextBufferView_content_drawScrollBar_(
    struct ncplane * const contentPlane,
    int const x,
    ScrollBar const * const bar,
    ScrollBar_Thumb const * const thumb)
{
    if (!contentPlane) { return; }
    if (!bar) { return; }
    if (!thumb) { return; }
    if (!bar->enabled) { return; }

    unsigned rows;
    unsigned cols;
    ncplane_dim_yx(contentPlane, &rows, &cols);
    if ((int)x < 0 || (unsigned)x >= cols || rows == 0U) { return; }

    for (uint32_t i = 0U; i < rows; ++i) {
        uint32_t const ch = ScrollBar_cellCh(thumb, i);
        if (ch) {
            ScrollBar_Color const * const color = (ch == 0x2588U)
                                                  ? &bar->style.thumb
                                                  : &bar->style.track;
            ncplane_set_fg_rgb8(contentPlane, color->r, color->g, color->b);
            (void)TextBufferView_content_putStrYx_(
                contentPlane, (int)i, (unsigned)x,
                ch == 0x2588U ? "\xe2\x96\x88" : "\xe2\x96\x91");
        }
    }
}

//============================================================================
//=== Interaction handlers
//
// These handlers express TextBufferView coupling: std/frame/content geometry,
// TextArea repaint, and ScrollBar thumb projection into content.

static void TextBufferView_frame_content_create_(
    struct TextBufferView * const view,
    struct ncplane * const stdPlane,
    void * const owner,
    int const y,
    unsigned const rows,
    unsigned const cols,
    uint64_t const borderCh,
    TextBufferView_ResizeCb const frameCb,
    TextBufferView_ResizeCb const contentCb)
{
    DBC_REQUIRE(420, view != (struct TextBufferView *)0);
    DBC_REQUIRE(421, stdPlane != (struct ncplane *)0);

    view->framePlane = TextBufferView_frame_create_(
        stdPlane, owner, y, rows, cols, borderCh, frameCb);
    view->contentPlane = TextBufferView_content_create_(view->framePlane,
                                                        owner, rows, cols,
                                                        contentCb);
}

static void TextBufferView_content_text_scrollBar_refresh_(
    struct TextBufferView * const view,
    struct ncplane * const contentPlane)
{
    DBC_REQUIRE(422, view != (struct TextBufferView *)0);
    DBC_REQUIRE(423, contentPlane != (struct ncplane *)0);

    TextBufferView_content_clear_(contentPlane);

    unsigned rows;
    unsigned cols;
    ncplane_dim_yx(contentPlane, &rows, &cols);
    if (rows == 0U) { return; }

    TextBufferView_content_drawText_(view, rows, cols);

    if (cols > 0U) {
        ScrollBar_Thumb const thumb = ScrollBar_computeThumb(
            TextArea_total(&view->textArea),
            rows,
            (uint32_t)TextArea_scrollOffset(&view->textArea));
        TextBufferView_content_drawScrollBar_(contentPlane,
                                              (int)(cols - 1U),
                                              &view->scrollBar,
                                              &thumb);
    }
}

//============================================================================
//=== TextBufferView operations

void TextBufferView_init(struct TextBufferView * const view) {
    DBC_REQUIRE(400, view != (struct TextBufferView *)0);

    view->framePlane = (struct ncplane *)0;
    view->contentPlane = (struct ncplane *)0;
    TextArea_init(&view->textArea);
    ScrollBar_init(&view->scrollBar);
    view->dirty = false;
}

void TextBufferView_create(struct TextBufferView * const view,
                           struct ncplane * const stdPlane,
                           void * const owner,
                           int const y,
                           unsigned const rows,
                           unsigned const cols,
                           uint64_t const borderCh,
                           TextBufferView_ResizeCb const frameCb,
                           TextBufferView_ResizeCb const contentCb)
{
    DBC_REQUIRE(430, view != (struct TextBufferView *)0);

    TextBufferView_frame_content_create_(
        view, stdPlane, owner, y, rows, cols,
        borderCh, frameCb, contentCb);
}

void TextBufferView_pushText(struct TextBufferView * const view,
                             char const * const text,
                             size_t const len)
{
    DBC_REQUIRE(431, view != (struct TextBufferView *)0);
    DBC_REQUIRE(432, text != (char const *)0);

    uint32_t const written = TextArea_push(&view->textArea, text, len);
    TextArea_preserveScrollOnAppend(&view->textArea, written);
    view->dirty = true;
}

void TextBufferView_clear(struct TextBufferView * const view) {
    DBC_REQUIRE(433, view != (struct TextBufferView *)0);
    TextArea_clear(&view->textArea);
    view->dirty = true;
}

void TextBufferView_scrollPageUp(struct TextBufferView * const view) {
    DBC_REQUIRE(434, view != (struct TextBufferView *)0);
    DBC_REQUIRE(435, view->contentPlane != (struct ncplane *)0);

    unsigned rows;
    ncplane_dim_yx(view->contentPlane, &rows, NULL);
    TextArea_scrollBy(&view->textArea, (int32_t)(rows / 2U), rows);
    view->dirty = true;
}

void TextBufferView_scrollPageDown(struct TextBufferView * const view) {
    DBC_REQUIRE(436, view != (struct TextBufferView *)0);
    DBC_REQUIRE(437, view->contentPlane != (struct ncplane *)0);

    unsigned rows;
    ncplane_dim_yx(view->contentPlane, &rows, NULL);
    TextArea_scrollBy(&view->textArea, -(int32_t)(rows / 2U), rows);
    view->dirty = true;
}

void TextBufferView_refresh(struct TextBufferView * const view) {
    DBC_REQUIRE(438, view != (struct TextBufferView *)0);
    if (!view->dirty || !view->contentPlane) { return; }

    TextBufferView_content_text_scrollBar_refresh_(view, view->contentPlane);
    view->dirty = false;
}

void TextBufferView_resizeFrame(struct TextBufferView * const view,
                                unsigned const rows,
                                unsigned const cols,
                                uint64_t const borderCh)
{
    DBC_REQUIRE(439, view != (struct TextBufferView *)0);
    DBC_REQUIRE(440, view->framePlane != (struct ncplane *)0);

    ncplane_resize_simple(view->framePlane, rows, cols);
    TextBufferView_frame_drawBorder_(view->framePlane, rows, cols, borderCh);
}

void TextBufferView_resizeContent(struct TextBufferView * const view) {
    DBC_REQUIRE(441, view != (struct TextBufferView *)0);
    DBC_REQUIRE(442, view->framePlane != (struct ncplane *)0);
    DBC_REQUIRE(443, view->contentPlane != (struct ncplane *)0);

    unsigned rows;
    unsigned cols;
    ncplane_dim_yx(view->framePlane, &rows, &cols);
    uint32_t const contentRows = (rows > 0U) ? rows : 1U;
    uint32_t const contentCols = (cols > 1U) ? (cols - 1U) : 1U;
    ncplane_resize_simple(view->contentPlane, contentRows, contentCols);
    view->dirty = true;
}
