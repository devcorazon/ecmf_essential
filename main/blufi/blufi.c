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
#include "freertos/timers.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "esp_wifi.h"

#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_blufi_api.h"
#include "esp_blufi.h"
#include "esp_wps.h"
#include "esp_err.h" // for error handling

#include "lwip/sockets.h"
#include "lwip/netdb.h"

#include "blufi.h"
#include "blufi_internal.h"

#include "storage.h"
#include "structs.h"

#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"


static bool is_bt_mem_released = false;
static bool wifi_scan_on = false;

static esp_netif_t *ap_netif = NULL;
static esp_netif_t *sta_netif = NULL;

typedef void (*command_callback_t) (char *pnt_data, size_t length);

struct custom_command_s {
	const char				*command;
	command_callback_t		command_callback;
};

static void ota_callback(char *pnt_data, size_t length);
static void version_callback(char *pnt_data, size_t length);
static void server_callback(char *pnt_data, size_t length);
static void port_callback(char *pnt_data, size_t length);
static void wifi_active_callback(char *pnt_data, size_t length);
static void wifi_wps_callback(char *pnt_data, size_t length);

static const struct custom_command_s custom_commands_table[] = {
	{ 	BLUFI_CMD_OTA,			ota_callback	    	},
	{ 	BLUFI_CMD_VERSION,		version_callback		},
	{ 	BLUFI_CMD_SERVER,		server_callback			},
	{ 	BLUFI_CMD_PORT,			port_callback	    	},
	{	BLUFI_CMD_WIFI_ACTIVE,	wifi_active_callback	},
	{   BLUFI_CMD_WIFI_WPS,     wifi_wps_callback       },
};

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

static void tcp_received_data_handler(const uint8_t *data);

static int blufi_wifi_connect(void);
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

static wifi_config_t sta_config;
static wifi_config_t ap_config;


 /* Wps Config */
static esp_wps_config_t config = WPS_CONFIG_INIT_DEFAULT(WPS_MODE);
static wifi_config_t wps_ap_creds[MAX_WPS_AP_CRED];

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int CONNECTED_BIT = BIT0;
const int FAIL_BIT = BIT1;

static int s_ap_creds_num = 0;

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

enum wifi_connection_state {
    WIFI_DISCONNECTED = 0,
    WIFI_CONNECTED
};

enum ble_connection_state {
    BLE_DISCONNECTED,
    BLE_CONNECTED
};

static enum ble_connection_state current_ble_state = BLE_DISCONNECTED;

static void analyse_received_data(const uint8_t *data, uint32_t data_len) {
    for (size_t i = 0; i < ARRAY_SIZE(custom_commands_table); i++) {
        char *ptr = strstr((char *) data, custom_commands_table[i].command);
        size_t len = data_len;
        if (ptr != NULL) {
            len -= strlen(custom_commands_table[i].command);
            len--;
            ptr += strlen(custom_commands_table[i].command);
            ptr++;
            custom_commands_table[i].command_callback(ptr, len);
            break;
        }
    }
}

static void version_callback(char *pnt_data, size_t length) {
	esp_err_t ret;

//	uint8_t fw_ver[] = { FW_VERSION_MAJOR, FW_VERSION_MINOR, FW_VERSION_PATCH };
	uint8_t fw_ver[] = { FW_VERSION_MAJOR + 0x30, FW_VERSION_MINOR + 0x30, FW_VERSION_PATCH + 0x30 };

	ret = esp_blufi_send_custom_data(fw_ver, sizeof(fw_ver));
	printf("version_callback - %d\n", ret);
}

static void server_callback(char *pnt_data, size_t length) {
	uint8_t server[SERVER_SIZE + 1] = { 0 };

    if (length <= SERVER_SIZE) {
    	memcpy(server, pnt_data, length);
        set_server((const uint8_t *) server);
    }
    else {
        printf("Received server data exceeds the storage limit.\n");
    }
}

static void port_callback(char *pnt_data, size_t length) {
    uint8_t port[PORT_SIZE + 1] = { 0 };
    char *endptr;

    if (length <= PORT_SIZE) {
        memcpy(port, pnt_data, length);
        long port_value = strtol((char *)port, &endptr, 10);

        // Validate conversion and port range
        if (*endptr != '\0' || port_value < 0 || port_value > MAX_PORT_VALUE) {
            printf("Received invalid port number.\n");
        } else {
            set_port((const uint8_t *) port);
        }
    }
    else {
        printf("Received port data exceeds the storage limit.\n");
    }
}

