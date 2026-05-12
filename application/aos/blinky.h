//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#ifndef BLINKY_H_
#define BLINKY_H_

#include "sst.h"

#define BLINKY_Q_LEN_ 8U

void Blinky_ctor(void);
SST_Task *Blinky_getTask(void);
extern SST_Evt const *Blinky_qBuf_[BLINKY_Q_LEN_];

#endif // BLINKY_H_
