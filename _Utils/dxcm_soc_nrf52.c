/* Copyright 2018 by Dexcom, Inc.  All rights reserved. */

#include <stdint.h>
#include "dxcm_soc.h"
#include "dxcm_hw_soc.h"
#include "nrf52.h"
#include "dbg.h"

#include "nrf_sdm.h"
#include "nrf_nvic.h"
#include "ble.h"
#include "app_error.h"

/*----------------------------------------------------------------------------*/
#define _NUM_HDLRS (4)
typedef void (*_evt_hdlr_t)(uint32_t evt);
static _evt_hdlr_t _evt_handlers[_NUM_HDLRS];
static unsigned _evt_handler_count;
static void _sd_evt_dispatch(uint32_t evt)
{
  unsigned n;
  for (n=0;n<_evt_handler_count;n++)
    _evt_handlers[n](evt);
}
dxcm_errno_t dxcm_soc_reg_evt_hdlr(void (*handler)(uint32_t evt))
{
  /* Reject null pointers */
  if (!handler) return dxcm_errno_inval;

  /* Skip over handlers that are already registered */
  unsigned n;
  for (n=0;n<_evt_handler_count;n++)
    if (_evt_handlers[n] == handler)
      return dxcm_errno_ok;

  /* If we're full then we're full */
  if (_evt_handler_count == _NUM_HDLRS) return dxcm_errno_nomem;
  
  /* Sotre this new handler */
  _evt_handlers[_evt_handler_count++] = handler;

  return dxcm_errno_ok;
}

///* Global nvic state instance, required by nrf_nvic.h */
//nrf_nvic_state_t nrf_nvic_state;

static volatile int _sd_enabled;

//static void (*_ble_evt_handler)(void*);

///* uint32_t softdevice_ble_evt_handler_set(ble_evt_handler_t ble_evt_handler) */
//void dxcm_soc_reg_ble_hdlr(void (*handler)(void* ble_evt_data))
//{
//  _ble_evt_handler = handler;
//}

//void SD_EVT_IRQHandler(void)
//{
//  if (!_sd_enabled) return;

//  /* Fetch and dispatch all SOC events */
//  for (;;)
//  {
//    uint32_t evt_id;
//    uint32_t err_code = sd_evt_get(&evt_id);

//    if (err_code == NRF_ERROR_NOT_FOUND) break;
//    if (err_code != NRF_SUCCESS) APP_ERROR_HANDLER(err_code);
//    else                         _sd_evt_dispatch(evt_id);
//  }

//  if (!_ble_evt_handler) return;

//  for (;;)
//  {
//    /* Pull event from stack */
//    uint32_t ble_evt[(sizeof(ble_evt_t)+BLE_GATT_ATT_MTU_DEFAULT+3)/4];
//    uint16_t evt_len = sizeof(ble_evt);
//    uint32_t err_code = sd_ble_evt_get((void*)ble_evt, &evt_len);
//    if (err_code == NRF_ERROR_NOT_FOUND) break;
//    if (err_code != NRF_SUCCESS) APP_ERROR_HANDLER(err_code);
//    else                         _ble_evt_handler((ble_evt_t *)ble_evt);
//  }
//}

/*----------------------------------------------------------------------------*/
void dxcm_soc_disable(void)
{
  if (dxcm_soc_is_enabled()) sd_softdevice_disable();
  _sd_enabled = 0;
}

/*----------------------------------------------------------------------------*/
void dxcm_soc_init(unsigned force_reinit)
{
  static unsigned _initted = 0;
  if (force_reinit) _initted = 0;
  if (_initted) return;
  _initted = 1;

  dxcm_soc_disable();

  nrf_clock_lf_cfg_t clock_lf_cfg;
  clock_lf_cfg.source        = NRF_CLOCK_LF_SRC_RC;
  clock_lf_cfg.rc_ctiv       = 16;
  clock_lf_cfg.rc_temp_ctiv  = 2;
  clock_lf_cfg.xtal_accuracy   = NRF_CLOCK_LF_XTAL_ACCURACY_500_PPM;
	uint32_t error_code = sd_softdevice_enable(&clock_lf_cfg, app_error_fault_handler);
  if (NRF_SUCCESS!= error_code)
    dbgcol(red, blk, "SOftdevice failed to initialize!, %X", error_code);

  _sd_enabled = 1;

  sd_nvic_EnableIRQ((IRQn_Type)SD_EVT_IRQn);

  //Stack power settings
  //softdevice uses this to fine tune power: may affect S to N ratio
  sd_power_dcdc_mode_set(NRF_POWER_DCDC_ENABLE);

  //set lowest power mode when nRF enters sd_wait_event()
  sd_power_mode_set(NRF_POWER_MODE_LOWPWR);

  /* Let anyone know, who may be interested, that we've reset the nordic SD */
  _sd_evt_dispatch(DXCM_SOC_EVT_RESTARTED);
}

/*----------------------------------------------------------------------------*/
int dxcm_soc_is_enabled(void)
{
  uint8_t enabled;
  return ((NRF_SUCCESS == sd_softdevice_is_enabled(&enabled)) && enabled);
}

/*----------------------------------------------------------------------------*/
uint32_t dxcm_soc_silicon_version(void)
{
  return NRF_FICR->INFO.VARIANT;
}

/*----------------------------------------------------------------------------*/
uint64_t dxcm_soc_device_address(void)
{
  uint64_t addr;
  addr = NRF_FICR->DEVICEADDR[1];
  addr <<= 32;
  addr |= NRF_FICR->DEVICEADDR[0];
  return addr;
}

/*----------------------------------------------------------------------------*/
void dxcm_soc_reset(void)
{
  NVIC_SystemReset();
}
