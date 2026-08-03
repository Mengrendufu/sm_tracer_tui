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

#include <stddef.h>
#include "sst.h"

enum {
    SPMNGR_VALUE_LEN = 256U
};

typedef struct {
    char port[SPMNGR_VALUE_LEN];
    char baudrate[SPMNGR_VALUE_LEN];
    char dataBits[SPMNGR_VALUE_LEN];
    char stopBits[SPMNGR_VALUE_LEN];
    char parity[SPMNGR_VALUE_LEN];
    char flowControl[SPMNGR_VALUE_LEN];
    char protocol[SPMNGR_VALUE_LEN];
} SpMngrConfig;

// Complete serial configuration snapshot carried across the AO boundary.
typedef struct {
    SST_Evt super;
    SpMngrConfig config;
} SpMngrConfigEvt;

// Serial-port-list result. Ownership of the packed names transfers with the
// event. Names are NUL-separated and the sequence is double-NUL-terminated.
typedef struct {
    SST_Evt super;
    char *portNames;
    size_t portNamesSize;
} SpMngrPortsEvt;

//============================================================================
//=== AO_SpMngr lifecycle

extern SST_Task * const AO_SpMngr;

void SpMngr_ctor(void);

#endif // SP_MNGR_H_
