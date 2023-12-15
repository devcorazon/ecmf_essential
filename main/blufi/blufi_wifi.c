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
#include "freertos/ringbuf.h"

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

#include "esp_task_wdt.h"
#include <sys/select.h>

#include "blufi.h"

#include "structs.h"
#include "protocol.h"
#include "messaging.h"
#include "storage.h"
#include "test.h"

#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"

#define	TCP_RECEIVE_TASK_STACK_SIZE			        (configMINIMAL_STACK_SIZE * 4)
#define	TCP_RECEIVE_TASK_PRIORITY			        (1)
#define	TCP_RECEIVE_TASK_PERIOD				        (100ul / portTICK_PERIOD_MS)

static bool wifi_scan_on = false;

static esp_netif_t *ap_netif = NULL;
static esp_netif_t *sta_netif = NULL;

int blufi_wifi_connect(void);

static TimerHandle_t tcp_reconnect_timer = NULL;
static TimerHandle_t wifi_reconnect_timer = NULL;
static TimerHandle_t voluntary_periodic_timer  = NULL;
static TimerHandle_t reset_rx_trame_timer = NULL;

static int sock = -1; // Global socket descriptor

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int CONNECTED_BIT = BIT0;
const int FAIL_BIT = BIT1;

/* store the station info for send back to phone */
static bool gl_sta_connected = false;
static bool gl_sta_got_ip = false;
static bool gl_tcp_connected = false;
static bool ble_is_connected = false;
static uint8_t gl_sta_bssid[BSSID_SIZE] = {0};
static uint8_t gl_sta_ssid[SSID_SIZE] = {0};
static int gl_sta_ssid_len = 0;
static wifi_sta_list_t gl_sta_list = {0};
static bool gl_sta_is_connecting = false;
static esp_blufi_extra_info_t gl_sta_conn_info = {0};
static wifi_config_t gl_sta_config = {0};
static wifi_config_t gl_ap_config = {0};
static bool gl_wps_is_enabled = false;
static esp_wps_config_t gl_wps_config = WPS_CONFIG_INIT_DEFAULT(WPS_MODE);

RingbufHandle_t xRingBuffer;
static uint8_t out_data[RING_BUFFER_SIZE];
static size_t out_data_size = 0;

uint8_t *temp_buffer = NULL;
size_t temp_buffer_size = 0;

enum wifi_connection_state {
    WIFI_DISCONNECTED = 0,
    WIFI_CONNECTED
};

void tcp_reconnect_timer_callback(TimerHandle_t xTimer) {
    if (get_wifi_active()) {
    	tcp_connect_to_server();                         // connect to server TCP
    }
}

void wifi_reconnect_timer_callback(TimerHandle_t xTimer) {
    if (get_wifi_active()) {
	    blufi_wifi_connect();                           // connect to WIFI
    }
}

void voluntary_periodic_timer_callback(TimerHandle_t xTimer) {
	if ( get_wifi_period() ) {
		printf("Sending Voluntary Trame\n");
		blufi_wifi_send_voluntary(PROTOCOL_FUNCT_VOLUNTARY, PROTOCOL_OBJID_STATE, 0);   // Send voluntary message
		xTimerChangePeriod(voluntary_periodic_timer,pdMS_TO_TICKS(SECONDS_TO_MS(get_wifi_period())),0);
		xTimerStart(voluntary_periodic_timer,0);
	}
}

void reset_rx_trame_timer_callback(TimerHandle_t xTimer) {
     printf("Reseting TCP Receive trame.\n");          // TCP Rx trame callback
     size_t itemSize;
     void* item = NULL;
     item = xRingbufferReceive(xRingBuffer, &itemSize, 0);
     if ( item != NULL) {
         vRingbufferReturnItem(xRingBuffer,item);
     }
}

bool get_sta_connected(void) {
    return gl_sta_connected;
}

void set_sta_connected(bool value) {
    gl_sta_connected = value;
}

bool get_sta_got_ip(void) {
    return gl_sta_got_ip;
}

void set_sta_got_ip(bool value) {
    gl_sta_got_ip = value;
}

bool get_tcp_connected(void) {
    return gl_tcp_connected;
}

void set_tcp_connected(bool value) {
    gl_tcp_connected = value;
}

bool get_ble_is_connected(void) {
    return ble_is_connected;
}

void set_ble_is_connected(bool value) {
    ble_is_connected = value;
}

uint8_t* get_sta_bssid(void) {
    return gl_sta_bssid;
}

void set_sta_bssid(const uint8_t* bssid) {
    if (bssid != NULL) {
        memcpy(gl_sta_bssid, bssid, BSSID_SIZE);
    }
}

