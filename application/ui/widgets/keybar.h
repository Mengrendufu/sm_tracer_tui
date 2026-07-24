//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#ifndef KEYBAR_H_
#define KEYBAR_H_

struct ncplane;

struct Keybar {
    struct ncplane *plane;
};

typedef int (*Keybar_ResizeCb)(struct ncplane *plane);

void Keybar_init(struct Keybar *keybar);
void Keybar_create(struct Keybar *keybar,
                   struct ncplane *parent,
                   void *owner,
                   int y,
                   unsigned cols,
                   Keybar_ResizeCb resizeCb);
void Keybar_resize(struct Keybar *keybar, int y, unsigned cols);
void Keybar_showMainHints(struct Keybar *keybar);
void Keybar_showMenuHints(struct Keybar *keybar);

#endif // KEYBAR_H_
