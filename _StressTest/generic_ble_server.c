/* Copyright 2018 by Dexcom, Inc.  All rights reserved. */
#include "generic_comm_interface.h"

#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
#include "ble.h"
#include "ble_gap.h"
#include "ble_gatts.h"
#include "nrf_error.h"
#include "ble_hci.h"
#include "nrf_nvic.h"


#include "dxcm_ecdh.h"
#include "dbg.h"

#include "app_util_platform.h"

#include "boards.h"
#include "bsp.h"
#include "bsp_btn_ble.h"
#define PERIPHERAL_ADVERTISING_LED      BSP_BOARD_LED_0
#define PERIPHERAL_CONNECTED_LED        BSP_BOARD_LED_1
#define PERIPHERAL_BOND_FAILED_LED      BSP_BOARD_LED_2
#define PERIPHERAL_BONDED_LED						BSP_BOARD_LED_3

#define MAX_SYS_ATTR_DATA_LEN 56

uint16_t connHandle = BLE_CONN_HANDLE_INVALID;

extern ble_cfg_t                s_ble_cfg;
extern uint16_t                 s_serviceHandle;
extern uint8_t                  UUID_TYPE_DEXCOM_BASE;

static const uint16_t           UUID_16_LEGACY_SERVICE_TEST_CHAR  = 0x3533;
static const uint8_t            ASSIGNED_GAP_COMPLETE_LOCAL_NAME 	= 0x09;
static const uint8_t ASSIGNED_GAP_INCOMPLETE_UUID_16_BIT 		      = 0x02;
static bool                     _bond_completed                   = false;

static uint8_t _sys_attr_data[MAX_SYS_ATTR_DATA_LEN];
static uint16_t _sys_attr_data_len = MAX_SYS_ATTR_DATA_LEN;

static uint8_t                  advData[MAX_ADV_LENGTH];
extern ble_gap_sec_params_t     m_securityParams;
extern ble_gap_id_key_t         _peerId;
static ble_gatts_char_handles_t _dexcom_charHandles;
extern ble_gap_enc_key_t        _encryptionKey;
extern ble_gap_sec_keyset_t     _keys_exchanged;
extern dxcm_ecdh_cntxt_t			  _le_ecdh_ctxt;
extern ble_gap_lesc_dhkey_t 	  _le_shared_secret;
extern ble_evt_t _ble_evnt_array[MAX_QUEUED_EVNT_COUNT];
extern uint8_t   _ble_evnt_array_indx;
extern ble_cfg_t s_ble_cfg;

#include "ble_advertising.h"
//static void on_adv_evt(ble_adv_evt_t ble_adv_evt)
//{
//}

