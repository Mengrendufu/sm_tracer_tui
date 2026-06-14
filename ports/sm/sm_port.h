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
//=== sm_hsm port: offsetof/containerof
#ifndef SM_PORT_H_
#define SM_PORT_H_

#include <stdbool.h>
#include <stdint.h>

//============================================================================
//=== offsetof / containerof macros.
#ifndef offsetof
    #define offsetof(type_, member_) ((size_t)(&(((type_ *)0)->member_)))
#endif

#ifndef containerof
    #define containerof(ptr_, type_, member_) \
                     ((type_ *)(((char *)(ptr_ )) - offsetof(type_, member_)))
#endif

#endif /* SM_PORT_H_ */