static void ota_callback(char *pnt_data, size_t length) {
    uint8_t ota_url[OTA_URL_SIZE + 1] = { 0 };

    if (length <= OTA_URL_SIZE) {
        memcpy(ota_url, pnt_data, length);
        set_ota_url(ota_url);
        blufi_ota_start();
    }
    else {
        printf("Received port data exceeds the storage limit.\n");
    }
}

static void wifi_active_callback(char *pnt_data, size_t length) {
	uint8_t wifi_active = (uint8_t) (pnt_data[0]);

#warning VALUE_NOT_BINARY
#if 1
	if ((wifi_active != 0x30) && (wifi_active != 0x31)) {
		wifi_active = 0x31;
	}
	wifi_active -= 0x30;
#endif

	if (length == 1) {
		if (get_wifi_active() != wifi_active) {
			if (wifi_active) {
				char ssid[SSID_SIZE + 1] = { 0 };
				char pw[PASSWORD_SIZE + 1] = { 0 };
				char server[SERVER_SIZE + 1] = { 0 };
				char port[PORT_SIZE + 1] = { 0 };

				get_ssid((uint8_t *) ssid);
				get_password((uint8_t *) pw);
				get_server((uint8_t *) server);
				get_port((uint8_t *) port);

				if (!strlen(ssid) || !strlen(pw) || !strlen(server) || !strlen(port)) {
					printf("SSID, PSK, SERVER and PORT missing\n");
					return;
				}
			}

			set_wifi_active(wifi_active);

			if (get_wifi_active()) {
				if (gl_sta_connected == true) {
					wifi_connect_to_server_tcp();
				}
				else {
					blufi_wifi_connect();
				}
			}
			else {
				if (gl_sta_connected == true) {
#warning chiudere sock tcp
					esp_wifi_disconnect();
				}
			}
		}
	}
    else {
        printf("Received wifi_active data exceeds the storage limit.\n");
    }
}

static void wifi_wps_callback(char *pnt_data, size_t length) {
	int ret;

	printf("Im in WPS CALLBACK\n");

	ret = esp_wifi_wps_enable(&config);
	if (ret != ESP_OK) {
		printf("Failed esp_wifi_wps_enable\n");
		return;
	}

	ret = esp_wifi_wps_start(0);
	if (ret != ESP_OK) {
		printf("Failed esp_wifi_wps_start\n");
		return;
	}
}

static int blufi_wifi_configure(uint8_t mode, wifi_config_t *wifi_config) {

    if (mode == WIFI_MODE_AP) {
        uint32_t serial_number = get_serial_number();
        char ssid_prefix[] = "ECMF-";
        char ssid[SSID_SIZE];
        char serial_number_str[9];

        sprintf(serial_number_str, "%08" PRIx32, serial_number);
        snprintf(ssid, sizeof(ssid), "%s%s", ssid_prefix, serial_number_str);

        memcpy(wifi_config->ap.ssid, ssid, sizeof(ssid));
        wifi_config->ap.ssid_len = strlen(ssid);

        // Assuming ESP_WIFI_PASS is defined as your AP password. If it is not defined, you will need to replace it.
        if (strlen(ESP_WIFI_PASS) == 0) {
            wifi_config->ap.authmode = WIFI_AUTH_OPEN;
        }

        if (esp_wifi_set_config(ESP_IF_WIFI_AP, wifi_config) != ESP_OK) {
            printf("Failed to set WiFi Config AP.\n");
            return -1;
        }
    }
    else if (mode == WIFI_MODE_STA) {
    	uint8_t ssid[SSID_SIZE] = {0};
    	uint8_t pw[PASSWORD_SIZE] = {0};

    	get_ssid(ssid);
    	get_password(pw);

    	memcpy(wifi_config->sta.ssid, ssid, sizeof(ssid));
    	memcpy(wifi_config->sta.password, pw, sizeof(pw));

        if (esp_wifi_set_config(ESP_IF_WIFI_STA, wifi_config) != ESP_OK) {
            printf("Failed to set WiFi Config STA.\n");
            return -1;
        }
    }
    else {
        printf("Invalid WiFi mode");
        return ESP_ERR_INVALID_ARG;
    }

    return 0;
}

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

