/* Copyright 2018 by Dexcom, Inc.  All rights reserved. */
#pragma once

#include <stdint.h>
#include "dxcm_errno.h"
#include "dxcm_hw_soc.h" /* For HW specific events */

#ifdef __cplusplus
extern "C"
{
#endif

/* Initialize SOC (or force reinit without reseting processor) */
void dxcm_soc_init(unsigned force_reinit);

/* Reset the processor within the SOC
   (WARNING: Use dxcm_reset.h instead in most cases. This is a sledgehammer) */
void dxcm_soc_reset(void);

/* Indicates if SOC services have been initialized/enabled
   returns !0 if dxcm_soc_init() has been called and, when run on Nordic,
   the soft device is still running */
int dxcm_soc_is_enabled(void);

/* Disable the SOC (on Nordic this has the affect of disabling the softdevice
   which is needed, sometimes, to perform low level stuff. To re-enable the
   softdevice it is necessary to invoke dxcm_soc_init() */
void dxcm_soc_disable(void);

/* Register handler (can register more than one) to receive events from
   SOC. Events are SOC specific */
dxcm_errno_t dxcm_soc_reg_evt_hdlr(void (*handler)(uint32_t evt));

/* This turns out to be rather Nordic-centric but it might apply to other stacks
   as well. This handler gets called for BLE events with ble_evt_t* data. At
   this time, however, I do not want to pull the definition of ble_evt_t* in
   from Nordic files */
void dxcm_soc_reg_ble_hdlr(void (*handler)(void* ble_evt_data));

/* Query any version information for the SOC silicon revision */
uint32_t dxcm_soc_silicon_version(void);

/* Query the SOC's device address. Ideally the SOC has a unique MAC address
   or other sort of device address available. */
uint64_t dxcm_soc_device_address(void);

#ifdef __cplusplus
}
#endif
