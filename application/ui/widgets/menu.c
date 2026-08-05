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
#include <notcurses/notcurses.h>
#include "dbc_assert.h"
#include "selection_viewport_priv.h"
#include "widget_io_priv.h"
#include "menu.h"
DBC_MODULE_NAME("menu")

#define MENU_W_ 24U
#define MENU_NUM_ITEMS_ 4U
#define MENU_MAX_VISIBLE_ITEMS_ 6U
#define MENU_VISIBLE_ITEMS_ \
    ((MENU_NUM_ITEMS_ < MENU_MAX_VISIBLE_ITEMS_) \
     ? MENU_NUM_ITEMS_ : MENU_MAX_VISIBLE_ITEMS_)
#define MENU_H_ (MENU_VISIBLE_ITEMS_ + 2U)

static char const * const Menu_items_[MENU_NUM_ITEMS_] = {
    "Resume",
    "Clear screen",
    "About",
    "Quit"
};

//============================================================================
static void Menu_layout_(struct Menu * const menu,
                         struct ncplane * const parent)
{
    DBC_REQUIRE(100, menu != (struct Menu *)0);
    DBC_REQUIRE(101, menu->plane != (struct ncplane *)0);
    DBC_REQUIRE(102, parent != (struct ncplane *)0);

    unsigned dimY;
    unsigned dimX;
    ncplane_dim_yx(parent, &dimY, &dimX);

    int const y = (dimY > MENU_H_)
                  ? (int)((dimY - MENU_H_) / 2U) : 0;
    int const x = (dimX > MENU_W_)
                  ? (int)((dimX - MENU_W_) / 2U) : 0;
    ncplane_move_yx(menu->plane, y, x);

    unsigned rows;
    unsigned cols;
    ncplane_dim_yx(menu->plane, &rows, &cols);
    if (rows != MENU_H_ || cols != MENU_W_) {
        ncplane_resize_simple(menu->plane, MENU_H_, MENU_W_);
    }
}

static void Menu_drawItem_(struct Menu const * const menu,
                           uint32_t const idx,
                           uint32_t const row)
{
    DBC_REQUIRE(110, menu != (struct Menu const *)0);
    DBC_REQUIRE(111, menu->plane != (struct ncplane *)0);
    DBC_REQUIRE(112, idx < MENU_NUM_ITEMS_);
    DBC_REQUIRE(113, row < MENU_VISIBLE_ITEMS_);

    char line[32];
    (void)snprintf(line, sizeof(line), "| %-*s|",
                   (int)(MENU_W_ - 3U), Menu_items_[idx]);

    if (idx == menu->sel) {
        ncplane_set_bg_rgb8(menu->plane, 80, 80, 160);
        ncplane_set_fg_rgb8(menu->plane, 255, 255, 255);
    } else {
        ncplane_set_bg_rgb8(menu->plane, 50, 50, 100);
        ncplane_set_fg_rgb8(menu->plane, 200, 200, 220);
    }
    (void)WidgetIO_putStrYx(menu->plane, (int)(row + 1U), 0U, line);
}

static void Menu_draw_(struct Menu const * const menu) {
    DBC_REQUIRE(120, menu != (struct Menu const *)0);
    DBC_REQUIRE(121, menu->plane != (struct ncplane *)0);

    ncplane_erase(menu->plane);
    ncplane_set_bg_rgb8(menu->plane, 50, 50, 100);
    ncplane_set_fg_rgb8(menu->plane, 140, 140, 200);
    (void)WidgetIO_putStrYx(menu->plane, 0, 0U,
                            "|     ----menu----     |");
    (void)WidgetIO_putStrYx(menu->plane,
                            (int)(MENU_VISIBLE_ITEMS_ + 1U), 0U,
                            "|----------------------|");

    for (uint32_t row = 0U; row < MENU_VISIBLE_ITEMS_; ++row) {
        uint32_t const idx = (uint32_t)menu->firstVisible + row;
        Menu_drawItem_(menu, idx, row);
    }
}

