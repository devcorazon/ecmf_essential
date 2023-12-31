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

#include "esp_efuse.h"
#include "esp_wifi.h"
#include "esp_wps.h"

#include "blufi.h"
#include "messaging.h"
#include "storage.h"
#include "statistic.h"

extern struct blufi_security *blufi_sec;

typedef void (*command_callback_t) (char *pnt_data, size_t length);

struct custom_command_s {
	const char				*command;
	command_callback_t		command_callback;
};

static void ota_callback(char *pnt_data, size_t length);
static void version_callback(char *pnt_data, size_t length);
static void ssid_callback(char *pnt_data, size_t length);
static void password_callback(char *pnt_data, size_t length);
static void server_callback(char *pnt_data, size_t length);
static void port_callback(char *pnt_data, size_t length);
static void wifi_active_callback(char *pnt_data, size_t length);
static void wifi_unlock_callback(char *pnt_data, size_t length);
static void wifi_wps_callback(char *pnt_data, size_t length);
static void wrn_flt_disable_callback(char *pnt_data, size_t length);
static void wrn_flt_clear_callback(char *pnt_data, size_t length);
static void reboot_callback(char *pnt_data, size_t length);
static void factory_callback(char *pnt_data, size_t length);
static void th_rh_callback(char *pnt_data, size_t length);
static void th_lux_callback(char *pnt_data, size_t length);
static void th_voc_callback(char *pnt_data, size_t length);
static void offset_rh_callback(char *pnt_data, size_t length);
static void offset_t_callback(char *pnt_data, size_t length);

static const struct custom_command_s custom_commands_table[] = {
	{ 	BLUFI_CMD_OTA,		    	ota_callback	         	},
	{ 	BLUFI_CMD_VERSION,	    	version_callback	     	},
	{	BLUFI_CMD_SSID,	            ssid_callback	            },
	{   BLUFI_CMD_PWD,              password_callback           },
	{ 	BLUFI_CMD_SERVER,		    server_callback		    	},
	{ 	BLUFI_CMD_PORT,			    port_callback	    	    },
	{	BLUFI_CMD_WIFI_ACTIVE,	    wifi_active_callback	    },
	{	BLUFI_CMD_WIFI_UNLOCK,   	wifi_unlock_callback        },
	{   BLUFI_CMD_WIFI_WPS,         wifi_wps_callback           },
	{	BLUFI_CMD_WRN_FLT_DISABLE,	wrn_flt_disable_callback    },
	{	BLUFI_CMD_WRN_FLT_CLEAR,	wrn_flt_clear_callback     	},
	{	BLUFI_CMD_REBOOT,	        reboot_callback	            },
	{   BLUFI_CMD_FACTORY,          factory_callback            },
	{   BLUFI_CMD_TH_RH,            th_rh_callback              },
	{   BLUFI_CMD_TH_LUX,           th_lux_callback             },
	{   BLUFI_CMD_TH_VOC,           th_voc_callback             },
	{   BLUFI_CMD_OFFSET_RH,        offset_rh_callback          },
	{   BLUFI_CMD_OFFSET_T,         offset_t_callback           },
};

int ble_analyse_received_data(const uint8_t *data, uint32_t data_len) {
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

	uint8_t fw_ver[] = { FW_VERSION_MAJOR + 0x30, FW_VERSION_MINOR + 0x30, FW_VERSION_PATCH + 0x30 };

	ret = esp_blufi_send_custom_data(fw_ver, sizeof(fw_ver));
	printf("version_callback - %d\n", ret);
}

static void ssid_callback(char *pnt_data, size_t length) {
	uint8_t ssid[SSID_SIZE + 1] = { 0 };

    if (length <= SSID_SIZE) {
    	memcpy(ssid, pnt_data, length);
        set_ssid((const uint8_t *) ssid);
    }
    else {
        printf("Received ssid data exceeds the storage limit.\n");
    }
}

static void password_callback(char *pnt_data, size_t length) {
	uint8_t pwd[PASSWORD_SIZE + 1] = { 0 };

    if (length <= PASSWORD_SIZE) {
    	memcpy(pwd, pnt_data, length);
        set_password((const uint8_t *) pwd);
    }
    else {
        printf("Received password data exceeds the storage limit.\n");
    }
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

	if ((wifi_active != 0x30) && (wifi_active != 0x31)) {
		wifi_active = 0x31;
	}
	wifi_active -= 0x30;

	if (length == 1) {
		if (get_wifi_unlocked()) {
			if (get_wifi_active() != wifi_active) {
				if (wifi_active) {
					char ssid[SSID_SIZE + 1] = { 0 };
					char pwd[PASSWORD_SIZE + 1] = { 0 };
					char server[SERVER_SIZE + 1] = { 0 };
					char port[PORT_SIZE + 1] = { 0 };

					get_ssid((uint8_t*) ssid);
					get_password((uint8_t*) pwd);
					get_server((uint8_t*) server);
					get_port((uint8_t*) port);

					if (!strlen(ssid) || !strlen(pwd) || !strlen(server)
							|| !strlen(port)) {
						printf("SSID, PSK, SERVER and PORT missing\n");
						return;
					}
				}

				set_wifi_active(wifi_active);
				if (get_wifi_active()) {
					if (get_sta_connected() == true) {
						tcp_connect_to_server();
					} else {
						blufi_wifi_connect();
					}
				} else {
					if (get_sta_connected() == true) {
						tcp_close_reconnect();
						esp_wifi_disconnect();
					}
				}
			}
		}
		else {
			 printf("No WIFI Unlocked.\n");
		}
	}
    else {
        printf("Received wifi_active data exceeds the storage limit.\n");
    }
}

