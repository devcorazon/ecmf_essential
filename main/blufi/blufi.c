/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */


/****************************************************************************
* This is a demo for bluetooth config wifi connection to ap. You can config ESP32 to connect a softap
* or config ESP32 as a softap to be connected by other device. APP can be downloaded from github
* android source code: https://github.com/EspressifApp/EspBlufi
* iOS source code: https://github.com/EspressifApp/EspBlufiForiOS
****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_blufi_api.h"
#include "esp_blufi.h"
#include "esp_err.h" // for error handling

#include "../include/blufi.h"
#include "storage.h"

#define WIFI_CONNECTION_MAXIMUM_RETRY 4
#define INVALID_REASON                255
#define INVALID_RSSI                  -128

#define STA_ESP_WIFI_SSID     "SFR_C798"
#define STA_ESP_WIFI_PASS     "73jhee8dkn95iraj7mpq"
#define STA_ESP_MAXIMUM_RETRY  4

#define AP_ESP_WIFI_SSID      "ECMF-{serial_number}"
#define AP_ESP_WIFI_PASS      "mypassword"
#define AP_ESP_WIFI_CHANNEL   1

#define BLUFI_TASK_STACK_SIZE			(configMINIMAL_STACK_SIZE * 4)
#define	BLUFI_TASK_PRIORITY		     	(1)
#define	BLUFI_TASK_PERIOD				(100ul / portTICK_PERIOD_MS)

#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK

static bool is_bt_mem_released = false;

static esp_netif_t *ap_netif = NULL;
static esp_netif_t *sta_netif = NULL;

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

static void event_callback(esp_blufi_cb_event_t event, esp_blufi_cb_param_t *param);

static esp_blufi_callbacks_t callbacks = {
    .event_cb = event_callback,
    .negotiate_data_handler = blufi_dh_negotiate_data_handler,
    .encrypt_func = blufi_aes_encrypt,
    .decrypt_func = blufi_aes_decrypt,
    .checksum_func = blufi_crc_checksum,
};

#define WIFI_LIST_NUM   10

static wifi_config_t sta_config;
static wifi_config_t ap_config;

enum wifi_state {
    WIFI_STA = 1,
    WIFI_AP = 2,
};

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
static uint8_t wifi_retry = 0;

/* store the station info for send back to phone */
static bool gl_sta_connected = false;
static bool gl_sta_got_ip = false;
static bool ble_is_connected = false;
static uint8_t gl_sta_bssid[6];
static uint8_t gl_sta_ssid[32];
static int gl_sta_ssid_len;
static wifi_sta_list_t gl_sta_list;
static bool gl_sta_is_connecting = false;
static esp_blufi_extra_info_t gl_sta_conn_info;

static void record_wifi_conn_info(int rssi, uint8_t reason) {
    memset(&gl_sta_conn_info, 0, sizeof(esp_blufi_extra_info_t));
    if (gl_sta_is_connecting) {
        gl_sta_conn_info.sta_max_conn_retry_set = true;
        gl_sta_conn_info.sta_max_conn_retry = WIFI_CONNECTION_MAXIMUM_RETRY;
    } else {
        gl_sta_conn_info.sta_conn_rssi_set = true;
        gl_sta_conn_info.sta_conn_rssi = rssi;
        gl_sta_conn_info.sta_conn_end_reason_set = true;
        gl_sta_conn_info.sta_conn_end_reason = reason;
    }
}

static void wifi_connect(void) {
    wifi_retry = 0;
    gl_sta_is_connecting = (esp_wifi_connect() == ESP_OK);
    record_wifi_conn_info(INVALID_RSSI, INVALID_REASON);
}

static bool wifi_reconnect(void) {
    bool ret;
    if (gl_sta_is_connecting && wifi_retry++ < WIFI_CONNECTION_MAXIMUM_RETRY) {
    	printf("BLUFI WiFi starts reconnection\n");
        gl_sta_is_connecting = (esp_wifi_connect() == ESP_OK);
        record_wifi_conn_info(INVALID_RSSI, INVALID_REASON);
        ret = true;
    } else {
        ret = false;
    }
    return ret;
}

