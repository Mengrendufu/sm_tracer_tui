//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#ifndef TITLE_BAR_H_
#define TITLE_BAR_H_

struct ncplane;

struct TitleBar {
    struct ncplane *plane;
};

typedef int (*TitleBar_ResizeCb)(struct ncplane *plane);

void TitleBar_init(struct TitleBar *bar);
void TitleBar_create(struct TitleBar *bar,
                     struct ncplane *parent,
                     void *owner,
                     unsigned cols,
                     TitleBar_ResizeCb resizeCb);
void TitleBar_resize(struct TitleBar *bar, unsigned cols);

#endif // TITLE_BAR_H_
