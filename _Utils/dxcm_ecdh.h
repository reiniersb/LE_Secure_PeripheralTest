/* Copyright 2018 by Dexcom, Inc.  All rights reserved */

#pragma once

#include <stdint.h>
#include "dxcm_errno.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define DXCM_ECDH_PUBLIC_KEY_LEN    (64 /* bytes */)
#define DXCM_ECDH_SHARED_SECRET_LEN (32 /* bytes */)

/* This context is provided to the library. The pointed to memory
   must not be modified by the client (or reused by the client) until use
   of the context is complete */
typedef struct
{
  uint8_t private_key[32] __attribute__((aligned(4)));

  /* The memory pointed to must be aligned on 4 byte boundary */
  void* local_public_key;  /* Must point to 64 bytes of storage */
  void* remote_public_key; /* Must point to 64 bytes of storage */
  void* shared_secret;     /* Must point to 32 bytes of storage */
} dxcm_ecdh_cntxt_t;

/* Initialize library */
void dxcm_ecdh_init(void);

/* Initialize a cntxt. Subsequently, private_key and local_public_key have
   been initialized and can be used by the client (but probably best for
   client to not provide private key to anyone else) */
dxcm_errno_t dxcm_ecdh_create(dxcm_ecdh_cntxt_t *cntxt);

/* Compute shared secret. Prior to invocation client must initialize
   remote_public_key. Subsequent to invocation shared_secret is populated.
   Note that this routine may be called more than once, per contect, but that
   the results will be the same each time. Once shared_secret has been
   extracted the cntxt could be recycled. */
dxcm_errno_t dxcm_ecdh_compute_shared_secret(dxcm_ecdh_cntxt_t *cntxt);

#ifdef __cplusplus
}
#endif