static void wifi_unlock_callback(char *pnt_data, size_t length) {
    if (length != 16) {
        printf("Data length is too short.\n");
        return;
    }

    // Read key from eFUSE block 5
    uint8_t key[16];
    esp_err_t err = esp_efuse_read_block(EFUSE_BLK5, key, 0, 128);
    if (err != ESP_OK) {
        printf("Error reading key from eFUSE: %d\n", err);
        return;
    }

    // Initialize BLUFI security with the eFUSE key
    if (blufi_security_init_with_key(key) != ESP_OK) {
        printf("Failed to initialize BLUFI security with eFUSE key\n");
        return;
    }

    uint8_t hex_data[8]; // Buffer for the converted data (8 bytes)
    for (int i = 0; i < 8; i++) {
        sscanf(pnt_data + (i * 2), "%2hhx", &hex_data[i]);
    }

    uint8_t iv8 = 0; // Same IV as used in encryption

    // Decrypt the received data
    if (blufi_aes_decrypt(iv8, hex_data, 16) < 0) {
        printf("Decryption failed.\n");
        return;
    }

    // Debug: Print decrypted data
    printf("Decrypted Data: ");
    for (int i = 0; i < 8; i++) {
        printf("%02X", hex_data[i]);
    }
    printf("\n");

    // Prepare expected data: repeat the serial number twice in a 16-byte array
    uint8_t expected_data[8];
    uint32_t serial_number = get_serial_number();
    for (int i = 0; i < 8; i++) {
        expected_data[i] = (serial_number >> (i * 8)) & 0xFF;
    }

    // Debug: Print expected data (8 bytes)
    printf("Expected Data: ");
    for (int i = 0; i < 8; i++) {
        printf("%02X", expected_data[i]);
    }
    printf("\n");

    if (memcmp(hex_data, expected_data, 8) == 0) {
        printf("Unlocking successful.\n");
        int8_t key_flag = WIFI_UNLOCKED_VALUE;
        if (esp_efuse_write_block(EFUSE_BLK4, &key_flag, 0, 8) == ESP_OK) {           // Writing 1 byte at block 4, offset 0
        	printf("Written %02X to eFUSE block4 offset 0\n", key_flag);
            set_wifi_unlocked(1);
        } else {
            printf("Error writing to eFUSE: %d\n", err);
        }
    } else {
        printf("Unlocking failed.\n");
    }
}


static void wifi_wps_callback(char *pnt_data, size_t length) {
	int ret;

	if (!get_wps_is_enabled())
	{
		esp_wps_config_t wps_config = get_wps_config();
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
		printf("Im in WPS CALLBACK\n");
		set_wps_is_enabled(true);
	}
}

static void wrn_flt_disable_callback(char *ptr_data, size_t length) {
    if (ptr_data == NULL || length == 0) {
        printf("No data received for WRNFLTDISABLE command\n");
        return;
    }

    int value = atoi(ptr_data);
    if (value) {
    	printf("Disabling Filter Warning\n");
    	set_wrn_flt_disable(1);
    }
    else {
    	printf("Enabling Filter Warning\n");
    	set_wrn_flt_disable(0);
    }
}

static void wrn_flt_clear_callback(char *pnt_data, size_t length) {
	printf("Reseting Filter\n");
	statistic_reset_filter();
}

static void reboot_callback(char *pnt_data, size_t length) {
	esp_restart();
}

static void factory_callback(char *pnt_data, size_t length) {
	storage_set_default();
	esp_restart();
}

static void th_rh_callback(char *pnt_data, size_t length) {
	uint8_t th_rh = (uint8_t) (pnt_data[0]);

	if (th_rh < 0x34 && th_rh >= 0x30) {
		th_rh -= 0x30;
        set_relative_humidity_set(th_rh);
	}
    else {
        printf("Received RH THRESHOLD data exceeds the storage limit.\n");
    }
}
static void th_lux_callback(char *pnt_data, size_t length) {
	uint8_t th_lux = (uint8_t) (pnt_data[0]);

	if (th_lux < 0x34 && th_lux >= 0x30) {
		th_lux -= 0x30;
        set_lux_set(th_lux);
	}
    else {
        printf("Received LUX THRESHOLD data exceeds the storage limit.\n");
    }
}
static void th_voc_callback(char *pnt_data, size_t length) {
	uint8_t th_voc = (uint8_t) (pnt_data[0]);

	if (th_voc < 0x34 && th_voc >= 0x30) {
		th_voc -= 0x30;
        set_voc_set(th_voc);
	}
    else {
        printf("Received VOC THRESHOLD data exceeds the storage limit.\n");
    }
}

static void offset_rh_callback(char *pnt_data, size_t length) {
	int offset = atoi(pnt_data);

	 if (offset >= OFFSET_BOUND_MIN && offset <= OFFSET_BOUND_MAX) {
	     set_relative_humidity_offset(offset);
	  } else {
	     printf("Received offset is outside the allowed range (-50 to 50).\n");
	  }
}

static void offset_t_callback(char *pnt_data, size_t length) {
	int offset = atoi(pnt_data);

	 if (offset >= OFFSET_BOUND_MIN && offset <= OFFSET_BOUND_MAX) {
	     set_temperature_offset(offset);
	  } else {
	     printf("Received offset is outside the allowed range (-50 to 50).\n");
	  }
}
