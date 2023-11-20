/*
 * blufi_ble.c
 *
 *  Created on: 6 nov. 2023
 *      Author: youcef.benakmoume
 */


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/timers.h"

#include "esp_system.h"
#include "esp_mac.h"
#include "esp_wifi.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_blufi_api.h"
#include "esp_blufi.h"
#include "esp_wps.h"
#include "esp_err.h" // for error handling

#include "lwip/sockets.h"
#include "lwip/netdb.h"

#include "blufi.h"

#include "structs.h"
#include "messaging.h"
#include "storage.h"
#include "test.h"

static bool is_bt_mem_released = false;

esp_bt_controller_status_t bt_status;

static uint8_t blufi_service_uuid128[32] = {
    /* LSB <--------------------------------------------------------------------------------> MSB */
    //first uuid, 16bit, [12],[13] is the value
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00,
};

static esp_ble_adv_data_t blufi_adv_data = {
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = true,
    .min_interval = 0x0006, //slave connection min interval, Time = min_interval * 1.25 msec
    .max_interval = 0x0010, //slave connection max interval, Time = max_interval * 1.25 msec
    .appearance = 0x00,
    .manufacturer_len = 0,
    .p_manufacturer_data =  NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = 16,
    .p_service_uuid = blufi_service_uuid128,
    .flag = 0x6,
};

static void ble_event_callback(esp_blufi_cb_event_t event, esp_blufi_cb_param_t *param);
static int ble_connection_count = 0;

static TimerHandle_t blufi_adv_expiry_timer = NULL;

static esp_blufi_callbacks_t callbacks = {
    .event_cb = ble_event_callback,
    .negotiate_data_handler = blufi_dh_negotiate_data_handler,
    .encrypt_func = blufi_aes_encrypt,
    .decrypt_func = blufi_aes_decrypt,
    .checksum_func = blufi_crc_checksum,
};

enum ble_connection_state {
    BLE_DISCONNECTED,
    BLE_CONNECTED
};

static enum ble_connection_state current_ble_state = BLE_DISCONNECTED;

static void blufi_adv_expiry_timer_cb(TimerHandle_t xTimer) {
	bt_status = esp_bt_controller_get_status();

    if (bt_status == ESP_BT_CONTROLLER_STATUS_INITED || bt_status == ESP_BT_CONTROLLER_STATUS_ENABLED) {
    	printf("BLE ADV Timeout\n");
        blufi_adv_stop();
        blufi_ble_deinit();
    }
}

int blufi_adv_start(void) {
    esp_err_t ret;

    // Creating the new BT device name
    uint32_t serial_number = get_serial_number();
    char serial_number_str[9]; // 8 characters for the hexadecimal number and 1 for the null terminator
    sprintf(serial_number_str, "%08" PRIx32, serial_number); // Convert to hex string

    char bt_name_prefix[] = "ECMF-";
    char bt_device_name[32]; // BT name can be up to 248 bytes, but we keep it shorter for simplicity

    snprintf(bt_device_name, sizeof(bt_device_name), "%s%s", bt_name_prefix, serial_number_str); // Concatenate the prefix and the serial number

    ret = esp_blufi_host_and_cb_init(&callbacks);
	if (ret != ESP_OK) {
		printf("%s BLUFI initialise failed: %s\n", __func__, esp_err_to_name(ret));
		return -1;
	}

    // Set the custom Bluetooth device name
    ret = esp_ble_gap_set_device_name(bt_device_name);
    if (ret != ESP_OK) {
        printf("%s set device name failed: %s\n", __func__, esp_err_to_name(ret));
        return -1;
    }

    // Now, start BLUFi advertising
    esp_ble_gap_config_adv_data(&blufi_adv_data);

    printf("BLUFI VERSION %04x\n", esp_blufi_get_version());

    if (test_in_progress()) {
    	printf("STOPPING TIMER\n");
    	xTimerStop(blufi_adv_expiry_timer, 0);
    } else {
        printf("STARTING TIMER\n");
        xTimerStart(blufi_adv_expiry_timer, 0);
    }

    return 0;
}


