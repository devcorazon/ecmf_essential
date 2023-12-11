/*
 * SPDX-FileCopyrightText: 2017-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sdkconfig.h"
#include "esp_efuse.h"
#include <assert.h>
#include "esp_efuse_custom_table.h"

// md5_digest_table 7ef1e5c2db5cebbfc9b0d8794e72e74a
// This file was generated from the file esp_efuse_custom_table.csv. DO NOT CHANGE THIS FILE MANUALLY.
// If you want to change some fields, you need to change esp_efuse_custom_table.csv file
// then run `efuse_common_table` or `efuse_custom_table` command it will generate this file.
// To show efuse_table run the command 'show_efuse_table'.

static const esp_efuse_desc_t USER_DATA_UNLOCKED[] = {
    {EFUSE_BLK3, 216, 8}, 	 // Unlocked,
};

static const esp_efuse_desc_t USER_DATA_SERIAL_NUMBER[] = {
    {EFUSE_BLK3, 224, 32}, 	 // Serial Number,
};


const esp_efuse_desc_t* ESP_EFUSE_USER_DATA_UNLOCKED[] = {
    &USER_DATA_UNLOCKED[0],    		// Unlocked
    NULL
};

const esp_efuse_desc_t* ESP_EFUSE_USER_DATA_SERIAL_NUMBER[] = {
    &USER_DATA_SERIAL_NUMBER[0],    		// Serial Number
    NULL
};
