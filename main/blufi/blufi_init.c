/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "esp_blufi_api.h"
#include "esp_blufi.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"

#include "blufi.h"

int esp_blufi_host_init(void) {
    int ret;

    ret = esp_bluedroid_init();
    if (ret) {
        printf("%s init bluedroid failed: %s\n", __func__, esp_err_to_name(ret));
        return -1;
    }

    ret = esp_bluedroid_enable();
    if (ret) {
        printf("%s init bluedroid failed: %s\n", __func__, esp_err_to_name(ret));
        return -1;
    }

    printf("BD ADDR: "ESP_BD_ADDR_STR"\n", ESP_BD_ADDR_HEX(esp_bt_dev_get_address()));
    return 0;
}

int esp_blufi_host_deinit(void) {
    int ret;

    ret = esp_blufi_profile_deinit();
    if (ret != 0) {
        return ret;
    }

    ret = esp_bluedroid_disable();
    if (ret) {
        printf("%s deinit bluedroid failed: %s\n", __func__, esp_err_to_name(ret));
        return -1;
    }

    ret = esp_bluedroid_deinit();
    if (ret) {
        printf("%s deinit bluedroid failed: %s\n", __func__, esp_err_to_name(ret));
        return -1;
    }

    // Assumption: esp_bt_controller_disable() returns an int (0 for success)
    ret = esp_bt_controller_disable();
    if (ret) {
        printf("%s deinit bluedroid failed: %s\n", __func__, esp_err_to_name(ret));
        return -1;
    }

    ret = esp_bt_controller_deinit();
    if (ret) {
        printf("%s deinit bluedroid failed: %s\n", __func__, esp_err_to_name(ret));
        return -1;
    }

    return 0;
}

int esp_blufi_gap_register_callback(void) {
    int rc;
    rc = esp_ble_gap_register_callback(esp_blufi_gap_event_handler);
    if(rc) {
        return rc;
    }
    return esp_blufi_profile_init();
}

int esp_blufi_host_and_cb_init(esp_blufi_callbacks_t *callbacks) {
    int ret;

    ret = esp_blufi_host_init();
    if (ret) {
        printf("%s initialise host failed: %s\n", __func__, esp_err_to_name(ret));
        return ret;
    }

    ret = esp_blufi_register_callbacks(callbacks);
    if(ret) {
        printf("%s blufi register failed, error code = %x\n", __func__, ret);
        return ret;
    }

    ret = esp_blufi_gap_register_callback();
    if(ret) {
        printf("%s gap register failed, error code = %x\n", __func__, ret);
        return ret;
    }

    return 0;
}
