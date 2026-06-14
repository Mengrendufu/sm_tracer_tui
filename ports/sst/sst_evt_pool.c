//============================================================================
// Copyright (C) 2026 Sunny Matato
//
// This program is free software. It comes without any warranty, to
// the extent permitted by applicable law. You can redistribute it
// and/or modify it under the terms of the Do What The Fuck You Want
// To Public License, Version 2, as published by Sam Hocevar.
// See http://www.wtfpl.net/ for more details.
//============================================================================
//============================================================================
//=== SST event pool: multi-size pools for AO-to-AO communication
#include "sst.h"

#if (SST_EVT_POOL_NUM > 0U)

#include "dbc_assert.h"
DBC_MODULE_NAME("sst_evt_pool")

//============================================================================
//=== Event pool storage
static uint8_t    l_evtPoolsNum_;
static StaticPool l_evtPools_[SST_EVT_POOL_NUM];

//============================================================================
//=== Event pool operations
void SST_EvtPool_init(void * const sto,
                      PoolCtr const poolSize,
                      PoolCtr const blockSize)
{
    DBC_REQUIRE(100, l_evtPoolsNum_ < SST_EVT_POOL_NUM);
    DBC_REQUIRE(101, sto != (void *)0);
    DBC_REQUIRE(102, blockSize >= sizeof(SST_Evt));

    StaticPool_init(&l_evtPools_[l_evtPoolsNum_], sto, poolSize, blockSize);
    ++l_evtPoolsNum_;
}
//............................................................................
void *SST_Evt_new(PoolCtr const blockSize) {
    SST_Evt *evt = (SST_Evt *)0;

    DBC_REQUIRE(300, blockSize >= sizeof(SST_Evt));

    SST_PORT_CRIT_ENTRY();

    uint8_t poolNum = 0U;
    for (; poolNum < l_evtPoolsNum_; ++poolNum) {
        if (l_evtPools_[poolNum].blockSize >= blockSize) {
            break;
        }
    }

    DBC_ENSURE(401, poolNum < l_evtPoolsNum_);

    evt = (SST_Evt *)StaticPool_get(&l_evtPools_[poolNum]);
    DBC_ENSURE(400, evt != (SST_Evt *)0);

    evt->poolId = poolNum + 1U;
    evt->refCtr = 0U;

    SST_PORT_CRIT_EXIT();

    return evt;
}
//............................................................................
void SST_Evt_gc(void * const evt) {
    SST_Evt * const e = (SST_Evt *)evt;

    DBC_REQUIRE(200, e != (SST_Evt *)0);

    SST_PORT_CRIT_ENTRY();

    if (e->poolId != 0U) {
        uint8_t const poolNum = e->poolId - 1U;

        DBC_INVARIANT(201, poolNum < l_evtPoolsNum_);

        if (e->refCtr > 1U) {
            --e->refCtr;
        } else {
            StaticPool_put(&l_evtPools_[poolNum], evt);
        }
    }

    SST_PORT_CRIT_EXIT();
}

#endif // (SST_EVT_POOL_NUM > 0U)
