/*
 * blufi_internal.h
 *
 *  Created on: 22 oct. 2023
 *      Author: youcef.benakmoume
 */

#ifndef MAIN_INCLUDE_BLUFI_INTERNAL_H_
#define MAIN_INCLUDE_BLUFI_INTERNAL_H_

#define WIFI_CONNECTION_MAXIMUM_RETRY 4
#define INVALID_REASON                255
#define INVALID_RSSI                  -128

#define ESP_WIFI_SSID      "ECMF-{serial_number}"
#define ESP_WIFI_PASS      "mypassword"
#define ESP_WIFI_CHANNEL   1

#define BLUFI_CMD_OTA   		"OTA"
#define BLUFI_CMD_VERSION   	"VERSION"
#define BLUFI_CMD_SERVER    	"SERVER"
#define BLUFI_CMD_PORT      	"PORT"
#define BLUFI_CMD_WIFI_ACTIVE	"WIFIACT"
#define BLUFI_CMD_WIFI_WPS	    "WPS"

#define WIFI_ADDRESS_LEN        20
#define BT_ADDRESS_LEN          20

#define BLUFI_TASK_STACK_SIZE			(configMINIMAL_STACK_SIZE * 4)
#define	BLUFI_TASK_PRIORITY				(1)
#define	BLUFI_TASK_PERIOD				(100ul / portTICK_PERIOD_MS)

#define OTA_TASK_STACK_SIZE			(configMINIMAL_STACK_SIZE * 4)
#define	OTA_TASK_PRIORITY		    (1)
#define	OTA_TASK_PERIOD				(100ul / portTICK_PERIOD_MS)

#define WIFI_LIST_NUM           10
#define WPS_MODE WPS_TYPE_PBC

#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK

#define BSSID_SIZE                               6
#define SSID_SIZE                               32
#define PASSWORD_SIZE                           64
#define SERVER_SIZE                             32
#define PORT_SIZE                                5

#define OTA_URL_SIZE                           256

#define MAX_PORT_VALUE                        65535

#define BLE_ADV_EXPIRY_TIME                      1     // ( in min )
#define WIFI_RECONNECTING_DELAY               1000     // (in msec )

#endif /* MAIN_INCLUDE_BLUFI_INTERNAL_H_ */
