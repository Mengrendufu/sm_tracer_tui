//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#ifndef MENU_WIDGET_H_
#define MENU_WIDGET_H_

#include <stdbool.h>
#include <stdint.h>

struct ncplane;

typedef enum {
    MENU_ACT_RESUME,
    MENU_ACT_CLEAR,
    MENU_ACT_ABOUT,
    MENU_ACT_QUIT
} MenuAction;

struct Menu {
    struct ncplane *plane;
    uint32_t sel;
    bool visible;
};

typedef int (*Menu_ResizeCb)(struct ncplane *plane);

void Menu_init(struct Menu *menu);
void Menu_create(struct Menu *menu,
                 struct ncplane *parent,
                 void *owner,
                 Menu_ResizeCb resizeCb);
void Menu_show(struct Menu *menu, struct ncplane *parent);
void Menu_hide(struct Menu *menu, struct ncplane *parent);
void Menu_syncLayout(struct Menu *menu, struct ncplane *parent);
void Menu_selectNext(struct Menu *menu);
void Menu_selectPrev(struct Menu *menu);
MenuAction Menu_action(struct Menu const *menu);
bool Menu_isVisible(struct Menu const *menu);

#endif // MENU_WIDGET_H_
