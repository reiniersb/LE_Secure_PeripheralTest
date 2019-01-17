/* Copyright 2018 by Dexcom, Inc.  All rights reserved. */

#include "generic_comm_interface.h"
#include <string.h>
#include "ble.h"
#include "ble_hci.h"
//#include "nrf_nvic.h"

#include "dbg.h"

#include "dxcm_ecdh.h"
#include "dxcm_soc.h"

#include "app_util_platform.h"


/* Additional softdevice memory beyond the base (which is currently 0x13c0) */
//static uint8_t _sd_ram[4920] __attribute__((section(".softdevice_ram")));

ble_cfg_t s_ble_cfg;
ble_gap_conn_params_t s_connParams = {0};

uint8_t UUID_TYPE_DEXCOM_BASE                             = BLE_UUID_TYPE_UNKNOWN;
const uint64_t DEXCOM_BASE_UUID[2]                        = {0xC59430F1F86A4EA5, 0xF8080000849E531C}; //F8080000-849E-531C-C594-30F1F86A4EA5
const uint16_t UUID_16_LEGACY_SERVICE                     = 0x3532;
uint16_t s_serviceHandle                                  = 0;
ble_uuid_t s_serviceUuid                                  = {0};
ble_uuid128_t s_uuid;
ble_gap_sec_params_t                                m_securityParams;
ble_gap_sec_keyset_t                                _keys_exchanged;
ble_gap_id_key_t                                    _peerId;
ble_gap_enc_key_t                                   _encryptionKey;

dxcm_ecdh_cntxt_t			                              _le_ecdh_ctxt;
ble_gap_lesc_p256_pk_t                              _le_secure_pub_key;
ble_gap_lesc_p256_pk_t                              _le_secure_remote_pub_key;
ble_gap_lesc_dhkey_t 	                              _le_shared_secret;

void force_disconnect(void)
{
 if (connHandle != BLE_CONN_HANDLE_INVALID )
 {
	 sd_ble_gap_disconnect(connHandle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
 }
}

ble_evt_t _ble_evnt_array[MAX_QUEUED_EVNT_COUNT];
extern uint8_t   _ble_evnt_array_indx;

uint32_t register_dexcom_service()
{
  uint32_t err_code = NRF_SUCCESS;

  memcpy(s_uuid.uuid128, DEXCOM_BASE_UUID, sizeof(s_uuid.uuid128));
  err_code = sd_ble_uuid_vs_add(&s_uuid, &UUID_TYPE_DEXCOM_BASE);

  if(NRF_SUCCESS !=  err_code)
  {
    dbgmsg("Failed sd_ble_uuid_vs_add: 0x%x\n", err_code);
    return err_code;
  }

  s_serviceUuid.type = UUID_TYPE_DEXCOM_BASE;
  s_serviceUuid.uuid = UUID_16_LEGACY_SERVICE;

#if SERVER_MODE
  err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &s_serviceUuid, &s_serviceHandle);

  if(NRF_SUCCESS !=  err_code)
  {
    dbgmsg("Failed sd_ble_gatts_service_add: 0x%x\n", err_code);
    return err_code;
  }
#endif  

  return err_code;
}

void ble_evt_dispatch_srv(ble_evt_t * p_data)
{
  if(_ble_evnt_array_indx >= MAX_QUEUED_EVNT_COUNT) 
  {
    dbgmsg("ble_evt_dispatch::_ble_evnt_array_indx >= MAX_QUEUED_EVNT_COUNT\n");
    return;
  }

  uint8_t reqd_indx = 0/*, critical_or_not = 0*/;
//  sd_nvic_critical_region_enter(&critical_or_not);
	CRITICAL_REGION_ENTER();
  reqd_indx = _ble_evnt_array_indx;
  _ble_evnt_array_indx++;
//  sd_nvic_critical_region_exit(critical_or_not);
	CRITICAL_REGION_EXIT();
  memcpy(&_ble_evnt_array[reqd_indx], (ble_evt_t*)p_data, sizeof(ble_evt_t));

#if SERVER_MODE
#else
  handle_ble_event_interrput_ctxt((ble_evt_t*)p_data);
#endif  
}
#include "softdevice_handler.h"


