#include <string.h>
#include "dxcm_ecdh.h"
#include "dxcm_random.h"
#include "uECC.h"
#include "dbg.h"

#include "nrf_drv_rng.h"
#include "nrf_crypto_rng.h"
/*----------------------------------------------------------------------------*/
static uECC_Curve _uECCcurve = 0;
static int _rng_func(uint8_t *dest, unsigned size)
{
//  if (dxcm_errno_ok != dxcm_random_data(dest, size)) return 0;
//  return 1;
    uint32_t err_code;
    err_code = nrf_crypto_rng_vector_generate(dest, size);
    if (err_code == NRF_SUCCESS)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

/*----------------------------------------------------------------------------*/
void dxcm_ecdh_init(void)
{
  if (_uECCcurve) return;
	uint32_t ret_val;
    ret_val = nrf_drv_rng_init(NULL);
    if (ret_val == NRF_SUCCESS)
    {
        uECC_set_rng(&_rng_func);
    }
  _uECCcurve = uECC_secp256r1();
}

/*----------------------------------------------------------------------------*/
dxcm_errno_t dxcm_ecdh_create(dxcm_ecdh_cntxt_t *cntxt)
{
  if (!_uECCcurve)                                    return dxcm_errno_again;
  if (!cntxt)                                         return dxcm_errno_inval;
  if (!cntxt->local_public_key)                       return dxcm_errno_inval;
  if (((unsigned long)(cntxt->local_public_key)) & 3) return dxcm_errno_inval;
  if (!uECC_make_key(cntxt->local_public_key,
                     cntxt->private_key,
                     _uECCcurve))
    return dxcm_errno_io;
  return dxcm_errno_ok;
}

/*----------------------------------------------------------------------------*/
dxcm_errno_t dxcm_ecdh_compute_shared_secret(dxcm_ecdh_cntxt_t *cntxt)
{
  if (!_uECCcurve)                                     return dxcm_errno_again;
  if (!cntxt)                                          return dxcm_errno_inval;
  if (!cntxt->remote_public_key)                       return dxcm_errno_inval;
  if (((unsigned long)(cntxt->remote_public_key)) & 3) return dxcm_errno_inval;
  if (!cntxt->shared_secret)                           return dxcm_errno_inval;
  if (((unsigned long)(cntxt->shared_secret)) & 3)     return dxcm_errno_inval;
  if (!uECC_shared_secret(cntxt->remote_public_key,
                          cntxt->private_key,
                          cntxt->shared_secret, _uECCcurve))
    return dxcm_errno_io;
  return dxcm_errno_ok;
}