static int blufi_wifi_connect(void) {
    gl_sta_is_connecting = (esp_wifi_connect() == ESP_OK);
    record_wifi_conn_info(INVALID_RSSI, INVALID_REASON);
    return 0;
}

#if 0
static bool blufi_wifi_reconnect(void) {
	printf("BLUFI WiFi starts reconnection\n");
	if (!gl_sta_is_connecting) {
	    gl_sta_is_connecting = (esp_wifi_connect() == ESP_OK);
	    record_wifi_conn_info(INVALID_RSSI, INVALID_REASON);
	}

    return true;
}
#endif

int wifi_connect_to_server_tcp(void) {
	uint8_t server_ip[SERVER_SIZE + 1] = {0};
	uint8_t port_str[PORT_SIZE + 1] = {0};
	uint16_t port = 0;
    char recv_buf[128]; // Buffer to store incoming messages
    int len; // Length of the received message

	get_server(server_ip);
	get_port(port_str);

	// Ensure they're null-terminated after copying
	server_ip[SERVER_SIZE] = '\0';
	port_str[PORT_SIZE] = '\0';

	int int_val = atoi((char *)port_str);

	if (int_val < 0 || int_val > UINT16_MAX) {
	    // handle invalid port number
	} else
	{
	    port = (uint16_t)int_val;
	}

    printf("Connecting to %s:%d\n", server_ip, port);

    struct sockaddr_in server_addr;
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (sock < 0) {
    	printf("Unable to create socket: errno %d\n", errno);
        return -1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, (const char *)server_ip, &server_addr.sin_addr);


    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0) {
    	printf("Socket unable to connect: errno %d\n", errno);
        close(sock);
        return -1;
    }

    printf("Successfully connected to the server");

    // Infinite loop to listen to messages from the server
    while(1) {
        len = recv(sock, recv_buf, sizeof(recv_buf) - 1, 0);
        if (len < 0) {
            printf("Error occurred during receiving\n");
            break; // Exit the loop on error
        }
        else if (len == 0) {
            printf("Server disconnected\n");
            break; // Exit the loop if server closes the connection
        }
        else {
            recv_buf[len] = 0; // Null-terminate the received string

            // Pass the received data to the analysis function
            analyse_received_data((const uint8_t *)recv_buf, len);
        }
    }

    close(sock);
    return 0;
}