uint32_t config_sd(uint8_t* server_id, uint8_t len)
{
  uint32_t err_code;
	
	uint32_t app_ram = 0;
  err_code = softdevice_app_ram_start_get(&app_ram);
	
//  uint32_t app_ram = (uint32_t)(_sd_ram + sizeof(_sd_ram));
  const uint32_t DEFAULT_ATT_TABLE_SIZE   = 0x580;

  s_connParams.conn_sup_timeout           = BLE_GAP_CP_CONN_SUP_TIMEOUT_MAX;
  s_connParams.min_conn_interval          = 240; //BLE_GAP_CP_MIN_CONN_INTVL_MIN;
  s_connParams.max_conn_interval          = 240; //BLE_GAP_CP_MIN_CONN_INTVL_MIN;
  s_connParams.slave_latency              = 0;


  memset(&s_ble_cfg, 0, sizeof(s_ble_cfg));
  s_ble_cfg.conn_cfg.conn_cfg_tag = 1;

  ble_gap_cfg_role_count_t ble_gap_cfg_role_count = 
  { 1 /* No of concurrent peripherals that are supported */, 
    1,/* No of concurrent centrals that are supported */
    1
  };

  s_ble_cfg.gap_cfg.role_count_cfg = ble_gap_cfg_role_count;
  err_code = sd_ble_cfg_set(BLE_GAP_CFG_ROLE_COUNT, &s_ble_cfg, app_ram);
  if(NRF_SUCCESS != err_code)
  {
    dbgmsg("Failed to set BLE_GAP_CFG_ROLE_COUNT, 0x%x\n", err_code);
    return err_code;
  }

#if SERVER_MODE 
    /*Soft device being configured for the server configuration*/

    /*Set the device name here. If not set, the device name can appear as "nRF5X" in clients*/
    static uint8_t s_device_name[MAX_ADV_LENGTH];
    memcpy(s_device_name, server_id, len);  
    memset(&s_ble_cfg, 0, sizeof(s_ble_cfg));
    s_ble_cfg.conn_cfg.conn_cfg_tag                     = 1;
    s_ble_cfg.gap_cfg.device_name_cfg.write_perm.sm     = 1; /*No Security restrictions. Allow any device to read the device name*/
    s_ble_cfg.gap_cfg.device_name_cfg.write_perm.lv     = 1; /*No Security restrictions. Allow any device to read the device name*/
    s_ble_cfg.gap_cfg.device_name_cfg.vloc              = BLE_GATTS_VLOC_USER;
    s_ble_cfg.gap_cfg.device_name_cfg.p_value           = s_device_name;
    s_ble_cfg.gap_cfg.device_name_cfg.current_len       = len;
    s_ble_cfg.gap_cfg.device_name_cfg.max_len           = len;
    err_code = sd_ble_cfg_set(BLE_GAP_CFG_DEVICE_NAME, &s_ble_cfg, app_ram);
    if(NRF_SUCCESS != err_code)
    {
        dbgmsg("Failed to set BLE_GAP_CFG_DEVICE_NAME, 0x%x\n", err_code);
        return err_code;
    }
#endif    

  memset(&s_ble_cfg, 0, sizeof(s_ble_cfg));
  s_ble_cfg.conn_cfg.conn_cfg_tag = 1;
  s_ble_cfg.gatts_cfg.attr_tab_size.attr_tab_size = DEFAULT_ATT_TABLE_SIZE;
  err_code = sd_ble_cfg_set(BLE_GATTS_CFG_ATTR_TAB_SIZE, &s_ble_cfg, app_ram);
  if(NRF_SUCCESS != err_code)
  {
      dbgmsg("Failed to set BLE_GATTS_CFG_ATTR_TAB_SIZE, 0x%x\n", err_code);
      return err_code;
  }

  memset(&s_ble_cfg, 0, sizeof(s_ble_cfg));
  s_ble_cfg.conn_cfg.conn_cfg_tag = 1;
  s_ble_cfg.conn_cfg.params.gatt_conn_cfg.att_mtu = 23;
  err_code = sd_ble_cfg_set(BLE_CONN_CFG_GATT, &s_ble_cfg, app_ram);
  
  if(NRF_SUCCESS != err_code)
  {
      dbgmsg("Failed to set BLE_CONN_CFG_GATT, 0x%x\n", err_code);
      return err_code;
  }  


  uint32_t sd_ram = app_ram;
  err_code = sd_ble_enable(&sd_ram);
  if(NRF_SUCCESS != err_code)
  {
      dbgmsg("Failed sd_ble_enable, 0x%x\n", err_code);
  }

//  if (app_ram != sd_ram)
//  {
//    int delta = sd_ram - app_ram; 
//    dbgcol(red, blk, "_sd_ram[] should be %u instead of %u bytes!",
//           sizeof(_sd_ram)+delta, sizeof(_sd_ram));
//    if (sd_ram > app_ram) return NRF_ERROR_NOT_SUPPORTED;
//  }

//  /*Register function to receive BLE*/
//  dxcm_soc_reg_ble_hdlr(ble_evt_dispatch);

#if SERVER_MODE 
    err_code = sd_ble_gap_ppcp_set(&s_connParams);
    if(NRF_SUCCESS != err_code)
    {
        dbgmsg("Failed sd_ble_gap_ppcp_set, 0x%x\n", err_code);
        return err_code;
    }
#endif

    /*Init the ECDH context*/
    memset(&_le_secure_pub_key, 0, sizeof(_le_secure_pub_key));
    memset(&_le_secure_remote_pub_key, 0, sizeof(_le_secure_remote_pub_key));
    memset(&_le_ecdh_ctxt, 0, sizeof(_le_ecdh_ctxt));

    /*Set the pointers in the ecdh context to the arrays being managed by the stack*/
    _le_ecdh_ctxt.local_public_key  = _le_secure_pub_key.pk;
    _le_ecdh_ctxt.remote_public_key = _le_secure_remote_pub_key.pk;
    _le_ecdh_ctxt.shared_secret     = _le_shared_secret.key;

    dxcm_ecdh_init();


    m_securityParams.bond = 1; //< Perform bonding.
    m_securityParams.mitm = 0; //< Man In The Middle protection not required.
    m_securityParams.lesc = 1; //< LE Secure Connections enabled.
    m_securityParams.keypress = 0; //< Keypress notifications not enabled.
    m_securityParams.io_caps = BLE_GAP_IO_CAPS_NONE; //< No I/O capabilities.
    m_securityParams.oob = 0; //< Out Of Band data not available.
    m_securityParams.min_key_size = 7; //< Minimum encryption key size.
    m_securityParams.max_key_size = 16; //< Maximum encryption key size.

    m_securityParams.kdist_peer.enc  = 0;
    m_securityParams.kdist_peer.id   = 1;
    m_securityParams.kdist_peer.sign = 0;
    m_securityParams.kdist_peer.link = 0;
    m_securityParams.kdist_own.enc   = 1;
    m_securityParams.kdist_own.id    = 0;
    m_securityParams.kdist_own.sign  = 0;
    m_securityParams.kdist_own.link  = 0;

    _keys_exchanged.keys_peer.p_enc_key  = NULL;
    _keys_exchanged.keys_peer.p_id_key   = &_peerId; 
    _keys_exchanged.keys_peer.p_sign_key = NULL;
    _keys_exchanged.keys_peer.p_pk       = &_le_secure_remote_pub_key;
    _keys_exchanged.keys_own.p_enc_key   = &_encryptionKey;
    _keys_exchanged.keys_own.p_id_key    = NULL;
    _keys_exchanged.keys_own.p_sign_key  = NULL;
    _keys_exchanged.keys_own.p_pk 			 = &_le_secure_pub_key;


   err_code = register_dexcom_service();
   if(NRF_SUCCESS != err_code) 
    {
      dbgmsg("Failed in register_dexcom_service \n");
      return err_code;
    }

  return err_code;
}