int blufi_adv_stop(void) {
    esp_blufi_adv_stop();
    printf("BT_ADV: Stopped\n");

    return 0;
}

static void ble_event_callback(esp_blufi_cb_event_t event, esp_blufi_cb_param_t *param) {

//    if (!test_in_progress()) {
//        // Rearm the timer at the beginning of the callback for any BLE event
//        if (blufi_adv_expiry_timer != NULL) {
//            printf("REARM TIMER\n");
//            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
//            if (xTimerResetFromISR(blufi_adv_expiry_timer, &xHigherPriorityTaskWoken) != pdPASS) {
//                // Handle error
//                printf("Timer reset failed\n");
//            }
//            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
//        }
//    } else {
//        printf("BLE EVENT RECEIVED BUT TIMER NOT REARMED (test_in_progress)\n");
//    }

    /* actually, should post to blufi_task handle the procedure,
     * now, as a example, we do it more simply */
    switch (event) {
    case ESP_BLUFI_EVENT_INIT_FINISH: {
    	printf("BLUFI init finish\n");
   	    esp_ble_gap_config_adv_data(&blufi_adv_data);
        break;
    }
    case ESP_BLUFI_EVENT_DEINIT_FINISH: {
    	printf("BLUFI deinit finish\n");
        break;
    }
    case ESP_BLUFI_EVENT_BLE_CONNECT: {
    	printf("BLUFI ble connect\n");
        set_ble_is_connected(true);
        esp_blufi_adv_stop();
        blufi_security_init();
        current_ble_state = BLE_CONNECTED;
        ble_connection_count++;
        if (!test_in_progress()) {
            xTimerReset(blufi_adv_expiry_timer, 0);
        }
        break;
    }
    case ESP_BLUFI_EVENT_BLE_DISCONNECT: {
        printf("BLUFI ble disconnect\n");
        set_ble_is_connected(false);
        blufi_security_deinit();
   	    esp_ble_gap_config_adv_data(&blufi_adv_data);
   	    current_ble_state = BLE_DISCONNECTED;
   	    ble_connection_count--;
        break;
    }
    case ESP_BLUFI_EVENT_SET_WIFI_OPMODE: {
        printf("BLUFI Set WIFI opmode %d\n", param->wifi_mode.op_mode);
        esp_wifi_set_mode(param->wifi_mode.op_mode);
        break;
    }
    case ESP_BLUFI_EVENT_REQ_CONNECT_TO_AP: {
        printf("BLUFI requset wifi connect to AP\n");
        /* there is no wifi callback when the device has already connected to this wifi
        so disconnect wifi before connection.
        */
		if (get_sta_connected()) {
			esp_wifi_disconnect();
		}
		blufi_wifi_connect();
        break;
    }
    case ESP_BLUFI_EVENT_REQ_DISCONNECT_FROM_AP: {
        printf("BLUFI requset wifi disconnect from AP\n");
        esp_wifi_disconnect();
        break;
    }
    case ESP_BLUFI_EVENT_REPORT_ERROR: {
        printf("BLUFI report error, error code %d\n", param->report_error.state);
        esp_blufi_send_error_info(param->report_error.state);
        break;
    }
    case ESP_BLUFI_EVENT_GET_WIFI_STATUS: {
        wifi_mode_t mode;
        esp_blufi_extra_info_t info;

        esp_wifi_get_mode(&mode);

        if (get_sta_connected()) {
            memset(&info, 0, sizeof(esp_blufi_extra_info_t));
            memcpy(info.sta_bssid, get_sta_bssid(), BSSID_SIZE);
            info.sta_bssid_set = true;
            info.sta_ssid = get_sta_ssid();
            info.sta_ssid_len = get_sta_ssid_len();
            esp_blufi_send_wifi_conn_report(mode, get_sta_got_ip() ? ESP_BLUFI_STA_CONN_SUCCESS : ESP_BLUFI_STA_NO_IP, softap_get_current_connection_number(), &info);
        }
        else if (get_sta_is_connecting()) {
            esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONNECTING, softap_get_current_connection_number(), get_sta_conn_info());
        }
        else {
            esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONN_FAIL, softap_get_current_connection_number(), get_sta_conn_info());
        }
        printf("BLUFI get wifi status from AP\n");
        break;
    }
    case ESP_BLUFI_EVENT_RECV_SLAVE_DISCONNECT_BLE: {
        printf("blufi close a gatt connection");
        esp_blufi_disconnect();
        break;
    }
    case ESP_BLUFI_EVENT_DEAUTHENTICATE_STA:
        /* TODO */
        break;
    case ESP_BLUFI_EVENT_RECV_STA_BSSID: {
        wifi_config_t sta_config = get_sta_config();
        memcpy(sta_config.sta.bssid, param->sta_bssid.bssid, 6);
        sta_config.sta.bssid_set = 1;
        set_sta_config(&sta_config);
        esp_wifi_set_config(WIFI_IF_STA, &sta_config);
        printf("Recv STA BSSID %s\n", sta_config.sta.ssid);
        break;
    }

    case ESP_BLUFI_EVENT_RECV_STA_SSID: {
        wifi_config_t sta_config = get_sta_config();
        strncpy((char *)sta_config.sta.ssid, (char *)param->sta_ssid.ssid, param->sta_ssid.ssid_len);
        sta_config.sta.ssid[param->sta_ssid.ssid_len] = '\0';
        set_sta_config(&sta_config);
        esp_wifi_set_config(WIFI_IF_STA, &sta_config);
        printf("Recv STA SSID %s\n", sta_config.sta.ssid);
        set_ssid(sta_config.sta.ssid);
        break;
    }

    case ESP_BLUFI_EVENT_RECV_STA_PASSWD: {
        wifi_config_t sta_config = get_sta_config();
        strncpy((char *)sta_config.sta.password, (char *)param->sta_passwd.passwd, param->sta_passwd.passwd_len);
        sta_config.sta.password[param->sta_passwd.passwd_len] = '\0';
        set_sta_config(&sta_config);
        esp_wifi_set_config(WIFI_IF_STA, &sta_config);
        printf("Recv STA PASSWORD %s\n", sta_config.sta.password);
        set_password(sta_config.sta.password);
        break;
    }

    case ESP_BLUFI_EVENT_RECV_SOFTAP_SSID: {
        wifi_config_t ap_config = get_ap_config();
        strncpy((char *)ap_config.ap.ssid, (char *)param->softap_ssid.ssid, param->softap_ssid.ssid_len);
        ap_config.ap.ssid[param->softap_ssid.ssid_len] = '\0';
        ap_config.ap.ssid_len = param->softap_ssid.ssid_len;
        set_ap_config(&ap_config);
        esp_wifi_set_config(WIFI_IF_AP, &ap_config);
        printf("Recv SOFTAP SSID %s, ssid len %d\n", ap_config.ap.ssid, ap_config.ap.ssid_len);
        break;
    }

    case ESP_BLUFI_EVENT_RECV_SOFTAP_PASSWD: {
        wifi_config_t ap_config = get_ap_config();
        strncpy((char *)ap_config.ap.password, (char *)param->softap_passwd.passwd, param->softap_passwd.passwd_len);
        ap_config.ap.password[param->softap_passwd.passwd_len] = '\0';
        set_ap_config(&ap_config);
        esp_wifi_set_config(WIFI_IF_AP, &ap_config);
        printf("Recv SOFTAP PASSWORD %s len = %d\n", ap_config.ap.password, param->softap_passwd.passwd_len);
        break;
    }

    case ESP_BLUFI_EVENT_RECV_SOFTAP_MAX_CONN_NUM: {
        if (param->softap_max_conn_num.max_conn_num > 4) {
            return;
        }
        wifi_config_t temp_ap_config = get_ap_config();
        temp_ap_config.ap.max_connection = param->softap_max_conn_num.max_conn_num;
        set_ap_config(&temp_ap_config);
        esp_wifi_set_config(WIFI_IF_AP, &temp_ap_config);
        printf("Recv SOFTAP MAX CONN NUM %d\n", temp_ap_config.ap.max_connection);
        break;
    }
	case ESP_BLUFI_EVENT_RECV_SOFTAP_AUTH_MODE: {
	    if (param->softap_auth_mode.auth_mode >= WIFI_AUTH_MAX) {
	        return;
	    }
	    {
	        wifi_config_t temp_ap_config = get_ap_config();
	        temp_ap_config.ap.authmode = param->softap_auth_mode.auth_mode;
	        set_ap_config(&temp_ap_config);
	        esp_wifi_set_config(WIFI_IF_AP, &temp_ap_config);
	        printf("Recv SOFTAP AUTH MODE %d\n", temp_ap_config.ap.authmode);
	    }
	    break;
	}
	case ESP_BLUFI_EVENT_RECV_SOFTAP_CHANNEL: {
	    if (param->softap_channel.channel > 13) {
	        return;
	    }
	    {
	        wifi_config_t temp_ap_config = get_ap_config(); // Retrieve the current configuration
	        temp_ap_config.ap.channel = param->softap_channel.channel;
	        set_ap_config(&temp_ap_config); // Update the global configuration
	        esp_wifi_set_config(WIFI_IF_AP, &temp_ap_config); // Apply the modified configuration
	        printf("Recv SOFTAP CHANNEL %d\n", temp_ap_config.ap.channel);
	    }
	    break;
	}
    case ESP_BLUFI_EVENT_GET_WIFI_LIST: {
    	printf("Scan started\n");
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
    }
    case ESP_BLUFI_EVENT_RECV_CUSTOM_DATA: {
        printf("Recv Custom Data %" PRIu32 "\n", param->custom_data.data_len);
        if (!test_in_progress()) {
            xTimerReset(blufi_adv_expiry_timer, 0);
        }
        esp_log_buffer_hex("Custom Data", param->custom_data.data, param->custom_data.data_len);
        analyse_received_data(param->custom_data.data, param->custom_data.data_len);
        break;
    }

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
        break;
	case ESP_BLUFI_EVENT_RECV_SERVER_PRIV_KEY:
        /* Not handle currently */
        break;
    default:
        break;
    }
}

