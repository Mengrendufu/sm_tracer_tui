//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#ifndef BSP_H_
#define BSP_H_

//============================================================================
#define BSP_TICKS_PER_SEC    100U
#define BSP_MAX_TICK_HANDLERS_ 4U

typedef void (*BSP_TickHandler)(void);

void BSP_registerTickHandler(BSP_TickHandler handler);
void BSP_onTick(void);

#endif // BSP_H_
