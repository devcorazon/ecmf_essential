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

#define ESP_WIFI_SSID              "ECMF-{serial_number}"
#define ESP_WIFI_PASS              "mypassword"
#define ESP_WIFI_CHANNEL           1

#define BLUFI_CMD_OTA   		   "OTA"
#define BLUFI_CMD_VERSION   	   "VERSION"
#define BLUFI_CMD_SSID             "SSID"
#define BLUFI_CMD_PWD              "PWD"
#define BLUFI_CMD_SERVER    	   "SERVER"
#define BLUFI_CMD_PORT      	   "PORT"
#define BLUFI_CMD_WIFI_ACTIVE	   "WIFIACT"
#define BLUFI_CMD_WIFI_UNLOCK      "WIFIUNLOCK"
#define BLUFI_CMD_WIFI_WPS	       "WPS"
#define BLUFI_CMD_WRN_FLT_DISABLE  "WRNFLTDISABLE"
#define BLUFI_CMD_WRN_FLT_CLEAR	   "WRNFLTCLEAR"
#define BLUFI_CMD_REBOOT	       "REBOOT"
#define BLUFI_CMD_FACTORY	       "FACTORY"
#define BLUFI_CMD_TH_RH            "TH_RH"
#define BLUFI_CMD_TH_LUX    	   "TH_LUX"
#define BLUFI_CMD_TH_VOC	       "TH_VOC"
#define BLUFI_CMD_OFFSET_RH	       "OFFSET_RH"
#define BLUFI_CMD_OFFSET_T   	   "OFFSET_T"


#define WIFI_ADDRESS_LEN                6
#define BT_ADDRESS_LEN                  6

#define BLUFI_TASK_STACK_SIZE			(configMINIMAL_STACK_SIZE * 4)
#define	BLUFI_TASK_PRIORITY				(1)
#define	BLUFI_TASK_PERIOD				(100ul / portTICK_PERIOD_MS)

#define OTA_TASK_STACK_SIZE			    (configMINIMAL_STACK_SIZE * 4)
#define	OTA_TASK_PRIORITY		        (1)
#define	OTA_TASK_PERIOD			    	(100ul / portTICK_PERIOD_MS)

#define WIFI_LIST_NUM                   10
#define WPS_MODE                        WPS_TYPE_PBC

#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK

#define WIFI_SSID_ANY					((uint8_t[SSID_SIZE]) { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff })
#define PASSWORD_ANY					((uint8_t[PASSWORD_SIZE]) { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff })
#define SERVER_ANY					    ((uint8_t[SERVER_SIZE]) { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff })
#define PORT_ANY					    ((uint8_t[PORT_SIZE]) { 0xff, 0xff, 0xff, 0xff, 0xff })

#define BSSID_SIZE                               6
#define SSID_SIZE                               32
#define PASSWORD_SIZE                           64
#define SERVER_SIZE                             32
#define PORT_SIZE                                5

#define OTA_URL_SIZE                           256

#define MIN_PORT_VALUE                            1
#define MAX_PORT_VALUE                        65535

#define BLE_ADV_EXPIRY_TIME                       1     // ( in min )
#define TCP_RECONNECTING_DELAY                 1000     // ( in msec )
#define TCP_TRAME_RX_TIMEOUT                   2000     // ( in msec )
#define WIFI_RECONNECTING_DELAY                1000     // ( in msec )

#define TCP_KEEPALIVE_IDLE_TIME                  30     // Time in seconds that the connection must be idle before starting to send keepalive probes
#define TCP_KEEPALIVE_INTVAL                     10     // Time in seconds between individual keepalive probes
#define TCP_KEEPALIVE_COUNT                       3     // Number of keepalive probes to send before deciding that the connection is broken

#endif /* MAIN_INCLUDE_BLUFI_INTERNAL_H_ */