void blufi_get_ble_address(uint8_t* addr) {
    esp_read_mac(addr, ESP_MAC_BT);
}

int blufi_get_ble_connection_state(void) {
    return current_ble_state;
}

int blufi_get_ble_connection_number(void) {
    return ble_connection_count;
}

int blufi_ble_init(void) {
	esp_err_t ret;

	bt_status = esp_bt_controller_get_status();
    if (bt_status == ESP_BT_CONTROLLER_STATUS_INITED || bt_status == ESP_BT_CONTROLLER_STATUS_ENABLED) {
	  blufi_ble_deinit();
    }

	if (!is_bt_mem_released) {
		ret = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
		if (ret != ESP_OK) {
			printf("%s release BT classic memory failed: %s\n", __func__, esp_err_to_name(ret));
			return -1;
		}
		is_bt_mem_released = true;
	}

	esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();

	if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_IDLE) {
		ret = esp_bt_controller_init(&bt_cfg);
		if (ret != ESP_OK) {
			printf("%s initialize controller failed: %s\n", __func__, esp_err_to_name(ret));
			return -1;
		}
	}

	if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_INITED) {
		ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
		if (ret != ESP_OK) {
			printf("%s enable controller failed: %s\n", __func__, esp_err_to_name(ret));
			return -1;
		}
	}

    blufi_adv_expiry_timer = xTimerCreate("blufi_adv_expiry_timer", pdMS_TO_TICKS(BLE_ADV_EXPIRY_TIME * 60 * 1000), pdFALSE, (void *) 0, blufi_adv_expiry_timer_cb);

	return 0;
}

int blufi_ble_deinit(void) {
	esp_err_t ret;

	/* bluetooth de-init */
	ret = esp_blufi_host_deinit();

	if (ret != ESP_OK) {
		printf("BT_INIT", "BLUFI de-initialise failed: %s", esp_err_to_name(ret));
		return -1;
	}
	return 0;
}