static int softap_get_current_connection_number(void) {

    if (esp_wifi_ap_get_sta_list(&gl_sta_list) == ESP_OK)
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

        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
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

        if (get_wifi_active()){
            wifi_connect_to_server_tcp();                         // connect to server TCP
        }
        break;
    }
    default:
        break;
    }
    return ;
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,int32_t event_id, void* event_data) {
    wifi_event_sta_connected_t *event;
    wifi_event_sta_disconnected_t *disconnected_event;
    wifi_mode_t mode;
    int status = 0;

    switch (event_id) {
    case WIFI_EVENT_STA_START:
	    if (get_wifi_active() == 1) {
		    blufi_wifi_connect();
	    }
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
#if 0
    	if (!blufi_wifi_reconnect()) {
            gl_sta_is_connecting = false;
            disconnected_event = (wifi_event_sta_disconnected_t*) event_data;
            record_wifi_conn_info(disconnected_event->rssi, disconnected_event->reason);
        }
#else
    	blufi_wifi_connect();
#endif
        gl_sta_connected = false;
        gl_sta_got_ip = false;
        memset(gl_sta_ssid, 0, 32);
        memset(gl_sta_bssid, 0, 6);
        gl_sta_ssid_len = 0;
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
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
        wifi_scan_on = false;
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
    case WIFI_EVENT_STA_WPS_ER_SUCCESS:
#if 0
        printf("WIFI_EVENT_STA_WPS_ER_SUCCESS\n");
        {
            wifi_event_sta_wps_er_success_t *evt =
                (wifi_event_sta_wps_er_success_t *)event_data;
            int i;

            if (evt) {
                s_ap_creds_num = evt->ap_cred_cnt;
                for (i = 0; i < s_ap_creds_num; i++) {
                    memcpy(wps_ap_creds[i].sta.ssid, evt->ap_cred[i].ssid,
                           sizeof(evt->ap_cred[i].ssid));
                    memcpy(wps_ap_creds[i].sta.password, evt->ap_cred[i].passphrase,
                           sizeof(evt->ap_cred[i].passphrase));
                }
                /* If multiple AP credentials are received from WPS, connect with first one */
                printf("Connecting to SSID: %s, Passphrase: %s\n",
                         wps_ap_creds[0].sta.ssid, wps_ap_creds[0].sta.password);
                esp_wifi_set_config(WIFI_IF_STA, &wps_ap_creds[0]);

//                set_ssid(wps_ap_creds[0].sta.ssid);
//                set_password(wps_ap_creds[0].sta.password);
            }
            /*
             * If only one AP credential is received from WPS, there will be no event data and
             * esp_wifi_set_config() is already called by WPS modules for backward compatibility
             * with legacy apps. So directly attempt connection here.
             */
            ESP_ERROR_CHECK(esp_wifi_wps_disable());
            esp_wifi_connect();
        }
#else
        {
			wifi_event_sta_wps_er_success_t *evt = (wifi_event_sta_wps_er_success_t *)event_data;
			uint8_t ssid[SSID_SIZE + 1] = { 0 };
			uint8_t psw[PASSWORD_SIZE + 1] = { 0 };

			printf("WIFI_EVENT_STA_WPS_ER_SUCCESS\n");

			if (evt) {
				wifi_config_t wifi_config = { 0 };

				printf("Connecting to SSID: %s, Passphrase: %s\n", evt->ap_cred[0].ssid, evt->ap_cred[0].passphrase);

				memcpy(ssid, evt->ap_cred[0].ssid, sizeof(evt->ap_cred[0].ssid));
				memcpy(psw, evt->ap_cred[0].passphrase, sizeof(evt->ap_cred[0].passphrase));

				set_ssid((const uint8_t *) ssid);
				set_password((const uint8_t *) psw);

				blufi_wifi_configure(WIFI_MODE_STA, &wifi_config);
			}

			if (gl_sta_connected) {
				esp_wifi_disconnect();
			}

			esp_wifi_wps_disable();
			blufi_wifi_connect();
        }

#endif
        break;
    case WIFI_EVENT_STA_WPS_ER_FAILED:
    	printf("WIFI_EVENT_STA_WPS_ER_FAILED\n");
        esp_wifi_wps_disable();
        esp_wifi_wps_enable(&config);
        esp_wifi_wps_start(0);
        break;
    case WIFI_EVENT_STA_WPS_ER_TIMEOUT:
    	printf("WIFI_EVENT_STA_WPS_ER_TIMEOUT\n");
        esp_wifi_wps_disable();
        esp_wifi_wps_enable(&config);
        esp_wifi_wps_start(0);
        break;
    case WIFI_EVENT_STA_WPS_ER_PIN:
    	 printf("WIFI_EVENT_STA_WPS_ER_PIN\n");
        /* display the PIN code */
        wifi_event_sta_wps_er_pin_t* event = (wifi_event_sta_wps_er_pin_t*) event_data;
  //      printf("WPS_PIN = \n" PINSTR, PIN2STR(event->pin_code));
        break;
    default:
        break;
    }

    if (status == -1) {
        printf("Error occurred in wifi_event_handler for event_id: %d.\n", event_id);
    }
}

/* Event handler for catching system events */
static void ota_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == ESP_HTTPS_OTA_EVENT) {
        switch (event_id) {
            case ESP_HTTPS_OTA_START:
                printf("OTA started");
                break;
            case ESP_HTTPS_OTA_CONNECTED:
            	printf("Connected to server");
                break;
            case ESP_HTTPS_OTA_GET_IMG_DESC:
            	printf("Reading Image Description");
                break;
            case ESP_HTTPS_OTA_VERIFY_CHIP_ID:
            	printf("Verifying chip id of new image: %d", *(esp_chip_id_t *)event_data);
                break;
            case ESP_HTTPS_OTA_DECRYPT_CB:
            	printf("Callback to decrypt function");
                break;
            case ESP_HTTPS_OTA_WRITE_FLASH:
            	printf("Writing to flash: %d written", *(int *)event_data);
                break;
            case ESP_HTTPS_OTA_UPDATE_BOOT_PARTITION:
            	printf("Boot partition updated. Next Partition: %d", *(esp_partition_subtype_t *)event_data);
                break;
            case ESP_HTTPS_OTA_FINISH:
                printf("OTA finish");
                break;
            case ESP_HTTPS_OTA_ABORT:
            	printf("OTA abort");
            	blufi_ota_start();
                break;
        }
    }
}

