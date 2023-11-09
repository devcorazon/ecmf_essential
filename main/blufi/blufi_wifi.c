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
#include "esp_err.h"

#include "lwip/sockets.h"
#include "lwip/netdb.h"

#include "blufi.h"

#include "structs.h"
#include "messaging.h"
#include "storage.h"

#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"

static bool wifi_scan_on = false;

static esp_netif_t *ap_netif = NULL;
static esp_netif_t *sta_netif = NULL;

int blufi_wifi_connect(void);

static TimerHandle_t tcp_reconnect_timer = NULL;
static int sock = -1; // Global socket descriptor

 /* Wps Config */
esp_wps_config_t wps_config = WPS_CONFIG_INIT_DEFAULT(WPS_MODE);
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
bool gl_sta_connected = false;
bool gl_sta_got_ip = false;
bool ble_is_connected = false;
uint8_t gl_sta_bssid[BSSID_SIZE] = {0};
uint8_t gl_sta_ssid[SSID_SIZE] = {0};
int gl_sta_ssid_len = 0;
wifi_sta_list_t gl_sta_list = {0};   // Use designated initializers to zero out a struct.
bool gl_sta_is_connecting = false;
esp_blufi_extra_info_t gl_sta_conn_info = {0}; // Use designated initializers to zero out a struct.

enum wifi_connection_state {
    WIFI_DISCONNECTED = 0,
    WIFI_CONNECTED
};

void tcp_reconnect_timer_callback(TimerHandle_t xTimer) {
    printf("TCP expiry, retrying connection...\n");
    wifi_connect_to_server_tcp();
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

int blufi_wifi_connect(void) {
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
    char recv_buf[128];
    int len;

    get_server(server_ip);
    get_port(port_str);

    server_ip[SERVER_SIZE] = '\0';
    port_str[PORT_SIZE] = '\0';

    int int_val = atoi((char *)port_str);
    if (int_val < 0 || int_val > UINT16_MAX) {
        // Handle invalid port number
        return -1;
    } else {
        port = (uint16_t)int_val;
    }

    struct sockaddr_in server_addr;
    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        printf("Unable to create socket: errno %d\n", errno);
        return -1;
    }

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; // Use IPv4
    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
    hints.ai_protocol = IPPROTO_TCP; // TCP protocol

    // Validate IP address format or resolve hostname
    if (inet_pton(AF_INET, (const char *)server_ip, &server_addr.sin_addr) != 1) {
        // If the server_ip is not a valid IP address, resolve the hostname
        if (getaddrinfo((const char *)server_ip, NULL, &hints, &res) != 0) {
            printf("DNS resolution failed for %s: errno %d\n", server_ip, errno);
            close(sock);
            sock = -1;
            return -1;
        }
        // Copy the resolved IP address to server_addr
        memcpy(&server_addr.sin_addr, &((struct sockaddr_in *)(res->ai_addr))->sin_addr, sizeof(struct in_addr));
        freeaddrinfo(res); // Free the address info struct
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    printf("Connecting to %s:%d\n", inet_ntoa(server_addr.sin_addr), port);

    // Create the reconnect timer if it hasn't been created yet
    if (tcp_reconnect_timer == NULL) {
        tcp_reconnect_timer = xTimerCreate("ReconnectTimer", TCP_CONN_RECONNECTING_DELAY, pdFALSE, (void *)0, tcp_reconnect_timer_callback);
    }

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0) {
        printf("Socket unable to connect: errno %d\n", errno);
        close(sock);
        sock = -1;
        // Start the reconnect timer to retry after a delay
        xTimerStart(tcp_reconnect_timer, 0);
        return -1;
    }

    printf("Successfully connected to the server\n");
    // Connection established, stop the reconnect timer if it is running
    xTimerStop(tcp_reconnect_timer, 0);

    // Receive data loop
    while (1) {
        len = recv(sock, recv_buf, sizeof(recv_buf) - 1, 0);
        // If error in receiving or connection lost
        if (len <= 0) {
            printf("Server disconnected or recv error: errno %d\n", errno);
            close(sock);
            sock = -1;
            // Restart the reconnect timer to retry after a delay
            xTimerStart(tcp_reconnect_timer, 0);
            return 0;
        }
        // Process received data
        recv_buf[len] = '\0'; // Null-terminate the received data
        analyse_received_data((const uint8_t *)recv_buf, len);
    }
    // If the loop exits
    close(sock);
    sock = -1;
    return 0;
}

int softap_get_current_connection_number(void) {

    if (esp_wifi_ap_get_sta_list(&gl_sta_list) == ESP_OK) {
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
        memcpy(info.sta_bssid, gl_sta_bssid, sizeof(gl_sta_bssid));
        info.sta_bssid_set = true;
        info.sta_ssid = gl_sta_ssid;
        info.sta_ssid_len = gl_sta_ssid_len;
        gl_sta_got_ip = true;
        if (ble_is_connected == true) {
            esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONN_SUCCESS, softap_get_current_connection_number(), &info);
        }
        else {
        	printf("BLUFI BLE is not connected yet\n");
        }

        if (get_wifi_active()) {
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
	    if (get_wifi_active()) {
		    blufi_wifi_connect();
	    }
        break;
    case WIFI_EVENT_STA_CONNECTED:
        gl_sta_connected = true;
        gl_sta_is_connecting = false;
        event = (wifi_event_sta_connected_t*) event_data;
        memcpy(gl_sta_bssid, event->bssid, sizeof(gl_sta_bssid));
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
    	vTaskDelay(pdMS_TO_TICKS(WIFI_RECONNECTING_DELAY));
    	blufi_wifi_connect();
#endif
        gl_sta_connected = false;
        gl_sta_got_ip = false;
        memset(gl_sta_ssid, 0, sizeof(gl_sta_ssid));
        memset(gl_sta_bssid, 0, sizeof(gl_sta_bssid));
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
            memcpy(info.sta_bssid, gl_sta_bssid, sizeof(gl_sta_bssid));
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
            esp_wifi_wps_disable();
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
        esp_wifi_wps_enable(&wps_config);
        esp_wifi_wps_start(0);
        break;
    case WIFI_EVENT_STA_WPS_ER_TIMEOUT:
    	printf("WIFI_EVENT_STA_WPS_ER_TIMEOUT\n");
        esp_wifi_wps_disable();
        esp_wifi_wps_enable(&wps_config);
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
	printf("wifi statring 1");
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