uint8_t* get_sta_ssid(void) {
    return gl_sta_ssid;
}

void set_sta_ssid(const uint8_t* ssid,uint8_t ssid_length) {
    if (ssid != NULL) {
        memcpy(gl_sta_ssid, ssid, ssid_length);
    }
}

int get_sta_ssid_len(void) {
    return gl_sta_ssid_len;
}

void set_sta_ssid_len(int len) {
    gl_sta_ssid_len = len;
}

wifi_sta_list_t get_sta_list(void) {
    return gl_sta_list;
}

void set_sta_list(const wifi_sta_list_t* list) {
    if (list != NULL) {
        gl_sta_list = *list;
    }
}

bool get_sta_is_connecting(void) {
    return gl_sta_is_connecting;
}

void set_sta_is_connecting(bool value) {
    gl_sta_is_connecting = value;
}

esp_blufi_extra_info_t* get_sta_conn_info(void) {
    return &gl_sta_conn_info;
}

void set_sta_conn_info(const esp_blufi_extra_info_t* info) {
    if (info != NULL) {
        gl_sta_conn_info = *info;
    }
}

wifi_config_t get_sta_config(void) {
    return gl_sta_config;
}

void set_sta_config(const wifi_config_t* config) {
    if (config != NULL) {
        gl_sta_config = *config;
    }
}

wifi_config_t get_ap_config(void) {
    return gl_ap_config;
}

void set_ap_config(const wifi_config_t* config) {
    if (config != NULL) {
        gl_ap_config = *config;
    }
}

bool get_wps_is_enabled(void) {
    return gl_wps_is_enabled;
}

void set_wps_is_enabled(bool value) {
	gl_wps_is_enabled = value;
}

esp_wps_config_t get_wps_config(void) {
    return gl_wps_config;
}

void set_wps_config(const esp_wps_config_t* config) {
    if (config != NULL) {
    	gl_wps_config = *config;
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

        // Setting Open authorization
        wifi_config->ap.authmode = WIFI_AUTH_OPEN;

        // Setting max connections
        wifi_config->ap.max_connection = 1;

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
    esp_blufi_extra_info_t conn_info;
    memset(&conn_info, 0, sizeof(esp_blufi_extra_info_t));

    if (get_sta_is_connecting()) {
    	conn_info.sta_max_conn_retry_set = true;
    	conn_info.sta_max_conn_retry = WIFI_CONNECTION_MAXIMUM_RETRY;
    } else {
    	conn_info.sta_conn_rssi_set = true;
    	conn_info.sta_conn_rssi = rssi;
        conn_info.sta_conn_end_reason_set = true;
        conn_info.sta_conn_end_reason = reason;
    }

    set_sta_conn_info(&conn_info);
}

int blufi_wifi_connect(void) {
    set_sta_is_connecting(esp_wifi_connect() == ESP_OK);
    record_wifi_conn_info(INVALID_RSSI, INVALID_REASON);
    return 0;
}

int tcp_close_reconnect(void) {
    close(sock);
    sock = -1;
    set_tcp_connected(false);
    xTimerStop(voluntary_periodic_timer,0);
    // Start the reconnect timer to retry after a delay
    xTimerStart(tcp_reconnect_timer, 0);

    return 0;
}

static int tcp_enable_keepalive(int sock) {
	int enable = 1;
    int idle = TCP_KEEPALIVE_IDLE_TIME;
    int interval = TCP_KEEPALIVE_INTVAL;
    int count = TCP_KEEPALIVE_COUNT;

    // Enable TCP keepalive
    if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &enable, sizeof(enable)) < 0) {
        printf("Failed to set SO_KEEPALIVE on TCP socket\n");
        return -1;
    }

    // Set the keepalive idle time
    if (setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(idle)) < 0) {
        printf("Failed to set TCP_KEEPIDLE on TCP socket\n");
        return -1;
    }

    // Set the keepalive interval
    if (setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval)) < 0) {
        printf("Failed to set TCP_KEEPINTVL on TCP socket\n");
        return -1;
    }

    // Set the keepalive count
    if (setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, &count, sizeof(count)) < 0) {
        printf("Failed to set TCP_KEEPCNT on TCP socket\n");
        return -1;
    }

    return 0;
}

