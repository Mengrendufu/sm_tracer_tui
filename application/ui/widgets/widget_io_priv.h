//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#ifndef WIDGET_IO_PRIV_H_
#define WIDGET_IO_PRIV_H_

#include <stdbool.h>

struct ncplane;

bool WidgetIO_putStrYx(struct ncplane *plane, int y, unsigned x,
                       char const *text);
bool WidgetIO_putStr(struct ncplane *plane, char const *text);

#endif // WIDGET_IO_PRIV_H_
