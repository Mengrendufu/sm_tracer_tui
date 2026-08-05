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
#include "widget_io_priv.h"
#include "connection_status_bar.h"
DBC_MODULE_NAME("connection_status_bar")

//============================================================================
static void ConnectionStatusBar_setText_(char * const dst,
                                         size_t const dstLen,
                                         char const * const value,
                                         char const * const fallback)
{
    DBC_REQUIRE(100, dst != (char *)0);
    DBC_REQUIRE(101, dstLen > 0U);
    DBC_REQUIRE(102, fallback != (char const *)0);

    char const * const src =
        (value != (char const *)0) ? value : fallback;
    int const n = snprintf(dst, dstLen, "%s", src);
    DBC_REQUIRE(103, n >= 0);
    DBC_ENSURE(300, dst[dstLen - 1U] == '\0');
}

static void ConnectionStatusBar_drawCell_(
    struct ConnectionStatusBar * const bar,
    int const y,
    unsigned * const x,
    char const * const label,
    char const * const value,
    bool const connectionValue)
{
    DBC_REQUIRE(110, bar != (struct ConnectionStatusBar *)0);
    DBC_REQUIRE(111, bar->plane != (struct ncplane *)0);
    DBC_REQUIRE(112, x != (unsigned *)0);
    DBC_REQUIRE(113, label != (char const *)0);
    DBC_REQUIRE(114, value != (char const *)0);

    char labelPart[16];
    char valuePart[CONNECTION_STATUS_PROTO_LEN_ + 2U];
    int const labelN = snprintf(labelPart, sizeof(labelPart),
                                " %s: ", label);
    int const valueN = snprintf(valuePart, sizeof(valuePart),
                                "%s ", value);
    DBC_REQUIRE(115, labelN >= 0);
    DBC_REQUIRE(116, valueN >= 0);
    if ((unsigned)labelN >= sizeof(labelPart)
        || (unsigned)valueN >= sizeof(valuePart))
    {
        ncplane_dim_yx(bar->plane, NULL, x);
        return;
    }

    int const labelWidth = ncstrwidth(labelPart, NULL, NULL);
    int const valueWidth = ncstrwidth(valuePart, NULL, NULL);
    unsigned cols;
    ncplane_dim_yx(bar->plane, NULL, &cols);
    if (labelWidth < 0 || valueWidth < 0 || *x >= cols
        || (unsigned)(labelWidth + valueWidth) > (cols - *x))
    {
        ncplane_dim_yx(bar->plane, NULL, x);
        return;
    }

    ncplane_on_styles(bar->plane, NCSTYLE_BOLD);
    ncplane_set_fg_rgb8(bar->plane, 105, 185, 170);
    (void)WidgetIO_putStrYx(bar->plane, y, *x, labelPart);
    ncplane_off_styles(bar->plane, NCSTYLE_BOLD);
    if (connectionValue) {
        if (bar->connected) {
            ncplane_set_fg_rgb8(bar->plane, 105, 220, 150);
        } else {
            ncplane_set_fg_rgb8(bar->plane, 235, 100, 110);
        }
    } else {
        ncplane_set_fg_rgb8(bar->plane, 225, 230, 232);
    }
    (void)WidgetIO_putStr(bar->plane, valuePart);
    unsigned const cellWidth = (unsigned)(labelWidth + valueWidth);
    unsigned const remaining = cols - *x - cellWidth;
    unsigned const padding = (remaining < 2U) ? remaining : 2U;
    if (padding == 2U) {
        (void)WidgetIO_putStr(bar->plane, "  ");
    }
    else if (padding == 1U) {
        (void)WidgetIO_putStr(bar->plane, " ");
    }
    ncplane_set_fg_rgb8(bar->plane, 205, 210, 212);
    *x += cellWidth + padding;
}

static void ConnectionStatusBar_draw_(
    struct ConnectionStatusBar * const bar)
{
    DBC_REQUIRE(120, bar != (struct ConnectionStatusBar *)0);
    DBC_REQUIRE(121, bar->plane != (struct ncplane *)0);

    nccell base = NCCELL_TRIVIAL_INITIALIZER;
    nccell_set_bg_rgb8(&base, 24, 27, 31);
    nccell_load_char(bar->plane, &base, ' ');
    ncplane_set_base_cell(bar->plane, &base);
    nccell_release(bar->plane, &base);
    ncplane_erase(bar->plane);

    ncplane_set_bg_rgb8(bar->plane, 42, 42, 44);
    ncplane_set_fg_rgb8(bar->plane, 205, 210, 212);

