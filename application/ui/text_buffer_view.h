//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#ifndef TEXT_BUFFER_VIEW_H_
#define TEXT_BUFFER_VIEW_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct ncplane;

//============================================================================
//=== Text area data: circular line buffer + scroll model.
#define TEXT_AREA_CAP_  1000U
#define TEXT_LINE_W_    512U

struct TextArea {
    char     lineBuf[TEXT_AREA_CAP_][TEXT_LINE_W_];
    uint32_t lineTotal;
    uint32_t lineHead;
    int32_t  scrollOff;
};

void TextArea_init(struct TextArea *ta);
void TextArea_clear(struct TextArea *ta);

uint32_t TextArea_push(struct TextArea *ta, char const *text, size_t len);
void     TextArea_preserveScrollOnAppend(struct TextArea *ta,
                                         uint32_t addedLines);

void     TextArea_scrollBy(struct TextArea *ta, int32_t delta,
                           uint32_t visibleRows);
uint32_t TextArea_firstVisible(struct TextArea const *ta,
                               uint32_t visibleRows);
char const *TextArea_lineAt(struct TextArea const *ta, uint32_t logicalIdx);
uint32_t TextArea_total(struct TextArea const *ta);
int32_t  TextArea_scrollOffset(struct TextArea const *ta);

//============================================================================
//=== Scrollbar component data: configuration only, no rendering.

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} ScrollBar_Color;

typedef struct {
    ScrollBar_Color thumb;
    ScrollBar_Color track;
} ScrollBar_Style;

typedef struct {
    bool enabled;
    ScrollBar_Style style;
} ScrollBar;

void ScrollBar_init(ScrollBar *bar);

//============================================================================
//=== Thumb computation: pure math, no rendering.

typedef struct {
    uint32_t height;
    uint32_t pos;
    bool     visible;
    int32_t  maxOff;
} ScrollBar_Thumb;

ScrollBar_Thumb ScrollBar_computeThumb(uint32_t nTotal, uint32_t nVisible,
                                        uint32_t scrollOff);
uint32_t ScrollBar_cellCh(ScrollBar_Thumb const *thumb, uint32_t row);

//============================================================================
//=== Scrollable text buffer view: frame + content + data + scrollbar.

struct TextBufferView {
    struct ncplane *framePlane;
    struct ncplane *contentPlane;
    struct TextArea textArea;
    ScrollBar       scrollBar;
};

typedef int (*TextBufferView_ResizeCb)(struct ncplane *plane);

void TextBufferView_init(struct TextBufferView *view);
void TextBufferView_create(struct TextBufferView *view,
                           struct ncplane *stdPlane,
                           void *owner,
                           unsigned rows,
                           unsigned cols,
                           uint64_t borderCh,
                           TextBufferView_ResizeCb frameCb,
                           TextBufferView_ResizeCb contentCb);
void TextBufferView_pushText(struct TextBufferView *view,
                             char const *text, size_t len);
void TextBufferView_clear(struct TextBufferView *view);
void TextBufferView_scrollPageUp(struct TextBufferView *view);
void TextBufferView_scrollPageDown(struct TextBufferView *view);
void TextBufferView_refresh(struct TextBufferView *view);
void TextBufferView_resizeFrame(struct ncplane *framePlane,
                                unsigned rows,
                                unsigned cols,
                                uint64_t borderCh);
void TextBufferView_resizeContent(struct ncplane *framePlane,
                                  struct ncplane *contentPlane);

#endif // TEXT_BUFFER_VIEW_H_