static int softap_get_current_connection_number(void) {
    esp_err_t ret;
    ret = esp_wifi_ap_get_sta_list(&gl_sta_list);
    if (ret == ESP_OK)
    {
        return gl_sta_list.num;
    }

    return 0;
}

static void ip_event_handler(void* arg, esp_event_base_t event_base,int32_t event_id, void* event_data) {
    wifi_mode_t mode;

    switch (event_id) {
    case IP_EVENT_STA_GOT_IP: {
        esp_blufi_extra_info_t info;

        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        esp_wifi_get_mode(&mode);

        memset(&info, 0, sizeof(esp_blufi_extra_info_t));
        memcpy(info.sta_bssid, gl_sta_bssid, 6);
        info.sta_bssid_set = true;
        info.sta_ssid = gl_sta_ssid;
        info.sta_ssid_len = gl_sta_ssid_len;
        gl_sta_got_ip = true;
        if (ble_is_connected == true) {
            esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONN_SUCCESS, softap_get_current_connection_number(), &info);
        } else {
        	printf("BLUFI BLE is not connected yet\n");
        }
        break;
    }
    default:
        break;
    }
    return;
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,int32_t event_id, void* event_data) {
    wifi_event_sta_connected_t *event;
    wifi_event_sta_disconnected_t *disconnected_event;
    wifi_mode_t mode;
    int status = 0;

    switch (event_id) {
    case WIFI_EVENT_STA_START:
        wifi_connect();
        break;
    case WIFI_EVENT_STA_CONNECTED:
        gl_sta_connected = true;
        gl_sta_is_connecting = false;
        event = (wifi_event_sta_connected_t*) event_data;
        memcpy(gl_sta_bssid, event->bssid, 6);
        memcpy(gl_sta_ssid, event->ssid, event->ssid_len);
        gl_sta_ssid_len = event->ssid_len;
        break;
    case WIFI_EVENT_STA_DISCONNECTED:
        if (!gl_sta_connected && !wifi_reconnect()) {
            gl_sta_is_connecting = false;
            disconnected_event = (wifi_event_sta_disconnected_t*) event_data;
            record_wifi_conn_info(disconnected_event->rssi, disconnected_event->reason);
        }
        gl_sta_connected = false;
        gl_sta_got_ip = false;
        memset(gl_sta_ssid, 0, 32);
        memset(gl_sta_bssid, 0, 6);
        gl_sta_ssid_len = 0;
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
        break;
    case WIFI_EVENT_AP_START:
        if (esp_wifi_get_mode(&mode) != ESP_OK) {
            status = -1;
            break;
        }
        if (ble_is_connected) {
            esp_blufi_extra_info_t info;
            memset(&info, 0, sizeof(esp_blufi_extra_info_t));
            memcpy(info.sta_bssid, gl_sta_bssid, 6);
            info.sta_bssid_set = true;
            info.sta_ssid = gl_sta_ssid;
            info.sta_ssid_len = gl_sta_ssid_len;
            esp_blufi_send_wifi_conn_report(mode, gl_sta_got_ip ? ESP_BLUFI_STA_CONN_SUCCESS : ESP_BLUFI_STA_NO_IP, softap_get_current_connection_number(), &info);
        } else {
            printf("BLUFI BLE is not connected yet\n");
        }
        break;
    case WIFI_EVENT_SCAN_DONE: {
        uint16_t apCount = 0;
        if (esp_wifi_scan_get_ap_num(&apCount) != ESP_OK || apCount == 0) {
            printf("Nothing AP found");
            status = -1;
            break;
        }
        wifi_ap_record_t *ap_list = (wifi_ap_record_t *)malloc(sizeof(wifi_ap_record_t) * apCount);
        if (!ap_list) {
            printf("malloc error, ap_list is NULL");
            status = -1;
            break;
        }
        if (esp_wifi_scan_get_ap_records(&apCount, ap_list) != ESP_OK) {
            status = -1;
            free(ap_list);
            break;
        }
        esp_blufi_ap_record_t * blufi_ap_list = (esp_blufi_ap_record_t *)malloc(apCount * sizeof(esp_blufi_ap_record_t));
        if (!blufi_ap_list) {
            printf("malloc error, blufi_ap_list is NULL");
            status = -1;
            free(ap_list);
            break;
        }
        for (int i = 0; i < apCount; ++i) {
            blufi_ap_list[i].rssi = ap_list[i].rssi;
            memcpy(blufi_ap_list[i].ssid, ap_list[i].ssid, sizeof(ap_list[i].ssid));
        }
        if (ble_is_connected) {
            esp_blufi_send_wifi_list(apCount, blufi_ap_list);
        } else {
            printf("BLUFI BLE is not connected yet\n");
        }
        esp_wifi_scan_stop();
        free(ap_list);
        free(blufi_ap_list);
        break;
    }
    case WIFI_EVENT_AP_STACONNECTED: {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        printf("station "MACSTR" join, AID=%d", MAC2STR(event->mac), event->aid);
        break;
    }
    case WIFI_EVENT_AP_STADISCONNECTED: {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        printf("station "MACSTR" leave, AID=%d", MAC2STR(event->mac), event->aid);
        break;
    }

    default:
        break;
    }

    if (status == -1) {
        printf("Error occurred in wifi_event_handler for event_id: %d.\n", event_id);
    }
}