void tcp_receive_data_task(void *pvParameters) {
    uint8_t recv_buf[RING_BUFFER_SIZE];
    int len;
    TickType_t tcp_receive_task_time;

    tcp_receive_task_time = xTaskGetTickCount();
    xRingBuffer = xRingbufferCreate(RING_BUFFER_SIZE, RINGBUF_TYPE_BYTEBUF);

    while (1) {
        memset(recv_buf, 0, RING_BUFFER_SIZE);

        if ((xEventGroupGetBits(wifi_event_group) & CONNECTED_BIT) != CONNECTED_BIT) {
            printf("Wi-Fi connection lost, terminating task\n");
            break;
        }

        len = recv(sock, recv_buf, RING_BUFFER_SIZE, 0);
        if (len < 0) {
            printf("Server disconnected or recv error: errno %d\n", errno);
            break;
        }

        if (len > 0) {
        	xTimerStart(reset_rx_trame_timer,0);
            xRingbufferSend(xRingBuffer, recv_buf, len, portMAX_DELAY);
        }

        while (1) {
            size_t in_data_size;
            uint8_t *in_data = (uint8_t *)xRingbufferReceiveUpTo(xRingBuffer, &in_data_size, 0, len);
            if (!in_data) {
                break; // no data received
            }

            temp_buffer = realloc(temp_buffer, temp_buffer_size + in_data_size);
            memcpy(temp_buffer + temp_buffer_size, in_data, in_data_size);
            temp_buffer_size += in_data_size;
            vRingbufferReturnItem(xRingBuffer, (void *)in_data);

            while (temp_buffer_size > 0) {
                int processed = proto_elaborate_data(temp_buffer, temp_buffer_size, out_data, &out_data_size);

                if (processed < 0) {
                    // wrong trame
                    temp_buffer_size = 0;
                    break;
                } else if (processed == 0) {
                    // incomplete trame
                    break;
                }

                if (out_data_size) {
                    tcp_send_data(out_data, out_data_size);
                }

                memmove(temp_buffer, temp_buffer + processed, temp_buffer_size - processed);
                temp_buffer_size -= processed;
            }
        }
        if (get_wifi_period()) {
        	if (!xTimerIsTimerActive(voluntary_periodic_timer)) {
        		xTimerChangePeriod(voluntary_periodic_timer,pdMS_TO_TICKS(SECONDS_TO_MS(get_wifi_period())),0);
        		xTimerStart(voluntary_periodic_timer,0);
        	}
        }
        vTaskDelayUntil(&tcp_receive_task_time, TCP_RECEIVE_TASK_PERIOD);
    }

    if (temp_buffer) {
        free(temp_buffer);
    }
    vRingbufferDelete(xRingBuffer);
    tcp_close_reconnect();
    vTaskDelete(NULL);
}

int tcp_connect_to_server(void) {
    uint8_t server_ip[SERVER_SIZE + 1] = {0};
    uint8_t port_str[PORT_SIZE + 1] = {0};
    uint16_t port = 0;

    if ((xEventGroupGetBits(wifi_event_group) & CONNECTED_BIT) != CONNECTED_BIT) {
    	return -1;
    }

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
        xTimerStart(tcp_reconnect_timer, 0);
        return -1;
    }

    // Enable TCP keepalive options on the socket
    if (tcp_enable_keepalive(sock) < 0) {
    	tcp_close_reconnect();
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
            tcp_close_reconnect();
            return -1;
        }
        // Copy the resolved IP address to server_addr
        memcpy(&server_addr.sin_addr, &((struct sockaddr_in *)(res->ai_addr))->sin_addr, sizeof(struct in_addr));
        freeaddrinfo(res); // Free the address info struct
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    printf("Connecting to %s:%d\n", inet_ntoa(server_addr.sin_addr), port);

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0) {
        printf("Socket unable to connect: errno %d\n", errno);
        tcp_close_reconnect();
        return -1;
    }

    printf("Successfully connected to the server\n");
    set_tcp_connected(true);
    if (get_wifi_period()) {
    	xTimerChangePeriod(voluntary_periodic_timer,pdMS_TO_TICKS(SECONDS_TO_MS(get_wifi_period())),0);
    	xTimerStart(voluntary_periodic_timer,0);
    }
    // Start the TCP receive data task
    BaseType_t task_created = xTaskCreate(tcp_receive_data_task, "TCPReceiveTask", TCP_RECEIVE_TASK_STACK_SIZE, NULL, TCP_RECEIVE_TASK_PRIORITY, NULL);
    proto_prepare_identification(out_data, &out_data_size);
    if (out_data_size) {
    	tcp_send_data(out_data, out_data_size);
    }
    return task_created == pdPASS ? 0 : -1;
}

