/* Copyright 2017 by Dexcom, Inc.  All rights reserved. */

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

/* Many of these are reused from a list of "generic" errno's from a Linux/Posix
   system. Dexcom will map its errno type responses into this domain of error */
typedef enum
{
  dxcm_errno_ok    =  0,/* No error - all is well */
  dxcm_errno_perm  =  1,/* Not permitted */
  dxcm_errno_noent =  2,/* No such file or entity */
  dxcm_errno_io    =  5,/* I/O error - or other device error */
  dxcm_errno_badf  =  9,/* Bad handle */
  dxcm_errno_again = 11,/* Try again later */
  dxcm_errno_nomem = 12,/* Out of memory */
  dxcm_errno_acces = 13,/* Permission denied */
  dxcm_errno_fault = 14,/* Bad address */
  dxcm_errno_busy  = 16,/* Busy */
  dxcm_errno_inval = 22,/* Invalid argument */
  dxcm_errno_nospc = 28,/* No space left on device */
  dxcm_errno_range = 34,/* Range of value is not acceptable */
  dxcm_errno_nosys = 38,/* Function not implemented */
  dxcm_errno_time  = 62,/* Timer expired */
  dxcm_errno_proto = 71,/* Protocol error */
} dxcm_errno_t;

#ifdef __cplusplus
}
#endif