uint32_t start_advertising()
{
  uint32_t err_code = sd_ble_gap_adv_data_set(advData, advData[0] + 5, 0, 0);

  if(NRF_SUCCESS != err_code)
  {
    dbgmsg("sd_ble_gap_adv_data_set Failed, 0x%x\n", err_code);
    return err_code;
  }

  ble_gap_adv_params_t params = {0};
  params.type                 = BLE_GAP_ADV_TYPE_ADV_IND; /*Connectable advertising*/
  params.p_peer_addr          = 0;
  params.fp                   = BLE_GAP_ADV_FP_ANY;
  params.interval             = 144; /*adv interval 90 ms - 144 * 0.625*/
  params.timeout              = 0 /*No time out advertise forever*/;

  err_code = sd_ble_gap_adv_start(&params, s_ble_cfg.conn_cfg.conn_cfg_tag);
  if(NRF_SUCCESS != err_code)
  {
    dbgmsg("sd_ble_gap_adv_start Failed, 0x%x\n", err_code);
    return err_code;
  }

  return NRF_SUCCESS;   

}

 uint32_t init_dex_characteristic()
{
    uint32_t err_code = NRF_SUCCESS;

    ble_gatts_char_md_t char_md;
    // The ble_gatts_char_md_t structure uses bit fields. So we reset the memory to zero.
    memset(&char_md, 0, sizeof(char_md));

    char_md.char_props.read                 = 1;
    char_md.char_props.write                = 1;
    char_md.char_props.write_wo_resp        = 0;
    char_md.char_props.notify               = 0;
    char_md.char_props.indicate             = 1;
    char_md.p_char_user_desc                = NULL;
    char_md.p_char_pf                       = NULL;
    char_md.p_user_desc_md                  = NULL;
    char_md.p_cccd_md                       = NULL;
    char_md.p_sccd_md                       = NULL;

    ble_gatts_attr_md_t attr_md;
    memset(&attr_md, 0, sizeof(attr_md));
    attr_md.read_perm.lv                  = 2;
    attr_md.read_perm.sm                  = 1;
    attr_md.write_perm.lv                 = 2;
    attr_md.write_perm.sm                 = 1;
    attr_md.vloc                          = BLE_GATTS_VLOC_STACK;
    attr_md.rd_auth                       = 0;
    attr_md.wr_auth                       = 1;
    attr_md.vlen                          = 0;


    ble_gatts_attr_t attr_char_value;
    uint8_t empty[22] = {0};
    ble_uuid_t charUuid = {0};
    charUuid.type               = UUID_TYPE_DEXCOM_BASE;
    charUuid.uuid               = UUID_16_LEGACY_SERVICE_TEST_CHAR;
    memset(&attr_char_value, 0, sizeof(attr_char_value));
    attr_char_value.p_uuid      = &charUuid;
    attr_char_value.p_attr_md   = &attr_md;
    attr_char_value.init_len    = 12;
    attr_char_value.max_len     = 12;
    attr_char_value.p_value     = empty;

    err_code = sd_ble_gatts_characteristic_add(s_serviceHandle, &char_md, &attr_char_value, &_dexcom_charHandles);

    if(NRF_SUCCESS !=  err_code)
    {
      dbgmsg("Failed sd_ble_gatts_characteristic_add: 0x%x\n", err_code);
      return err_code;
    }

    return NRF_SUCCESS;
}