int tcp_send_data(const uint8_t *data, size_t len) {
    size_t lenght = len;
	size_t offset = 0;
	int transmit = 0;

    if (sock < 0) {
        printf("Invalid socket\n");
        return -1;
    }
    // Uncomment for debugging
    printf("Sending data (length = %zu):\n", len);
//    for (size_t i = 0; i < len; ++i) {
//        printf("%02x ", data[i]);
//        if ((i + 1) % 16 == 0) printf("\n");
//    }
//    printf("\n");

	while ((lenght) && ((transmit = send(sock, data + offset, lenght, 0)) > 0)) {
		offset += transmit;
		lenght -= transmit;
	}
    if (transmit < 0) {
        printf("Failed to send data: errno %d\n", errno);
        return -1;
    }

    return offset;
}

int blufi_wifi_send_voluntary(uint8_t funct, uint16_t obj_id, uint16_t index) {
	if ( get_tcp_connected() ) {
		proto_prepare_answer_voluntary(funct, obj_id, index , out_data, &out_data_size);

    	if (out_data_size) {
    		tcp_send_data(out_data, out_data_size);
    	}
	}
    return 0;
}

int softap_get_current_connection_number(void) {
    wifi_sta_list_t sta_list;

    if (esp_wifi_ap_get_sta_list(&sta_list) == ESP_OK) {
        set_sta_list(&sta_list);
        return get_sta_list().num;
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
        memcpy(info.sta_bssid, get_sta_bssid(), sizeof(gl_sta_bssid));
        info.sta_bssid_set = true;
        info.sta_ssid = get_sta_ssid();
        info.sta_ssid_len = get_sta_ssid_len();
        set_sta_got_ip(true);

        if (get_ble_is_connected()) {
            esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONN_SUCCESS, softap_get_current_connection_number(), &info);
        }
        else {
        	printf("BLUFI BLE is not connected yet\n");
        }

        if (get_wifi_active()) {
        	tcp_connect_to_server();                         // connect to server TCP
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
    wifi_mode_t mode;
    int status = 0;

    switch (event_id) {
    case WIFI_EVENT_STA_START:
    	if (get_wifi_active()) {
    		xTimerStart(wifi_reconnect_timer, 0);
    	}
        break;
    case WIFI_EVENT_STA_CONNECTED:
    	set_sta_connected(true);
    	set_sta_is_connecting(false);
    	event = (wifi_event_sta_connected_t*) event_data;
    	set_sta_bssid(event->bssid);
    	set_sta_ssid(event->ssid,event->ssid_len);
    	set_sta_ssid_len(event->ssid_len);
    	break;
    case WIFI_EVENT_STA_DISCONNECTED:
    	printf("WIFI_EVENT_STA_DISCONNECTED...\n");
    	set_sta_connected(false);
    	set_sta_got_ip(false);
    	memset(get_sta_ssid(), 0, SSID_SIZE);
    	memset(get_sta_bssid(), 0, BSSID_SIZE);
    	set_sta_ssid_len(0);
    	xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
    	if (test_in_progress()) {
    		xTimerStop(wifi_reconnect_timer, 0);
    	}
    	else {
    		xTimerStart(wifi_reconnect_timer, 0);
    	}
    	break;
    case WIFI_EVENT_AP_START:
        if (esp_wifi_get_mode(&mode) != ESP_OK) {
            status = -1;
            break;
        }
        if (get_ble_is_connected()) {
            esp_blufi_extra_info_t info;
            memset(&info, 0, sizeof(esp_blufi_extra_info_t));
            memcpy(info.sta_bssid, get_sta_bssid(), BSSID_SIZE);
            info.sta_bssid_set = true;
            info.sta_ssid = get_sta_ssid();
            info.sta_ssid_len = get_sta_ssid_len();
            esp_blufi_send_wifi_conn_report(mode, get_sta_got_ip() ? ESP_BLUFI_STA_CONN_SUCCESS : ESP_BLUFI_STA_NO_IP, softap_get_current_connection_number(), &info);
        } else {
            printf("BLUFI BLE is not connected yet\n");
        }
        break;
    case WIFI_EVENT_SCAN_DONE:
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
        if (get_ble_is_connected()) {
            esp_blufi_send_wifi_list(apCount, blufi_ap_list);
        } else {
            printf("BLUFI BLE is not connected yet\n");
        }
        esp_wifi_scan_stop();
        free(ap_list);
        free(blufi_ap_list);
        break;

    case WIFI_EVENT_STA_WPS_ER_SUCCESS:
        printf("WIFI_EVENT_STA_WPS_ER_SUCCESS\n");

        wifi_event_sta_wps_er_success_t *evt = (wifi_event_sta_wps_er_success_t *)event_data;
		uint8_t ssid[SSID_SIZE + 1] = { 0 };
		uint8_t pwd[PASSWORD_SIZE + 1] = { 0 };

		get_ssid(ssid);

		if ((memcmp(ssid, evt->ap_cred[0].ssid, SSID_SIZE) == 0) && (strlen((char *)ssid))) {

			memcpy(pwd, evt->ap_cred[0].passphrase, sizeof(evt->ap_cred[0].passphrase));

			set_password((const uint8_t *) pwd);

			wifi_config_t sta_config = get_sta_config();
			blufi_wifi_configure(WIFI_MODE_STA, &sta_config);

			printf("Connecting to SSID: %s, Passphrase: %s\n", evt->ap_cred[0].ssid, evt->ap_cred[0].passphrase);

			if (gl_sta_connected) {
				esp_wifi_disconnect();
			}

			esp_wifi_wps_disable();
			set_wps_is_enabled(false);
			blufi_wifi_connect();
		}
        break;
    case WIFI_EVENT_STA_WPS_ER_FAILED:
    	printf("WIFI_EVENT_STA_WPS_ER_FAILED\n");
        esp_wifi_wps_disable();
        set_wps_is_enabled(false);
        break;
    case WIFI_EVENT_STA_WPS_ER_TIMEOUT:
    	printf("WIFI_EVENT_STA_WPS_ER_TIMEOUT\n");
        esp_wifi_wps_disable();
        set_wps_is_enabled(false);
        break;
    case WIFI_EVENT_STA_WPS_ER_PIN:
    	 printf("WIFI_EVENT_STA_WPS_ER_PIN\n");
        /* display the PIN code */
        wifi_event_sta_wps_er_pin_t* event = (wifi_event_sta_wps_er_pin_t*) event_data;
        printf("WPS_PIN = \n%08d", event->pin_code);
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
	esp_wifi_set_mode(WIFI_MODE_AP);

	wifi_config_t t_ap_config = get_ap_config();
    blufi_wifi_configure(WIFI_MODE_AP, &t_ap_config);

    esp_wifi_start();

	return 0;
}

int blufi_ap_stop(void) {
    printf("Stopping WiFi access point...");

    esp_wifi_set_mode(WIFI_MODE_STA);

    wifi_config_t sta_config = get_sta_config();
    blufi_wifi_configure(WIFI_MODE_STA, &sta_config);

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

    if (!sta_netif) { // Check if the STA netif already exists
        sta_netif = esp_netif_create_default_wifi_sta();
    }

    if (!ap_netif) { // Check if the AP netif already exists
        ap_netif = esp_netif_create_default_wifi_ap();
    }

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

	wifi_event_group = xEventGroupCreate();

	ret = esp_wifi_set_mode(WIFI_MODE_STA);
	if (ret != ESP_OK) {
		printf("Failed to set WiFi mode: %s\n", esp_err_to_name(ret));
		return -1;
	}

	wifi_config_t sta_config = get_sta_config();
	ret = blufi_wifi_configure(WIFI_MODE_STA, &sta_config);
	if (ret != ESP_OK) {
	    printf("Failed blufi_wifi_configure\n");
	    return -1;
	}

    // Create the wifi reconnect timer if it hasn't been created yet
    wifi_reconnect_timer = xTimerCreate("Wifi Reconnect Timer", pdMS_TO_TICKS(WIFI_RECONNECTING_DELAY), pdFALSE, (void *)0, wifi_reconnect_timer_callback);

    // Create the reconnect timer if it hasn't been created yet
    tcp_reconnect_timer = xTimerCreate("TCP Reconnect Timer", pdMS_TO_TICKS(TCP_RECONNECTING_DELAY), pdFALSE, (void *)0, tcp_reconnect_timer_callback);

    // Create the voluntary periodic
    voluntary_periodic_timer = xTimerCreate("Voluntary periodic Timer", pdMS_TO_TICKS(SECONDS_TO_MS(30)), pdFALSE, (void *)0, voluntary_periodic_timer_callback);

    // Create the TCP receive reset timer
    reset_rx_trame_timer = xTimerCreate("TCP Reset Rx Trame Timer", pdMS_TO_TICKS(TCP_TRAME_RX_TIMEOUT), pdFALSE, (void *)0, reset_rx_trame_timer_callback);

	ret = esp_wifi_start();
	if (ret != ESP_OK) {
		printf("Failed esp_wifi_start\n");
		return -1;
	}

	return 0;
}

int blufi_wifi_deinit(void) {
	esp_err_t ret;

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

    if (wifi_event_group) {
        vEventGroupDelete(wifi_event_group);
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
