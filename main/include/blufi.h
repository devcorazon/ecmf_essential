/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "system.h"
#include "esp_blufi_api.h"
#include "esp_wps.h"

#include "blufi_internal.h"

void blufi_dh_negotiate_data_handler(uint8_t *data, int len, uint8_t **output_data, int *output_len, bool *need_free);
int blufi_aes_encrypt(uint8_t iv8, uint8_t *crypt_data, int crypt_len);
int blufi_aes_decrypt(uint8_t iv8, uint8_t *crypt_data, int crypt_len);
uint16_t blufi_crc_checksum(uint8_t iv8, uint8_t *data, int len);

int blufi_security_init_with_key(const uint8_t *key);
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

int blufi_wifi_send_voluntary(uint8_t funct, uint16_t obj_id, uint16_t index);

int blufi_ble_init(void);
int blufi_ble_deinit(void);

int blufi_wifi_init(void);
int blufi_wifi_deinit(void);

int blufi_adv_start(void);
int blufi_adv_stop(void);

int blufi_ap_start(void);
int blufi_ap_stop(void);

int blufi_ota_start(void);

int softap_get_current_connection_number(void);
int tcp_connect_to_server(void);
int tcp_close_reconnect(void);
int tcp_send_data(const uint8_t *data, size_t len);

void tcp_receive_data_task(void *pvParameters);
void send_voluntary_task(void *pvParameters);

// Getters & Setters
esp_wps_config_t get_wps_config(void);
void set_wps_config(const esp_wps_config_t* config);

bool get_sta_connected(void);
void set_sta_connected(bool value);

bool get_sta_got_ip(void);
void set_sta_got_ip(bool value);

bool get_tcp_connected(void);
void set_tcp_connected(bool value);

bool get_ble_is_connected(void);
void set_ble_is_connected(bool value);

uint8_t* get_sta_bssid(void);
void set_sta_bssid(const uint8_t* bssid);

uint8_t* get_sta_ssid(void);
void set_sta_ssid(const uint8_t* ssid,uint8_t ssid_length);

int get_sta_ssid_len(void);
void set_sta_ssid_len(int len);

wifi_sta_list_t get_sta_list(void);
void set_sta_list(const wifi_sta_list_t* list);

bool get_sta_is_connecting(void);
void set_sta_is_connecting(bool value);

esp_blufi_extra_info_t* get_sta_conn_info(void);
void set_sta_conn_info(const esp_blufi_extra_info_t* info);

wifi_config_t get_sta_config(void);
void set_sta_config(const wifi_config_t* config);

wifi_config_t get_ap_config(void);
void set_ap_config(const wifi_config_t* config);

bool get_wps_is_enabled(void);
void set_wps_is_enabled(bool value);
