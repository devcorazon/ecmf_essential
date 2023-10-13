/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "system.h"
#include "esp_blufi_api.h"

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
//int blufi_ap_init();
//int blufi_sta_init();
int blufi_init(uint8_t mode);
int blufi_deinit();

