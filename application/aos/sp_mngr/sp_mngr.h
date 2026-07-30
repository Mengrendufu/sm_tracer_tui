//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#ifndef SP_MNGR_H_
#define SP_MNGR_H_

#include "sst.h"

//============================================================================
//=== AO_SpMngr lifecycle

extern SST_Task * const AO_SpMngr;

void SpMngr_ctor(void);

#endif // SP_MNGR_H_