    unsigned x = 0U;
    ConnectionStatusBar_drawCell_(
        bar, 0, &x, "status", bar->connection, true);
    ConnectionStatusBar_drawCell_(bar, 0, &x, "port", bar->port, false);
    ConnectionStatusBar_drawCell_(bar, 0, &x, "baud", bar->baud, false);
    ConnectionStatusBar_drawCell_(
        bar, 0, &x, "data", bar->dataBits, false);
    ConnectionStatusBar_drawCell_(
        bar, 0, &x, "stop", bar->stopBits, false);
    ConnectionStatusBar_drawCell_(
        bar, 0, &x, "parity", bar->parity, false);
    ConnectionStatusBar_drawCell_(
        bar, 0, &x, "flow", bar->flow, false);

    ncplane_set_bg_rgb8(bar->plane, 36, 36, 38);
    x = 0U;
    ConnectionStatusBar_drawCell_(
        bar, 2, &x, "protocol", bar->protocol, false);
}

//============================================================================
void ConnectionStatusBar_init(struct ConnectionStatusBar * const bar) {
    DBC_REQUIRE(200, bar != (struct ConnectionStatusBar *)0);

    bar->plane = (struct ncplane *)0;
    ConnectionStatusBar_setConnection(bar, false);
    ConnectionStatusBar_setSerial(bar, (char const *)0, (char const *)0,
                                  (char const *)0, (char const *)0,
                                  (char const *)0, (char const *)0);
    ConnectionStatusBar_setProtocol(bar, (char const *)0);
}

void ConnectionStatusBar_create(
    struct ConnectionStatusBar * const bar,
    struct ncplane * const parent,
    void * const owner,
    unsigned const cols,
    ConnectionStatusBar_ResizeCb const resizeCb)
{
    DBC_REQUIRE(210, bar != (struct ConnectionStatusBar *)0);
    DBC_REQUIRE(211, parent != (struct ncplane *)0);

    ncplane_options nopts = {
        .y = 3, .x = 2, .rows = CONNECTION_STATUS_ROWS_,
        .cols = cols, .name = "status",
        .userptr = owner, .resizecb = resizeCb,
    };
    bar->plane = ncplane_create(parent, &nopts);
    DBC_ENSURE(310, bar->plane != (struct ncplane *)0);
    ConnectionStatusBar_draw_(bar);
}

void ConnectionStatusBar_resize(
    struct ConnectionStatusBar * const bar,
    unsigned const cols)
{
    DBC_REQUIRE(220, bar != (struct ConnectionStatusBar *)0);
    DBC_REQUIRE(221, bar->plane != (struct ncplane *)0);
    ncplane_resize_simple(bar->plane, CONNECTION_STATUS_ROWS_, cols);
}

void ConnectionStatusBar_setConnection(
    struct ConnectionStatusBar * const bar,
    bool const connected)
{
    DBC_REQUIRE(230, bar != (struct ConnectionStatusBar *)0);
    bar->connected = connected;
    ConnectionStatusBar_setText_(
        bar->connection, sizeof(bar->connection),
        connected ? "connected" : "disconnected", "disconnected");
    if (bar->plane != (struct ncplane *)0) {
        ConnectionStatusBar_draw_(bar);
    }
}

void ConnectionStatusBar_setSerial(
    struct ConnectionStatusBar * const bar,
    char const * const port,
    char const * const baud,
    char const * const dataBits,
    char const * const stopBits,
    char const * const parity,
    char const * const flow)
{
    DBC_REQUIRE(240, bar != (struct ConnectionStatusBar *)0);

    ConnectionStatusBar_setText_(bar->port, sizeof(bar->port),
                                 port, "none");
    ConnectionStatusBar_setText_(bar->baud, sizeof(bar->baud),
                                 baud, "115200");
    ConnectionStatusBar_setText_(bar->dataBits, sizeof(bar->dataBits),
                                 dataBits, "8");
    ConnectionStatusBar_setText_(bar->stopBits, sizeof(bar->stopBits),
                                 stopBits, "1");
    ConnectionStatusBar_setText_(bar->parity, sizeof(bar->parity),
                                 parity, "none");
    ConnectionStatusBar_setText_(bar->flow, sizeof(bar->flow),
                                 flow, "none");
    if (bar->plane != (struct ncplane *)0) {
        ConnectionStatusBar_draw_(bar);
    }
}

void ConnectionStatusBar_setProtocol(
    struct ConnectionStatusBar * const bar,
    char const * const protocol)
{
    DBC_REQUIRE(250, bar != (struct ConnectionStatusBar *)0);
    ConnectionStatusBar_setText_(bar->protocol, sizeof(bar->protocol),
                                 protocol, "none");
    if (bar->plane != (struct ncplane *)0) {
        ConnectionStatusBar_draw_(bar);
    }
}