static void initialise_wifi_ap_mode() {
    esp_err_t err;
    wifi_config_t wifi_config = { 0 };

    // Creating the new SSID
    	uint32_t serial_number = get_serial_number();
    	char serial_number_str[9]; // 8 characters for the hexadecimal number and 1 for the null terminator
    	sprintf(serial_number_str, "%08" PRIx32, serial_number); // Convert to hex string

    	char ssid_prefix[] = "ECMF-";
    	char ssid[32]; // SSID length must be between 8-32 characters

    	snprintf(ssid, sizeof(ssid), "%s%s", ssid_prefix, serial_number_str); // Concatenate the prefix and the serial number


        strcpy((char*)wifi_config.sta.ssid, AP_ESP_WIFI_SSID);
        strcpy((char*)wifi_config.sta.password, AP_ESP_WIFI_PASS);

    // Initialize event loop and register event handlers
    err = esp_event_loop_create_default();
    if (err != ESP_OK) {
        printf("Failed to create default event loop: %s\n", esp_err_to_name(err));
        return;
    }

    err = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);
    if (err != ESP_OK) {
        printf("Failed to register WiFi event handler: %s\n", esp_err_to_name(err));
        return;
    }

    err = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_handler, NULL);
    if (err != ESP_OK) {
        printf("Failed to register IP event handler: %s\n", esp_err_to_name(err));
        return;
    }

    // Initialize network interface based on mode
	sta_netif = esp_netif_create_default_wifi_sta();
	if (!sta_netif) {
		printf("Failed to create default WiFi STA.\n");
		return;
	}

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);
    if (err != ESP_OK) {
        printf("Failed to initialize WiFi: %s\n", esp_err_to_name(err));
        return;
    }

    // Copy the SSID to the wifi_config
    memcpy(wifi_config.ap.ssid, ssid, sizeof(ssid));
    wifi_config.ap.ssid_len = strlen(ssid);

    if (strlen(AP_ESP_WIFI_PASS) == 0) {
    	    wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    // Set the mode and the WiFi configuration
    err = esp_wifi_set_mode(WIFI_AP);
    if (err != ESP_OK) {
        printf("Failed to set WiFi mode: %s\n", esp_err_to_name(err));
        return;
    }

	err = esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
	if (err != ESP_OK) {
		printf("Failed to set WiFi Config STA: %s\n", esp_err_to_name(err));
		return;
	}

    // Start WiFi
    err = esp_wifi_start();
    if (err != ESP_OK) {
        printf("Failed to start WiFi: %s\n", esp_err_to_name(err));
    }
}