//============================================================================
void Menu_init(struct Menu * const menu) {
    DBC_REQUIRE(200, menu != (struct Menu *)0);
    menu->plane = (struct ncplane *)0;
    menu->sel = 0U;
    menu->firstVisible = 0U;
    menu->visible = false;
}

void Menu_create(struct Menu * const menu,
                 struct ncplane * const parent,
                 void * const owner,
                 Menu_ResizeCb const resizeCb)
{
    DBC_REQUIRE(210, menu != (struct Menu *)0);
    DBC_REQUIRE(211, parent != (struct ncplane *)0);

    ncplane_options nopts = {
        .y = 0, .x = 0, .rows = MENU_H_, .cols = MENU_W_,
        .name = "menu", .userptr = owner, .resizecb = resizeCb,
    };
    menu->plane = ncplane_create(parent, &nopts);
    DBC_ENSURE(300, menu->plane != (struct ncplane *)0);
    Menu_hide(menu, parent);
}

void Menu_show(struct Menu * const menu,
               struct ncplane * const parent)
{
    DBC_REQUIRE(220, menu != (struct Menu *)0);
    DBC_REQUIRE(221, parent != (struct ncplane *)0);

    Menu_layout_(menu, parent);
    menu->sel = 0U;
    menu->firstVisible = 0U;
    menu->visible = true;
    Menu_draw_(menu);
    ncplane_move_top(menu->plane);
}

void Menu_hide(struct Menu * const menu,
               struct ncplane * const parent)
{
    DBC_REQUIRE(230, menu != (struct Menu *)0);
    DBC_REQUIRE(231, parent != (struct ncplane *)0);

    Menu_layout_(menu, parent);
    menu->firstVisible = 0U;
    menu->visible = false;
    ncplane_erase(menu->plane);
    ncplane_move_bottom(menu->plane);
}

void Menu_syncLayout(struct Menu * const menu,
                     struct ncplane * const parent)
{
    DBC_REQUIRE(240, menu != (struct Menu *)0);
    DBC_REQUIRE(241, parent != (struct ncplane *)0);

    if (menu->visible) {
        Menu_layout_(menu, parent);
        Menu_draw_(menu);
        ncplane_move_top(menu->plane);
    } else {
        Menu_hide(menu, parent);
    }
}

void Menu_selectNext(struct Menu * const menu) {
    DBC_REQUIRE(250, menu != (struct Menu *)0);
    DBC_REQUIRE(251, menu->plane != (struct ncplane *)0);
    uint32_t const maxIdx = MENU_NUM_ITEMS_ - 1U;
    menu->sel = (menu->sel >= maxIdx) ? 0U : (menu->sel + 1U);
    menu->firstVisible = SelectionViewport_update(
        menu->firstVisible, menu->sel,
        MENU_NUM_ITEMS_, MENU_VISIBLE_ITEMS_);
    Menu_draw_(menu);
}

void Menu_selectPrev(struct Menu * const menu) {
    DBC_REQUIRE(260, menu != (struct Menu *)0);
    DBC_REQUIRE(261, menu->plane != (struct ncplane *)0);
    uint32_t const maxIdx = MENU_NUM_ITEMS_ - 1U;
    menu->sel = (menu->sel == 0U) ? maxIdx : (menu->sel - 1U);
    menu->firstVisible = SelectionViewport_update(
        menu->firstVisible, menu->sel,
        MENU_NUM_ITEMS_, MENU_VISIBLE_ITEMS_);
    Menu_draw_(menu);
}

MenuAction Menu_action(struct Menu const * const menu) {
    DBC_REQUIRE(270, menu != (struct Menu const *)0);

    switch (menu->sel) {
    case 0U:
        return MENU_ACT_RESUME;
    case 1U:
        return MENU_ACT_CLEAR;
    case 2U:
        return MENU_ACT_ABOUT;
    case 3U:
        return MENU_ACT_QUIT;
    default:
        return MENU_ACT_RESUME;
    }
}

bool Menu_isVisible(struct Menu const * const menu) {
    DBC_REQUIRE(280, menu != (struct Menu const *)0);
    return menu->visible;
}
