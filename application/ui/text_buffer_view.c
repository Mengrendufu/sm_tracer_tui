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
#include "dbc_assert.h"
#include "text_buffer_view.h"
DBC_MODULE_NAME("text_buffer_view")

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
        char const * const nl = (char const *)memchr(p, '\n', (size_t)(end - p));
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
    ScrollBar_Thumb t = { .height = 0, .pos = 0, .visible = false, .maxOff = 0 };

    if (nTotal <= nVisible) { return t; }

    int32_t const maxOff = (int32_t)(nTotal - nVisible);
    if ((int32_t)scrollOff > maxOff) { scrollOff = (uint32_t)maxOff; }

    uint32_t thumb = (uint64_t)nVisible * nVisible / (uint64_t)nTotal;
    if (thumb < 1U) { thumb = 1U; }
    if (thumb > nVisible) { thumb = nVisible; }

    uint32_t const slop = nVisible - thumb;
    uint32_t const pos = (slop > 0)
        ? (uint32_t)((uint64_t)(maxOff - (int32_t)scrollOff) * slop / (uint64_t)maxOff)
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

    if (thumb->visible && row >= thumb->pos && row < thumb->pos + thumb->height) {
        return 0x2588U;  // full block
    }
    return thumb->maxOff > 0 ? 0x2591U : 0;  // light shade or space
}

//============================================================================
//=== TextBufferView operations

void TextBufferView_init(struct TextBufferView * const view) {
    DBC_REQUIRE(400, view != (struct TextBufferView *)0);

    view->framePlane = (struct ncplane *)0;
    view->contentPlane = (struct ncplane *)0;
    TextArea_init(&view->textArea);
    ScrollBar_init(&view->scrollBar);
}
