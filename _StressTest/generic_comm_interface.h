/* Copyright 2018 by Dexcom, Inc.  All rights reserved. */
#pragma once

#include <stdint.h>
#include "ble.h"

#define MAX_ADV_LENGTH  31
#define MAX_QUEUED_EVNT_COUNT 10

extern uint16_t connHandle;
uint32_t register_dexcom_service(void);

/*This method is used to configure the soft device. If the name of the server is provided, it is set, but if no value is provided,
the device shows up with the default name - The name is specified when configuring the soft device as part of running a server, whereas 
it is not specified when configuring the soft device for client configuration*/
uint32_t config_sd(uint8_t* server_id, uint8_t len);

/*This method starts BLE activity. If the server configuration is enabled, this method sets up the device name and starts advertising etc
If the client configuration is enabled, this method sets the device name, starts scanning etc*/
uint8_t start_ble(uint8_t* server_id, uint8_t len);

/*This function is responsible for handling events received from the BLE stack. Different implementations exist based on whether we are 
configured as a client or as a server */
void handle_ble_event(void* arg);

void handle_ble_event_interrput_ctxt(void* arg);

uint16_t get_connection_handle(void);

void ble_evt_dispatch_srv(ble_evt_t * p_data);

void force_disconnect(void);
