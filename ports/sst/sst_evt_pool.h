//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
#ifndef SST_EVT_POOL_H_
#define SST_EVT_POOL_H_

#ifndef SST_EVT_POOL_NUM
#define SST_EVT_POOL_NUM 0U
#endif // SST_EVT_POOL_NUM

//============================================================================
//=== AO event pools: multi-size pools for AO-to-AO communication.
#if (SST_EVT_POOL_NUM > 0U)

#include "static_pool.h"

void SST_EvtPool_init(void *sto, PoolCtr poolSize, PoolCtr blockSize);
void *SST_Evt_new(PoolCtr blockSize);
void SST_Evt_gc(void *evt);

#define SST_NEW(evtType_) ((evtType_ *)SST_Evt_new(sizeof(evtType_)))
#define SST_GC(evt_)       SST_Evt_gc((void *)(evt_))

#else // (SST_EVT_POOL_NUM == 0U):: event pool disable

#define SST_GC(evt_) ((void)0)

#endif // (SST_EVT_POOL_NUM > 0U)

#endif // SST_EVT_POOL_H_