int blufi_ap_start(void) {
	esp_err_t ret;
	wifi_config_t wifi_config = { 0 };

	ret = esp_wifi_set_mode(WIFI_MODE_AP);
	if (ret != ESP_OK) {
		printf("Failed to set WiFi mode: %s\n", esp_err_to_name(ret));
		return -1;
	}

	ret = blufi_wifi_configure(WIFI_MODE_AP, &wifi_config);
	if (ret != ESP_OK) {
		return -1;
	}

	ret = esp_wifi_start();
	if (ret != ESP_OK) {
		printf("Failed to start WiFi: %s\n", esp_err_to_name(ret));
	}

	return 0;
}

int blufi_ap_stop(void) {
    esp_err_t ret;

    printf("Stopping WiFi access point...");

    ret = esp_wifi_stop();
    if (ret != ESP_OK) {
        printf("Failed to stop WiFi: %s\n", esp_err_to_name(ret));
        return -1;
    }
    return 0;
}

static void blufi_adv_expiry_timer_cb(TimerHandle_t xTimer) {
    blufi_ap_stop();
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

    // Create the timer if not already done
    if (!blufi_adv_expiry_timer) {
    	blufi_adv_expiry_timer = xTimerCreate("blufi_adv_expiry_timer", pdMS_TO_TICKS(BLE_ADV_EXPIRY_TIME * 60 * 1000), pdFALSE, (void *) 0, blufi_adv_expiry_timer_cb);
    }

    // Start the timer
    if (blufi_adv_expiry_timer) {
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
    /* actually, should post to blufi_task handle the procedure,
     * now, as a example, we do it more simply */
    switch (event) {
    case ESP_BLUFI_EVENT_INIT_FINISH:
    	printf("BLUFI init finish\n");
   	    esp_ble_gap_config_adv_data(&blufi_adv_data);
        break;
    case ESP_BLUFI_EVENT_DEINIT_FINISH:
    	printf("BLUFI deinit finish\n");
        break;
    case ESP_BLUFI_EVENT_BLE_CONNECT:
    	printf("BLUFI ble connect\n");
        ble_is_connected = true;
        esp_blufi_adv_stop();
        blufi_security_init();
        current_ble_state = BLE_CONNECTED;
        ble_connection_count++;
        break;
    case ESP_BLUFI_EVENT_BLE_DISCONNECT:
        printf("BLUFI ble disconnect\n");
        ble_is_connected = false;
        blufi_security_deinit();
   	    esp_ble_gap_config_adv_data(&blufi_adv_data);
   	    current_ble_state = BLE_DISCONNECTED;
   	    ble_connection_count--;
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
		if (gl_sta_connected) {
			esp_wifi_disconnect();
		}
		blufi_wifi_connect();

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
        set_ssid(sta_config.sta.ssid);
        break;
	case ESP_BLUFI_EVENT_RECV_STA_PASSWD:
        strncpy((char *)sta_config.sta.password, (char *)param->sta_passwd.passwd, param->sta_passwd.passwd_len);
        sta_config.sta.password[param->sta_passwd.passwd_len] = '\0';
        esp_wifi_set_config(WIFI_IF_STA, &sta_config);
        printf("Recv STA PASSWORD %s\n", sta_config.sta.password);
        set_password(sta_config.sta.password);
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

    case ESP_BLUFI_EVENT_RECV_CUSTOM_DATA:
        printf("Recv Custom Data %" PRIu32 "\n", param->custom_data.data_len);
        esp_log_buffer_hex("Custom Data", param->custom_data.data, param->custom_data.data_len);
        analyse_received_data(param->custom_data.data, param->custom_data.data_len);
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


void blufi_get_ble_address(uint8_t* addr) {
    esp_read_mac(addr, ESP_MAC_BT);
}

int blufi_get_ble_connection_state(void) {
    return current_ble_state;
}

int blufi_get_ble_connection_number(void) {
    return ble_connection_count;
}

void blufi_get_wifi_address(uint8_t* addr) {
    esp_wifi_get_mac(WIFI_IF_STA, addr);
}

int blufi_get_wifi_connection_state(void) {
    wifi_ap_record_t wifidata;
    if (esp_wifi_sta_get_ap_info(&wifidata) == ESP_OK) {
        return WIFI_CONNECTED;
    }
    else {
        return WIFI_DISCONNECTED;
    }
}

int blufi_get_wifi_active(void) {
    wifi_mode_t mode;
    if (esp_wifi_get_mode(&mode) == ESP_OK) {
        return mode == WIFI_MODE_STA || mode == WIFI_MODE_AP || mode == WIFI_MODE_APSTA;
    }
    return 0;
}

int blufi_wifi_init(void) {
	esp_err_t ret;

	/* Initializing Wifi */
	esp_netif_init();

	ret = esp_event_loop_create_default();
	if (ret != ESP_OK) {
		printf("Failed to create default event loop: %s\n", esp_err_to_name(ret));
		return -1;
	}

	esp_netif_create_default_wifi_sta();

	esp_netif_create_default_wifi_ap();

	ret = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);
	if (ret != ESP_OK) {
		printf("Failed to register WiFi event handler: %s\n", esp_err_to_name(ret));
		return -1;
	}

	ret = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_handler, NULL);
	if (ret != ESP_OK) {
		printf("Failed to register IP event handler: %s\n", esp_err_to_name(ret));
		return -1;
	}

	ret = esp_event_handler_register(ESP_HTTPS_OTA_EVENT, ESP_EVENT_ANY_ID, &ota_event_handler, NULL);
	if (ret != ESP_OK) {
		printf("Failed to register OTA event handler: %s\n", esp_err_to_name(ret));
		return -1;
	}

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ret = esp_wifi_init(&cfg);
	if (ret != ESP_OK) {
		printf("Failed to initialize WiFi: %s\n", esp_err_to_name(ret));
		return -1;
	}

	wifi_config_t wifi_config = { 0 };
	wifi_event_group = xEventGroupCreate();

	ret = esp_wifi_set_mode(WIFI_MODE_STA);
	if (ret != ESP_OK) {
		printf("Failed to set WiFi mode: %s\n", esp_err_to_name(ret));
		return -1;
	}

	ret = blufi_wifi_configure(WIFI_MODE_STA, &wifi_config);
	if (ret != ESP_OK) {
		printf("Failed blufi_wifi_configure\n");
		return -1;
	}

	ret = esp_wifi_start();
	if (ret != ESP_OK) {
		printf("Failed esp_wifi_start\n");
		return -1;
	}

	return 0;
}

int ble_wifi_deinit(void) {
	esp_err_t ret;

	/* wifi de-init */
    ret = esp_wifi_deinit();
    if (ret != ESP_OK) {
        printf("Failed to deinitialize WiFi: %s\n", esp_err_to_name(ret));
        return -1;
    }

    ret = esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler);
    if (ret != ESP_OK) {
        printf("Failed to unregister WiFi event handler: %s\n", esp_err_to_name(ret));
        return -1;
    }

    ret = esp_event_handler_instance_unregister(IP_EVENT, ESP_EVENT_ANY_ID, &ip_event_handler);
    if (ret != ESP_OK) {
        printf("Failed to unregister IP event handler: %s\n", esp_err_to_name(ret));
        return -1;
    }

    ret = esp_event_handler_instance_unregister(ESP_HTTPS_OTA_EVENT, ESP_EVENT_ANY_ID, &ota_event_handler);
    if (ret != ESP_OK) {
        printf("Failed to unregister OTA event handler: %s\n", esp_err_to_name(ret));
        return -1;
    }

    ret = esp_event_loop_delete_default();
    if (ret != ESP_OK) {
        printf("Failed to delete default event loop: %s\n", esp_err_to_name(ret));
        return -1;
    }

    if (ap_netif) {
        esp_netif_destroy(ap_netif);
        ap_netif = NULL;
    }
    if (sta_netif) {
        esp_netif_destroy(sta_netif);
        sta_netif = NULL;
    }

	return 0;
}

int blufi_ble_init(void) {
	esp_err_t ret;

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

