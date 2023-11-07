/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "system.h"
#include "esp_blufi_api.h"

#include "blufi_internal.h"

extern wifi_config_t sta_config;
extern wifi_config_t ap_config;

extern bool gl_sta_connected;
extern bool gl_sta_got_ip;
extern bool ble_is_connected;
extern uint8_t gl_sta_bssid[6];
extern uint8_t gl_sta_ssid[32];
extern int gl_sta_ssid_len;
extern wifi_sta_list_t gl_sta_list;
extern bool gl_sta_is_connecting;
extern esp_blufi_extra_info_t gl_sta_conn_info;


void blufi_dh_negotiate_data_handler(uint8_t *data, int len, uint8_t **output_data, int *output_len, bool *need_free);
int blufi_aes_encrypt(uint8_t iv8, uint8_t *crypt_data, int crypt_len);
int blufi_aes_decrypt(uint8_t iv8, uint8_t *crypt_data, int crypt_len);
uint16_t blufi_crc_checksum(uint8_t iv8, uint8_t *data, int len);

int blufi_security_init(void);
void blufi_security_deinit(void);
int esp_blufi_gap_register_callback(void);
int esp_blufi_host_init(void);
int esp_blufi_host_and_cb_init(esp_blufi_callbacks_t *callbacks);
int esp_blufi_host_deinit(void);

void blufi_get_ble_address(uint8_t* addr);
int blufi_get_ble_connection_state(void);
int blufi_get_ble_connection_number(void);

int blufi_wifi_connect(void);
void blufi_get_wifi_address(uint8_t* addr);
int blufi_get_wifi_connection_state(void);
int blufi_get_wifi_active(void);

int blufi_ble_init(void);
int blufi_ble_deinit(void);

int blufi_wifi_init(void);
int blufi_wifi_denit(void);

int blufi_adv_start(void);
int blufi_adv_stop(void);

int blufi_ap_start(void);
int blufi_ap_stop(void);

int blufi_ota_start(void);

int softap_get_current_connection_number(void);
int wifi_connect_to_server_tcp(void);
