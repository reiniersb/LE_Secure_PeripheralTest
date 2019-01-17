/* Copyright 2018 by Dexcom, Inc.  All rights reserved. */

#pragma once

#include "dxcm_errno.h"

#ifdef __cplusplus
extern "C" dxcm_errno_t dxcm_random_data(void* dst, unsigned bytes);
#else
           dxcm_errno_t dxcm_random_data(void* dst, unsigned bytes);
#endif