void handle_ble_event(void* arg)
{
  uint32_t err_code = NRF_SUCCESS;
  if(!_ble_evnt_array_indx) 
  {
    dbgmsg("handle_ble_event::_ble_evnt_array_indx == 0\n");
    return;
  }

  ble_evt_t  ble_evt;
  uint8_t critical_or_not = 0, req_indx = 0;

//  sd_nvic_critical_region_enter(&critical_or_not);
	CRITICAL_REGION_ENTER();
  req_indx = --_ble_evnt_array_indx;
  sd_nvic_critical_region_exit(critical_or_not);
	CRITICAL_REGION_EXIT();

  memcpy(&ble_evt, &_ble_evnt_array[req_indx], sizeof(ble_evt_t));
  ble_evt_t * p_ble_evt = &ble_evt;
  switch(p_ble_evt->header.evt_id)
  {
    case BLE_GAP_EVT_CONNECTED:
    {
      dbgmsg("BLE_GAP_EVT_CONNECTED\n");
			bsp_board_led_off(PERIPHERAL_ADVERTISING_LED);
      bsp_board_led_on(PERIPHERAL_CONNECTED_LED);
			bsp_board_led_off(PERIPHERAL_BONDED_LED);
			bsp_board_led_off(PERIPHERAL_BOND_FAILED_LED);
			connHandle = p_ble_evt->evt.gap_evt.conn_handle;
      _peerId.id_addr_info = p_ble_evt->evt.gap_evt.params.connected.peer_addr;
      break;
    }
    case BLE_GAP_EVT_DISCONNECTED:
    {
      uint32_t err_code = sd_ble_gatts_sys_attr_get(p_ble_evt->evt.gap_evt.conn_handle, _sys_attr_data, &_sys_attr_data_len, 0);
      dbgmsg("len of sys data: %d, return val of sd_ble_gatts_sys_attr_get: 0x%x\n ", _sys_attr_data_len, err_code);
			connHandle = BLE_CONN_HANDLE_INVALID;
      dbgmsg("BLE_GAP_EVT_DISCONNECTED:::Advertising again\n");
			bsp_board_led_on(PERIPHERAL_ADVERTISING_LED);
			bsp_board_led_off(PERIPHERAL_CONNECTED_LED);
			bsp_board_led_off(PERIPHERAL_BONDED_LED);
      start_advertising();

      break;
    }
    case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
    {
      dbgmsg("BLE_GAP_EVT_SEC_PARAMS_REQUEST\n");
      if(p_ble_evt->evt.gap_evt.params.sec_params_request.peer_params.lesc == 1)
      {
        dxcm_errno_t err_no = dxcm_ecdh_create(&_le_ecdh_ctxt);
        
        if(dxcm_errno_ok != err_no)
        {
          dbgmsg("dxcm_ecdh_create failed, %d\n", err_no);
          sd_ble_gap_disconnect(p_ble_evt->evt.gap_evt.conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
          return;
        }
      }
      else
      {
        dbgmsg("Performing Legacy pairing\n");
        m_securityParams.lesc = 0;
      }

      err_code = sd_ble_gap_sec_params_reply(p_ble_evt->evt.gap_evt.conn_handle,BLE_GAP_SEC_STATUS_SUCCESS,&m_securityParams, &_keys_exchanged);
      if(NRF_SUCCESS != err_code)
      {
        dbgmsg("Failed sd_ble_gap_sec_params_reply, 0x%x\n", err_code);
        sd_ble_gap_disconnect(p_ble_evt->evt.gap_evt.conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
      }

      m_securityParams.lesc = 1;
      break;
    }
    case BLE_GAP_EVT_LESC_DHKEY_REQUEST:
    {
      if (memcmp(p_ble_evt->evt.gap_evt.params.lesc_dhkey_request.p_pk_peer, _le_ecdh_ctxt.remote_public_key,BLE_GAP_LESC_P256_PK_LEN))
      {
        dbgmsg("##########PEER PUBLIC KEY MISMATCH!!\n");
      }
      else
      {
        dbgmsg("Right Peer public key\n");
      }
      dbgmsg("BLE_GAP_EVT_LESC_DHKEY_REQUEST\n");
     
      if(dxcm_errno_ok == dxcm_ecdh_compute_shared_secret(&_le_ecdh_ctxt))
      {
        uint32_t err_code = sd_ble_gap_lesc_dhkey_reply(p_ble_evt->evt.gatts_evt.conn_handle, (ble_gap_lesc_dhkey_t*)&_le_shared_secret);
        if(NRF_SUCCESS != err_code)	
        {
          dbgmsg("sd_ble_gap_lesc_dhkey_reply failed: 0x%x \n", err_code);
          sd_ble_gap_disconnect(p_ble_evt->evt.gap_evt.conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
        }
      }
      else
      {
        dbgmsg("dxcm_ecdh_compute_shared_secret failed: \n");
        sd_ble_gap_disconnect(p_ble_evt->evt.gap_evt.conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
      }

      break;
    }
    case BLE_GAP_EVT_SEC_INFO_REQUEST:
    {
      dbgmsg("BLE_GAP_EVT_SEC_INFO_REQUEST\n");
      if(!_bond_completed)
      {
        dbgmsg("bond not present in server..Attempting fresh bond\n");
        err_code = sd_ble_gap_sec_info_reply(p_ble_evt->evt.gap_evt.conn_handle, NULL, &_peerId.id_info, 0);
        return;
      }
      else
      {
        dbgmsg("*****************************RESTORING BOND\n");
				bsp_board_led_on(PERIPHERAL_BONDED_LED);
			  bsp_board_led_off(PERIPHERAL_BOND_FAILED_LED);
        err_code = sd_ble_gap_sec_info_reply(p_ble_evt->evt.gap_evt.conn_handle, &_encryptionKey.enc_info, &_peerId.id_info, 0);
        uint32_t err_code = sd_ble_gatts_sys_attr_set(p_ble_evt->evt.gap_evt.conn_handle, _sys_attr_data, _sys_attr_data_len, 0);
        if(NRF_SUCCESS != err_code) dbgmsg("Failed to set System attributes to default state: 0x%x\n", err_code);
      }

      if(NRF_SUCCESS != err_code)
      {
        dbgmsg("Failed sd_ble_gap_sec_params_reply, 0x%x\n", err_code);
        sd_ble_gap_disconnect(p_ble_evt->evt.gap_evt.conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
      }      
      break;
    }
    case BLE_GAP_EVT_AUTH_STATUS:
    {
      dbgmsg("BLE_GAP_EVT_AUTH_STATUS\n");
      if(BLE_GAP_SEC_STATUS_SUCCESS != p_ble_evt->evt.gap_evt.params.auth_status.auth_status)
      {
        dbgmsg("###############################FAILED DURING PAIRING!!: 0x%x \n", p_ble_evt->evt.gap_evt.params.auth_status.auth_status);
				bsp_board_led_off(PERIPHERAL_BONDED_LED);
			  bsp_board_led_on(PERIPHERAL_BOND_FAILED_LED);
        /*Disconnect from the client if pairing fails. We dont want the connection to timeout*/
        sd_ble_gap_disconnect(p_ble_evt->evt.gap_evt.conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
      }
      else if(0 != p_ble_evt->evt.gap_evt.params.auth_status.bonded)
      {
          _bond_completed = true;
          dbgmsg("*****************************BONDING COMPLETED\n");
					bsp_board_led_on(PERIPHERAL_BONDED_LED);
					bsp_board_led_off(PERIPHERAL_BOND_FAILED_LED);
      }
      break;
    }
    case BLE_GATTS_EVT_SYS_ATTR_MISSING:
    {
      dbgmsg("BLE_GATTS_EVT_SYS_ATTR_MISSING\n");
      sd_ble_gatts_sys_attr_set(p_ble_evt->evt.gap_evt.conn_handle, 0, 0, 0);
      break;
    }
    case BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST:
    {
      if(BLE_GATTS_AUTHORIZE_TYPE_WRITE == p_ble_evt->evt.gatts_evt.params.authorize_request.type)
      {
				dbgmsg("BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST\n");
        ble_gatts_rw_authorize_reply_params_t params;
        static uint8_t data[31];
        memcpy(data, p_ble_evt->evt.gatts_evt.params.authorize_request.request.write.data, p_ble_evt->evt.gatts_evt.params.authorize_request.request.write.len);
        params.type = BLE_GATTS_AUTHORIZE_TYPE_WRITE;
        params.params.write.gatt_status = BLE_GATT_STATUS_SUCCESS;
        params.params.write.update = 1; //0 != p_ble_evt->evt.gatts_evt.params.authorize_request.request.write.len ? 1 : 0;
        params.params.write.offset = 0;
        params.params.write.len = p_ble_evt->evt.gatts_evt.params.authorize_request.request.write.len;
        params.params.write.p_data = data;

        uint32_t err_code = sd_ble_gatts_rw_authorize_reply(p_ble_evt->evt.gatts_evt.conn_handle, &params);

        if(NRF_SUCCESS != err_code) 
        {
          dbgmsg("FAIL  sd_ble_gatts_rw_authorize_reply::0x%x\n",err_code );
          return;
        }

        if(p_ble_evt->evt.gatts_evt.params.authorize_request.request.write.handle == _dexcom_charHandles.value_handle)
        {
          dbgmsg("Recvd BLE_GATTS_AUTHORIZE_TYPE_WRITE evnt...writing back data\n");
      
          static uint32_t cntr = 1;
          static uint8_t temp_val[4] = {0};
          ble_gatts_value_t value;
          value.len       = 4;
          value.offset    = 0;
          temp_val[0] = cntr & 0x000000FF;
          temp_val[1] = (cntr & 0x0000FF00) >> 8;
          temp_val[2] = (cntr & 0x00FF0000) >> 16;
          temp_val[3] = (cntr & 0xFF000000) >> 24;
          value.p_value   = temp_val;
          cntr++;
          err_code = sd_ble_gatts_value_set(BLE_CONN_HANDLE_INVALID, _dexcom_charHandles.value_handle, &value);
          if(NRF_SUCCESS != err_code)
          {
            dbgmsg("Failed sd_ble_gatts_value_set, 0x%x\n", err_code);
          }

          ble_gatts_hvx_params_t hvxValue;
          hvxValue.type = BLE_GATT_HVX_INDICATION;
          uint16_t inOutLength = 0;
          hvxValue.handle = _dexcom_charHandles.value_handle;
          hvxValue.offset = 0;
          hvxValue.p_len = &inOutLength;
          hvxValue.p_data = 0;
          err_code = sd_ble_gatts_hvx(p_ble_evt->evt.gatts_evt.conn_handle, &hvxValue);
          if(NRF_SUCCESS != err_code)
          {
            dbgmsg("Failed sd_ble_gatts_hvx, 0x%x\n", err_code);
          }
        }
      }
      else
      {
        dbgmsg("Unknown BLE_GATTS_EVT_RW_AUTHORIZE_REQUEST\n");
      }
      break;
    }
    case BLE_GAP_EVT_CONN_SEC_UPDATE:
    {
      dbgmsg("BLE_GAP_EVT_CONN_SEC_UPDATE\n");
      break;
    }
    case BLE_GATTS_EVT_WRITE:
    {
      dbgmsg("BLE_GATTS_EVT_WRITE\n");
      dbgmsg("UUID: 0x%X, Type: %u, Type of Write: %u, Writing deferred due to auth?: %u\n Data: ", 
      p_ble_evt->evt.gatts_evt.params.write.uuid.uuid, 
      p_ble_evt->evt.gatts_evt.params.write.uuid.type,
      p_ble_evt->evt.gatts_evt.params.write.op,
      p_ble_evt->evt.gatts_evt.params.write.auth_required);
      uint8_t* pointer = p_ble_evt->evt.gatts_evt.params.write.data;
      for (uint16_t i=p_ble_evt->evt.gatts_evt.params.write.len; i>0; i--)
      {
        dbgmsg("%d ",*pointer++);
      }
      dbgmsg("\n");
      break;
    }
    case BLE_GATTS_EVT_HVC:
    {
      dbgmsg("BLE_GATTS_EVT_HVC\n");
      break;
    }

    case SD_BLE_GATTS_SYS_ATTR_SET:
    {
			dbgmsg("SD_BLE_GATTS_SYS_ATTR_SET\n");
      break;
    }

    case BLE_GATTS_EVT_EXCHANGE_MTU_REQUEST:
    {
      uint16_t client_rx_mtu = p_ble_evt->evt.gatts_evt.params.exchange_mtu_request.client_rx_mtu;
      dbgmsg("BLE_GATTS_EVT_EXCHANGE_MTU_REQUEST\n");
      dbgmsg("Recvd client_rx_mtu: %d\n",client_rx_mtu);
      sd_ble_gatts_exchange_mtu_reply(p_ble_evt->evt.gatts_evt.conn_handle, s_ble_cfg.conn_cfg.params.gatt_conn_cfg.att_mtu);
      dbgmsg("Replyed with mtu: %d\n", s_ble_cfg.conn_cfg.params.gatt_conn_cfg.att_mtu);
      break;
    }

    case BLE_GATTS_EVT_TIMEOUT:
    {
      dbgmsg("BLE_GATTS_EVT_TIMEOUT\n");
      break;
    }
    
    case BLE_GAP_EVT_DATA_LENGTH_UPDATE:
    {
      dbgmsg("BLE_GAP_EVT_DATA_LENGTH_UPDATE\n");
      break;
    }

    case BLE_GAP_EVT_DATA_LENGTH_UPDATE_REQUEST:
    {
			dbgmsg("BLE_GAP_EVT_DATA_LENGTH_UPDATE_REQUEST\n");
      ble_gap_data_length_params_t dlp;
      ble_gap_data_length_limitation_t limit;
      
      const uint8_t LL_HEADER_LEN = 4;
      uint16_t max_dll_length = LL_HEADER_LEN + s_ble_cfg.conn_cfg.params.gatt_conn_cfg.att_mtu;
      
      dlp.max_rx_octets = p_ble_evt->evt.gap_evt.params.data_length_update_request.peer_params.max_rx_octets;
      dlp.max_rx_octets = (dlp.max_rx_octets > max_dll_length) ? max_dll_length : dlp.max_rx_octets;
      
      dlp.max_tx_octets = p_ble_evt->evt.gap_evt.params.data_length_update_request.peer_params.max_tx_octets;
      dlp.max_tx_octets = (dlp.max_tx_octets > max_dll_length) ? max_dll_length : dlp.max_tx_octets;                  

      dlp.max_rx_time_us = BLE_GAP_DATA_LENGTH_AUTO;
      dlp.max_tx_time_us = BLE_GAP_DATA_LENGTH_AUTO;

      //dbgmsg("Updating data length tx: %u, rx:%u bytes on connection 0x%x.\n", dlp.max_tx_octets, dlp.max_rx_octets , p_ble_evt->evt.gap_evt.conn_handle);

      uint32_t err_code = sd_ble_gap_data_length_update(p_ble_evt->evt.gap_evt.conn_handle, &dlp, &limit);

      if (err_code != NRF_SUCCESS)
      {
          dbgmsg("sd_ble_gap_data_length_update() (reply)"
                        " returned unexpected value 0x%x, %d, %d, %d\n",
                        err_code,limit.rx_payload_limited_octets, limit.tx_payload_limited_octets, limit.tx_rx_time_limited_us );
        
      }

      break;
    }

    case BLE_GAP_EVT_CONN_PARAM_UPDATE:
		{
			/*Get the conn params that the connection now supports*/

			ble_gap_conn_params_t* conn_params = &(p_ble_evt->evt.gap_evt.params.conn_param_update.conn_params);
			dbgmsg("BLE_GAP_EVT_CONN_PARAM_UPDATE::min_CI::%u, max_CI::%u,SL::%u,SUP:%u \n", (conn_params->min_conn_interval),
																												(conn_params->max_conn_interval),
																												(conn_params->slave_latency),
																												(conn_params->conn_sup_timeout) );

			break;
		}
		case BLE_EVT_USER_MEM_REQUEST:
		{
			dbgmsg("BLE_EVT_USER_MEM_REQUEST\n");
			break;
		}
    default:
    {
      dbgmsg("Unknown event, %d \n", p_ble_evt->header.evt_id);
      break;
    }
  }  

}

uint8_t start_ble(uint8_t* server_id, uint8_t len)
{
  if(MAX_ADV_LENGTH < len + 2)
  {
    dbgmsg("Adv payload greater than 29 bytes..Rejecting payload\n");
    return 0;
  }

  uint32_t err_code = init_dex_characteristic();
  if(NRF_SUCCESS != err_code)
  {
    dbgmsg("Failed init_dex_characteristic, 0x%x\n", err_code);
    return 0;
  }

  dbgmsg("Initiailized BLE test characteristic::UUID: 0x%x\n", UUID_16_LEGACY_SERVICE_TEST_CHAR);

#define DEVICE_NAME_LEN 8

  uint8_t curr_indx = 0;
  /*Prepare adv packet and start advertising*/
  advData[curr_indx] = DEVICE_NAME_LEN + 1;
  advData[curr_indx+1] = ASSIGNED_GAP_COMPLETE_LOCAL_NAME;
  memcpy(&advData[curr_indx+2], server_id, DEVICE_NAME_LEN);

  /*Add the 0xFEBC dexcom UUID into the advertisement packet*/
  curr_indx = DEVICE_NAME_LEN + 2;
  advData[curr_indx] = 3;
  advData[curr_indx + 1] = ASSIGNED_GAP_INCOMPLETE_UUID_16_BIT;
  advData[curr_indx + 2] = 0xBC;
  advData[curr_indx + 3] = 0xFE;

  if(NRF_SUCCESS == start_advertising())  
  {
    dbgmsg("Advertising for connections..\n");
			bsp_board_led_on(PERIPHERAL_ADVERTISING_LED);
      bsp_board_led_off(PERIPHERAL_CONNECTED_LED);
			bsp_board_led_off(PERIPHERAL_BONDED_LED);
			bsp_board_led_off(PERIPHERAL_BOND_FAILED_LED);
    return 1;
  }
  return 0;
}

void handle_ble_event_interrput_ctxt(void* arg)
{
  
}

uint16_t get_connection_handle()
{
  return connHandle;
}
