//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#ifndef UI_EVT_H_
#define UI_EVT_H_

#include <stddef.h>

//============================================================================
//=== UI event ingress — thread-safe, callable from application components

void UI_postText(char const *text);
// Copy a NUL-separated, double-NUL-terminated sequence into the UI inbox.
void UI_postPortList(char const *portNames, size_t portNamesSize);

#endif // UI_EVT_H_
