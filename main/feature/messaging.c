/*
 * messaging.c
 *
 *  Created on: 4 nov. 2023
 *      Author: youcef.benakmoume
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_wifi.h"
#include "esp_wps.h"

#include "blufi.h"
#include "messaging.h"
#include "storage.h"

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

int analyse_received_data(const uint8_t *data, uint32_t data_len) {
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
    return 0;
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
					tcp_connect_to_server();
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

	ret = esp_wifi_wps_enable(&wps_config);
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