static void initialise_wifi_sta_mode() {
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = STA_ESP_WIFI_SSID,
            .password = STA_ESP_WIFI_PASS,
            /* Authmode threshold resets to WPA2 as default if password matches WPA2 standards (pasword len => 8).
             * If you want to connect the device to deprecated WEP/WPA networks, Please set the threshold value
             * to WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK and set the password with length and format matching to
	     * WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK standards.
             */
            .threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    printf("wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
    	printf( "connected to ap SSID:%s password:%s",STA_ESP_WIFI_SSID, STA_ESP_WIFI_PASS);
    } else if (bits & WIFI_FAIL_BIT) {
    	printf("Failed to connect to SSID:%s, password:%s", STA_ESP_WIFI_SSID, STA_ESP_WIFI_PASS);
    } else {
    	printf("UNEXPECTED EVENT");
    }
}
static void stop_wifi_mode() {
    esp_err_t err;

    printf("Stopping WiFi access point...");

    err = esp_wifi_set_mode(WIFI_MODE_NULL);
    if (err != ESP_OK) {
        printf("Failed to set WiFi mode to NULL: %s\n", esp_err_to_name(err));
        return;
    }

    err = esp_wifi_stop();
    if (err != ESP_OK) {
        printf("Failed to stop WiFi: %s\n", esp_err_to_name(err));
        return;
    }

    err = esp_wifi_deinit();
    if (err != ESP_OK) {
        printf("Failed to deinitialize WiFi: %s\n", esp_err_to_name(err));
        return;
    }

    err = esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler);
    if (err != ESP_OK) {
        printf("Failed to unregister WiFi event handler: %s\n", esp_err_to_name(err));
        return;
    }

    err = esp_event_loop_delete_default();
    if (err != ESP_OK) {
        printf("Failed to delete default event loop: %s\n", esp_err_to_name(err));
        return;
    }

    if (ap_netif) {
        esp_netif_destroy(ap_netif);
        ap_netif = NULL;
    }
    if (sta_netif) {
        esp_netif_destroy(sta_netif);
        sta_netif = NULL;
    }
}
static void initialise_bluetooth() {
    esp_err_t ret;

    // Creating the new BT device name
    uint32_t serial_number = get_serial_number();
    char serial_number_str[9]; // 8 characters for the hexadecimal number and 1 for the null terminator
    sprintf(serial_number_str, "%08" PRIx32, serial_number); // Convert to hex string

    char bt_name_prefix[] = "ECMF-";
    char bt_device_name[32]; // BT name can be up to 248 bytes, but we keep it shorter for simplicity

    snprintf(bt_device_name, sizeof(bt_device_name), "%s%s", bt_name_prefix, serial_number_str); // Concatenate the prefix and the serial number

    if (!is_bt_mem_released) {
        ret = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
        if (ret != ESP_OK) {
            printf("%s release BT classic memory failed: %s\n", __func__, esp_err_to_name(ret));
            return;
        }
        is_bt_mem_released = true;
    }

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();

    if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_IDLE) {
        ret = esp_bt_controller_init(&bt_cfg);
        if (ret != ESP_OK) {
            printf("%s initialize controller failed: %s\n", __func__, esp_err_to_name(ret));
            return;
        }
    }

    if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_INITED) {
        ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
        if (ret != ESP_OK) {
            printf("%s enable controller failed: %s\n", __func__, esp_err_to_name(ret));
            return;
        }
    }

    // init blufi and callbacks
    ret = esp_blufi_host_and_cb_init(&callbacks);
    if (ret != ESP_OK) {
        printf("%s BLUFI initialise failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    // Set the custom Bluetooth device name
    ret = esp_ble_gap_set_device_name(bt_device_name);
    if (ret != ESP_OK) {
        printf("%s set device name failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    // Now, start BLUFi advertising
    esp_blufi_adv_start();

    printf("BT_INIT: BLUFI VERSION %04x\n", esp_blufi_get_version());
}


static void stop_bluetooth(void) {
    esp_err_t ret;

    ret = esp_blufi_host_deinit();

    if (ret != ESP_OK) {
    		printf("BT_INIT", "BLUFI de-initialise failed: %s",esp_err_to_name(ret));
    		return;
    }
}

static void event_callback(esp_blufi_cb_event_t event, esp_blufi_cb_param_t *param) {
    /* actually, should post to blufi_task handle the procedure,
     * now, as a example, we do it more simply */
    switch (event) {
    case ESP_BLUFI_EVENT_INIT_FINISH:
    	printf("BLUFI init finish\n");

        esp_blufi_adv_start();
        break;
    case ESP_BLUFI_EVENT_DEINIT_FINISH:
    	printf("BLUFI deinit finish\n");
        break;
    case ESP_BLUFI_EVENT_BLE_CONNECT:
    	printf("BLUFI ble connect\n");
        ble_is_connected = true;
        esp_blufi_adv_stop();
        blufi_security_init();
        break;
    case ESP_BLUFI_EVENT_BLE_DISCONNECT:
        printf("BLUFI ble disconnect\n");
        ble_is_connected = false;
        blufi_security_deinit();
        esp_blufi_adv_start();
        break;
    case ESP_BLUFI_EVENT_SET_WIFI_OPMODE:
        printf("BLUFI Set WIFI opmode %d\n", param->wifi_mode.op_mode);
        ESP_ERROR_CHECK( esp_wifi_set_mode(param->wifi_mode.op_mode) );
        break;
    case ESP_BLUFI_EVENT_REQ_CONNECT_TO_AP:
        printf("BLUFI requset wifi connect to AP\n");
        /* there is no wifi callback when the device has already connected to this wifi
        so disconnect wifi before connection.
        */
        esp_wifi_disconnect();
        wifi_connect();
        break;
    case ESP_BLUFI_EVENT_REQ_DISCONNECT_FROM_AP:
        printf("BLUFI requset wifi disconnect from AP\n");
        esp_wifi_disconnect();
        break;
    case ESP_BLUFI_EVENT_REPORT_ERROR:
        printf("BLUFI report error, error code %d\n", param->report_error.state);
        esp_blufi_send_error_info(param->report_error.state);
        break;
    case ESP_BLUFI_EVENT_GET_WIFI_STATUS: {
        wifi_mode_t mode;
        esp_blufi_extra_info_t info;

        esp_wifi_get_mode(&mode);

        if (gl_sta_connected) {
            memset(&info, 0, sizeof(esp_blufi_extra_info_t));
            memcpy(info.sta_bssid, gl_sta_bssid, 6);
            info.sta_bssid_set = true;
            info.sta_ssid = gl_sta_ssid;
            info.sta_ssid_len = gl_sta_ssid_len;
            esp_blufi_send_wifi_conn_report(mode, gl_sta_got_ip ? ESP_BLUFI_STA_CONN_SUCCESS : ESP_BLUFI_STA_NO_IP, softap_get_current_connection_number(), &info);
        } else if (gl_sta_is_connecting) {
            esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONNECTING, softap_get_current_connection_number(), &gl_sta_conn_info);
        } else {
            esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONN_FAIL, softap_get_current_connection_number(), &gl_sta_conn_info);
        }
        printf("BLUFI get wifi status from AP\n");

        break;
    }
    case ESP_BLUFI_EVENT_RECV_SLAVE_DISCONNECT_BLE:
        printf("blufi close a gatt connection");
        esp_blufi_disconnect();
        break;
    case ESP_BLUFI_EVENT_DEAUTHENTICATE_STA:
        /* TODO */
        break;
	case ESP_BLUFI_EVENT_RECV_STA_BSSID:
        memcpy(sta_config.sta.bssid, param->sta_bssid.bssid, 6);
        sta_config.sta.bssid_set = 1;
        esp_wifi_set_config(WIFI_IF_STA, &sta_config);
        printf("Recv STA BSSID %s\n", sta_config.sta.ssid);
        break;
	case ESP_BLUFI_EVENT_RECV_STA_SSID:
        strncpy((char *)sta_config.sta.ssid, (char *)param->sta_ssid.ssid, param->sta_ssid.ssid_len);
        sta_config.sta.ssid[param->sta_ssid.ssid_len] = '\0';
        esp_wifi_set_config(WIFI_IF_STA, &sta_config);
        printf("Recv STA SSID %s\n", sta_config.sta.ssid);
        break;
	case ESP_BLUFI_EVENT_RECV_STA_PASSWD:
        strncpy((char *)sta_config.sta.password, (char *)param->sta_passwd.passwd, param->sta_passwd.passwd_len);
        sta_config.sta.password[param->sta_passwd.passwd_len] = '\0';
        esp_wifi_set_config(WIFI_IF_STA, &sta_config);
        printf("Recv STA PASSWORD %s\n", sta_config.sta.password);
        break;
	case ESP_BLUFI_EVENT_RECV_SOFTAP_SSID:
        strncpy((char *)ap_config.ap.ssid, (char *)param->softap_ssid.ssid, param->softap_ssid.ssid_len);
        ap_config.ap.ssid[param->softap_ssid.ssid_len] = '\0';
        ap_config.ap.ssid_len = param->softap_ssid.ssid_len;
        esp_wifi_set_config(WIFI_IF_AP, &ap_config);
        printf("Recv SOFTAP SSID %s, ssid len %d\n", ap_config.ap.ssid, ap_config.ap.ssid_len);
        break;
	case ESP_BLUFI_EVENT_RECV_SOFTAP_PASSWD:
        strncpy((char *)ap_config.ap.password, (char *)param->softap_passwd.passwd, param->softap_passwd.passwd_len);
        ap_config.ap.password[param->softap_passwd.passwd_len] = '\0';
        esp_wifi_set_config(WIFI_IF_AP, &ap_config);
        printf("Recv SOFTAP PASSWORD %s len = %d\n", ap_config.ap.password, param->softap_passwd.passwd_len);
        break;
	case ESP_BLUFI_EVENT_RECV_SOFTAP_MAX_CONN_NUM:
        if (param->softap_max_conn_num.max_conn_num > 4) {
            return;
        }
        ap_config.ap.max_connection = param->softap_max_conn_num.max_conn_num;
        esp_wifi_set_config(WIFI_IF_AP, &ap_config);
        printf("Recv SOFTAP MAX CONN NUM %d\n", ap_config.ap.max_connection);
        break;
	case ESP_BLUFI_EVENT_RECV_SOFTAP_AUTH_MODE:
        if (param->softap_auth_mode.auth_mode >= WIFI_AUTH_MAX) {
            return;
        }
        ap_config.ap.authmode = param->softap_auth_mode.auth_mode;
        esp_wifi_set_config(WIFI_IF_AP, &ap_config);
        printf("Recv SOFTAP AUTH MODE %d\n", ap_config.ap.authmode);
        break;
	case ESP_BLUFI_EVENT_RECV_SOFTAP_CHANNEL:
        if (param->softap_channel.channel > 13) {
            return;
        }
        ap_config.ap.channel = param->softap_channel.channel;
        esp_wifi_set_config(WIFI_IF_AP, &ap_config);
        printf("Recv SOFTAP CHANNEL %d\n", ap_config.ap.channel);
        break;
    case ESP_BLUFI_EVENT_GET_WIFI_LIST:
        wifi_scan_config_t scanConf = {
            .ssid = NULL,
            .bssid = NULL,
            .channel = 0,
            .show_hidden = false
        };
        esp_err_t ret = esp_wifi_scan_start(&scanConf, true);
        if (ret != ESP_OK) {
            esp_blufi_send_error_info(ESP_BLUFI_WIFI_SCAN_FAIL);
        }
        break;

    case ESP_BLUFI_EVENT_RECV_CUSTOM_DATA:
        printf("Recv Custom Data %" PRIu32 "\n", param->custom_data.data_len);
        esp_log_buffer_hex("Custom Data", param->custom_data.data, param->custom_data.data_len);
        break;
	case ESP_BLUFI_EVENT_RECV_USERNAME:
        /* Not handle currently */
        break;
	case ESP_BLUFI_EVENT_RECV_CA_CERT:
        /* Not handle currently */
        break;
	case ESP_BLUFI_EVENT_RECV_CLIENT_CERT:
        /* Not handle currently */
        break;
	case ESP_BLUFI_EVENT_RECV_SERVER_CERT:
        /* Not handle currently */
        break;
	case ESP_BLUFI_EVENT_RECV_CLIENT_PRIV_KEY:
        /* Not handle currently */
        break;;
	case ESP_BLUFI_EVENT_RECV_SERVER_PRIV_KEY:
        /* Not handle currently */
        break;
    default:
        break;
    }
}

static void blufi_task(void *pvParameters) {
//	initialise_wifi_ap_mode();
//	initialise_wifi_sta_mode();
    initialise_bluetooth();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(BLUFI_TASK_PERIOD));  // Sleep for 100 ms
    }
}

int blufi_init() {
    BaseType_t task_created = xTaskCreate(blufi_task, "BLUFI task ", BLUFI_TASK_STACK_SIZE, NULL, BLUFI_TASK_PRIORITY, NULL);

    return task_created == pdPASS ? 0 : -1;
}

int blufi_deinit() {
   stop_wifi_mode();
   stop_bluetooth();

    return 0;
}

