/*
 * storage.c
 *
 *  Created on: 1 dic 2022
 *      Author: Daniele Schirosi
 */

#include <freertos/FreeRTOS.h>

#include "nvs_flash.h"
#include "esp_efuse.h"

#include "string.h"

#include "system.h"
#include "storage_internal.h"
#include "types.h"

#define MODE_SET_KEY		  "mode_set"
#define SPEED_SET_KEY		  "speed_set"
#define R_HUM_SET_KEY     	  "r_hum_set"
#define LUX_SET_KEY		      "lux_set"
#define VOC_SET_KEY		      "voc_set"
#define TEMP_OFFSET_KEY	      "temp_offset"
#define R_HUM_OFFSET_KEY      "r_hum_offset"
#define FILTER_OPERATING_KEY  "filter"
#define WRN_FLT_DISABLE_KEY   "wrnfltdisable"
#define WIFI_SSID_KEY         "ssid"
#define WIFI_PASSWORD_KEY     "password"
#define WIFI_ACTIVE_KEY       "active"
#define WIFI_PERIOD_KEY       "wifiperiod"
#define SERVER_KEY            "server"
#define PORT_KEY              "port"
#define OTA_URL_KEY           "ota"

static nvs_handle_t storage_handle;

static int storage_efuse_obtain(void);
static int storage_read_entry_with_idx(size_t i);
static int storage_save_entry_with_key(const char* key);
static int storage_save_all_entry(void);

// Data on ram.
//static __attribute__((section(".noinit"))) struct application_data_s application_data;
static struct application_data_s application_data;

static struct storage_entry_s storage_entry_poll[] = {
		{ MODE_SET_KEY,		           &application_data.configuration_settings.mode_set,					  DATA_TYPE_UINT8, 	  1 },
		{ SPEED_SET_KEY,		       &application_data.configuration_settings.speed_set,					  DATA_TYPE_UINT8, 	  1 },

		{ R_HUM_SET_KEY,		       &application_data.configuration_settings.relative_humidity_set,		  DATA_TYPE_UINT8, 	  1 },
		{ LUX_SET_KEY,		           &application_data.configuration_settings.lux_set,					  DATA_TYPE_UINT8, 	  1 },
		{ VOC_SET_KEY,		           &application_data.configuration_settings.voc_set,					  DATA_TYPE_UINT8, 	  1 },

		{ TEMP_OFFSET_KEY,    	       &application_data.configuration_settings.temperature_offset,			  DATA_TYPE_INT16, 	  2 },
		{ R_HUM_OFFSET_KEY,  	       &application_data.configuration_settings.relative_humidity_offset,	  DATA_TYPE_INT16, 	  2 },

 		{ FILTER_OPERATING_KEY,        &application_data.saved_data.filter_operating,                         DATA_TYPE_UINT32,   4 },
 		{ WRN_FLT_DISABLE_KEY,         &application_data.configuration_settings.wrn_flt_disable,              DATA_TYPE_UINT8,    1 },

		{ WIFI_SSID_KEY,               &application_data.wifi_configuration_settings.ssid,                    DATA_TYPE_STRING,   SSID_SIZE + 1 },
		{ WIFI_PASSWORD_KEY,           &application_data.wifi_configuration_settings.password,                DATA_TYPE_STRING,   PASSWORD_SIZE + 1 },
		{ WIFI_ACTIVE_KEY,             &application_data.wifi_configuration_settings.active,                  DATA_TYPE_UINT8,    1 },
		{ SERVER_KEY,                  &application_data.wifi_configuration_settings.server,                  DATA_TYPE_STRING,   SERVER_SIZE + 1 },
		{ PORT_KEY,                    &application_data.wifi_configuration_settings.port,                    DATA_TYPE_STRING,   PORT_SIZE + 1 },
		{ WIFI_PERIOD_KEY,             &application_data.wifi_configuration_settings.period,                  DATA_TYPE_UINT16,   2 },
		{ OTA_URL_KEY,                 &application_data.wifi_configuration_settings.ota_url,                 DATA_TYPE_STRING,   OTA_URL_SIZE + 1 }
};


static void storage_init_noinit_data(void) {
    if (application_data.crc_noinit_data != crc((const uint8_t *) &application_data.noinit_data, sizeof(application_data.noinit_data))) {
    	memset(&application_data.runtime_data, 0, sizeof(application_data.runtime_data));

    	application_data.crc_noinit_data = crc((const uint8_t *) &application_data.noinit_data, sizeof(application_data.noinit_data));
    }
}

static void storage_init_runtime_data(void) {
	memset(&application_data.runtime_data, 0, sizeof(application_data.runtime_data));

	application_data.runtime_data.temperature = TEMPERATURE_INVALID;
	application_data.runtime_data.relative_humidity = RELATIVE_HUMIDITY_INVALID;
	application_data.runtime_data.voc = VOC_INVALID;
	application_data.runtime_data.lux = LUX_INVALID;
	application_data.runtime_data.internal_temperature = TEMPERATURE_INVALID;
	application_data.runtime_data.external_temperature = TEMPERATURE_INVALID;
	application_data.runtime_data.device_state = 0;
	application_data.runtime_data.wifi_unlocked = 0;
}

static void storage_init_saved_data(void) {
	memset(&application_data.saved_data, 0, sizeof(application_data.saved_data));
}

static void storage_init_configuration_settings(void) {
	memset(&application_data.configuration_settings, 0, sizeof(application_data.configuration_settings));

	application_data.configuration_settings.relative_humidity_set = RH_THRESHOLD_SETTING_NOT_CONFIGURED;
	application_data.configuration_settings.voc_set = VOC_THRESHOLD_SETTING_NOT_CONFIGURED;
	application_data.configuration_settings.lux_set = LUX_THRESHOLD_SETTING_NOT_CONFIGURED;
	application_data.configuration_settings.speed_set = SPEED_NONE;
	application_data.configuration_settings.mode_set = MODE_OFF;
	application_data.configuration_settings.temperature_offset = 0;
	application_data.configuration_settings.relative_humidity_offset = 0;
	application_data.configuration_settings.wrn_flt_disable = 0;
}

static void storage_init_wifi_configuration_settings(void) {
	memset(&application_data.wifi_configuration_settings, 0, sizeof(application_data.wifi_configuration_settings));
}

int storage_init(void) {
	esp_err_t ret;

	storage_init_noinit_data();
    storage_init_runtime_data();
    storage_init_saved_data();
    storage_init_configuration_settings();
    storage_init_wifi_configuration_settings();

    storage_efuse_obtain();

    ret = nvs_flash_init();
    if ((ret == ESP_ERR_NVS_NO_FREE_PAGES) || (ret == ESP_ERR_NVS_NEW_VERSION_FOUND)) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    ret = nvs_open("storage", NVS_READWRITE, &storage_handle);
    if (ret == ESP_OK) {
    	for (size_t i = 0u; i < ARRAY_SIZE(storage_entry_poll); i++ ) {
    		storage_read_entry_with_idx(i);
    	}
    } else {
    	printf("nvs_open - ERROR\r\n");
    }

    return 0;
}

int storage_set_default(void) {
	memset(&application_data, 0, sizeof(application_data));

	storage_init_noinit_data();
	storage_init_runtime_data();
    storage_init_saved_data();
	storage_init_configuration_settings();
	storage_save_all_entry();

	return 0;
}

/// runtime data
uint32_t get_serial_number(void) {
	return application_data.runtime_data.serial_number;
}

int set_serial_number(uint32_t serial_number) {
	application_data.runtime_data.serial_number = serial_number;

	return 0;
}

uint16_t get_fw_version(void) {
	return application_data.runtime_data.fw_version_v_ctrl = FIRMWARE_VERSION;
}

uint8_t get_mode_state(void) {
	return application_data.runtime_data.mode_state;
}

int set_mode_state(uint8_t mode_state) {
//	if ((mode_state & 0x3f) > MODE_AUTOMATIC_CYCLE) {
//
//		return -1;
//	}

	application_data.runtime_data.mode_state = mode_state;

	return 0;
}

uint8_t get_speed_state(void) {
	return application_data.runtime_data.speed_state;
}

int set_speed_state(uint8_t speed_state) {
//	if ((speed_state & 0x3f) > SPEED_BOOST) {
//
//		return -1;
//	}

	application_data.runtime_data.speed_state = speed_state;

	return 0;
}

uint8_t get_direction_state(void) {
	return application_data.runtime_data.direction_state;
}

int set_direction_state(uint8_t direction_state) {
	application_data.runtime_data.direction_state= direction_state;

	return 0;
}

uint8_t get_device_state(void) {
	return application_data.runtime_data.device_state;
}

int set_device_state(uint8_t device_state) {
	application_data.runtime_data.device_state= device_state;

	return 0;
}

int16_t get_temperature(void) {
	return application_data.runtime_data.temperature;
}

int set_temperature(int16_t temperature) {
	application_data.runtime_data.temperature = temperature;

	return 0;
}

uint16_t get_relative_humidity(void) {
	return application_data.runtime_data.relative_humidity;
}

int set_relative_humidity(uint16_t relative_humidity) {
	application_data.runtime_data.relative_humidity = relative_humidity;

	return 0;
}

uint16_t get_voc(void) {
	return application_data.runtime_data.voc;
}

int set_voc(uint16_t voc) {
	application_data.runtime_data.voc = voc;

	return 0;
}

uint16_t get_lux(void) {
	return application_data.runtime_data.lux;
}

int set_lux(uint16_t lux) {
	application_data.runtime_data.lux = lux;

	return 0;
}

int16_t get_internal_temperature(void) {
	return application_data.runtime_data.internal_temperature;
}

int set_internal_temperature(int16_t temperature) {
	application_data.runtime_data.internal_temperature = temperature;

	return 0;
}

int16_t get_external_temperature(void) {
	return application_data.runtime_data.external_temperature;
}

int set_external_temperature(int16_t temperature) {
	application_data.runtime_data.external_temperature = temperature;

	return 0;
}

/// configuration settings
uint8_t get_mode_set(void) {
	return application_data.configuration_settings.mode_set;
}

int set_mode_set(uint8_t mode_set) {
	application_data.configuration_settings.mode_set = mode_set;

	storage_save_entry_with_key(MODE_SET_KEY);

	return 0;
}

uint8_t get_speed_set(void) {
	return application_data.configuration_settings.speed_set;
}

int set_speed_set(uint8_t speed_set) {
	application_data.configuration_settings.speed_set = speed_set;

	storage_save_entry_with_key(SPEED_SET_KEY);

	return 0;
}


uint8_t get_relative_humidity_set(void) {
	return application_data.configuration_settings.relative_humidity_set;
}

int set_relative_humidity_set(uint8_t relative_humidity_set) {
	if (relative_humidity_set > RH_THRESHOLD_SETTING_HIGH) {
		return -1;
	}

	application_data.configuration_settings.relative_humidity_set = relative_humidity_set;
	storage_save_entry_with_key(R_HUM_SET_KEY);

    return 0;
}

uint8_t get_lux_set(void) {
	return application_data.configuration_settings.lux_set;
}

int set_lux_set(uint8_t lux_set) {
	if (lux_set > LUX_THRESHOLD_SETTING_HIGH) {
		return -1;
	}

	application_data.configuration_settings.lux_set = lux_set;
	storage_save_entry_with_key(LUX_SET_KEY);

    return 0;
}

uint8_t get_voc_set(void) {
	return application_data.configuration_settings.voc_set;
}

int set_voc_set(uint8_t voc_set) {
	if (voc_set > VOC_THRESHOLD_SETTING_HIGH) {
		return -1;
	}

	application_data.configuration_settings.voc_set = voc_set;
	storage_save_entry_with_key(VOC_SET_KEY);

    return 0;
}

int16_t get_temperature_offset(void) {
	return application_data.configuration_settings.temperature_offset;
}

int set_temperature_offset(int16_t temperature_offset) {
	application_data.configuration_settings.temperature_offset = temperature_offset;

	storage_save_entry_with_key(TEMP_OFFSET_KEY);

    return 0;
}

int16_t get_relative_humidity_offset(void) {
	return application_data.configuration_settings.relative_humidity_offset;
}

int set_relative_humidity_offset(int16_t relative_humidity_offset) {
	application_data.configuration_settings.relative_humidity_offset = relative_humidity_offset;

	storage_save_entry_with_key(R_HUM_OFFSET_KEY);

    return 0;
}

uint16_t get_automatic_cycle_duration(void) {
    return application_data.runtime_data.automatic_cycle_duration;
}

int set_automatic_cycle_duration(uint16_t automatic_cycle_duration) {
    application_data.runtime_data.automatic_cycle_duration = automatic_cycle_duration;

    return 0;
}

uint32_t get_filter_operating(void) {
    return application_data.saved_data.filter_operating;

}

int set_filter_operating(uint32_t filter_operating) {
    application_data.saved_data.filter_operating = filter_operating;

    storage_save_entry_with_key(FILTER_OPERATING_KEY);

    return 0;
}

uint8_t get_wrn_flt_disable(void) {
    return application_data.configuration_settings.wrn_flt_disable;

}

int set_wrn_flt_disable(uint8_t wrn_flt_disable) {
	application_data.configuration_settings.wrn_flt_disable = wrn_flt_disable;

    storage_save_entry_with_key(WRN_FLT_DISABLE_KEY);

    return 0;
}


void get_ssid(uint8_t *ssid){
	memcpy(ssid, application_data.wifi_configuration_settings.ssid, sizeof(application_data.wifi_configuration_settings.ssid));
}

int set_ssid(const uint8_t *ssid) {
	memset(application_data.wifi_configuration_settings.ssid, 0, sizeof(application_data.wifi_configuration_settings.ssid));
	strcpy((char *)application_data.wifi_configuration_settings.ssid, (char *) ssid);

	storage_save_entry_with_key(WIFI_SSID_KEY);

	return 0;
}

void get_password(uint8_t *password){
	memcpy(password, application_data.wifi_configuration_settings.password, sizeof(application_data.wifi_configuration_settings.password));
}

int set_password(const uint8_t *password) {
	memset(application_data.wifi_configuration_settings.password, 0, sizeof(application_data.wifi_configuration_settings.password));
	strcpy((char *)application_data.wifi_configuration_settings.password, (char *) password);

	storage_save_entry_with_key(WIFI_PASSWORD_KEY);

	return 0;
}

void get_server(uint8_t *server){
	memcpy(server, application_data.wifi_configuration_settings.server, sizeof(application_data.wifi_configuration_settings.server));
}

int set_server(const uint8_t *server) {
	memset(application_data.wifi_configuration_settings.server, 0, sizeof(application_data.wifi_configuration_settings.server));
	strcpy((char *)application_data.wifi_configuration_settings.server, (char *) server);

	storage_save_entry_with_key(SERVER_KEY);
	return 0;
}

void get_port(uint8_t *port){
	memcpy(port, application_data.wifi_configuration_settings.port, sizeof(application_data.wifi_configuration_settings.port));
}

int set_port(const uint8_t *port) {
    memset(application_data.wifi_configuration_settings.port, 0, sizeof(application_data.wifi_configuration_settings.port));
    strcpy((char *)application_data.wifi_configuration_settings.port , (char *) port);

    storage_save_entry_with_key(PORT_KEY);
    return 0;
}

uint16_t get_wifi_period(void) {
	return application_data.wifi_configuration_settings.period;
}

int set_wifi_period(uint16_t wifi_period) {
	application_data.wifi_configuration_settings.period = wifi_period;

	storage_save_entry_with_key(WIFI_PERIOD_KEY);

    return 0;
}
void get_ota_url(uint8_t *ota_url) {
	memcpy(ota_url, application_data.wifi_configuration_settings.ota_url, sizeof(application_data.wifi_configuration_settings.ota_url));
}

int set_ota_url(const uint8_t *ota_url) {
    memset(application_data.wifi_configuration_settings.ota_url, 0, sizeof(application_data.wifi_configuration_settings.ota_url));
    strcpy((char *)application_data.wifi_configuration_settings.ota_url , (char *) ota_url);

    storage_save_entry_with_key(OTA_URL_KEY);
    return 0;
}

uint8_t get_wifi_active(void) {
	return application_data.wifi_configuration_settings.active;
}

int set_wifi_active(uint8_t active) {
	application_data.wifi_configuration_settings.active = active;

	storage_save_entry_with_key(WIFI_ACTIVE_KEY);

	return 0;
}

uint8_t get_wifi_unlocked(void) {
	return application_data.runtime_data.wifi_unlocked;
}

int set_wifi_unlocked(uint8_t unlocked) {
	application_data.runtime_data.wifi_unlocked = unlocked;

	return 0;
}

static int storage_efuse_obtain(void) {
    uint8_t serial_number_byte[4];
    uint8_t wifi_unlocked_key = 0;

    // Obtain serial number from block 3
    if (esp_efuse_read_block(EFUSE_BLK3, &serial_number_byte, 0, 32) != ESP_OK) {
    	return -1;
    }
    application_data.runtime_data.serial_number = ((uint32_t)serial_number_byte[0]) << 24 | ((uint32_t)serial_number_byte[1]) << 16 | ((uint32_t)serial_number_byte[2]) << 8 | ((uint32_t)serial_number_byte[3]);

    // Obtain wifi key flag from block 4
    if (esp_efuse_read_block(EFUSE_BLK4, &wifi_unlocked_key, 0, 8) != ESP_OK) {
    	return -1;
    }
    if ( wifi_unlocked_key == WIFI_UNLOCKED_VALUE ) {
    	set_wifi_unlocked(1);
    	printf("Setting wifi unlocked to 1\n");
    }

    return 0;
}

static int storage_read_entry_with_idx(size_t i) {
//	esp_err_t ret;

//	printf("storage_read_entry_with_idx: %u - %s - %02x - %u\r\n", i, storage_entry_poll[i].key, storage_entry_poll[i].type, storage_entry_poll[i].size);

    switch(storage_entry_poll[i].type) {
        case DATA_TYPE_UINT8:
        	nvs_get_u8(storage_handle, storage_entry_poll[i].key, (uint8_t *)storage_entry_poll[i].data);
//        	ret = nvs_get_u8(storage_handle, storage_entry_poll[i].key, (uint8_t *)storage_entry_poll[i].data);
//       	printf("storage_save_entry_with_key - nvs_get_u8 - index: %u - ret: %04x\r\n", i, ret);
            break;
        case DATA_TYPE_INT8:
        	nvs_get_i8(storage_handle, storage_entry_poll[i].key, (int8_t *)storage_entry_poll[i].data);
//        	ret = nvs_get_i8(storage_handle, storage_entry_poll[i].key, (int8_t *)storage_entry_poll[i].data);
//        	printf("storage_save_entry_with_key - nvs_get_i8 - index: %u - ret: %04x\r\n", i, ret);
            break;
        case DATA_TYPE_UINT16:
        	nvs_get_u16(storage_handle, storage_entry_poll[i].key, (uint16_t *)storage_entry_poll[i].data);
//        	ret = nvs_get_u16(storage_handle, storage_entry_poll[i].key, (uint16_t *)storage_entry_poll[i].data);
//        	printf("storage_save_entry_with_key - nvs_get_u16 - index: %u - ret: %04x\r\n", i, ret);
            break;
        case DATA_TYPE_INT16:
            nvs_get_i16(storage_handle, storage_entry_poll[i].key, (int16_t *)(storage_entry_poll[i].data));
//            ret = nvs_get_i16(storage_handle, storage_entry_poll[i].key, (int16_t *)(storage_entry_poll[i].data));
//        	printf("storage_save_entry_with_key - nvs_get_i16 - index: %u - ret: %04x\r\n", i, ret);
            break;
        case DATA_TYPE_UINT32:
            nvs_get_u32(storage_handle, storage_entry_poll[i].key, (uint32_t *)(storage_entry_poll[i].data));
//     	    ret = nvs_get_u32(storage_handle, storage_entry_poll[i].key, (uint32_t *)storage_entry_poll[i].data);
//        	printf("storage_save_entry_with_key - nvs_get_u32 - index: %u - ret: %04x\r\n", i, ret);
            break;
        case DATA_TYPE_INT32:
        	nvs_get_i32(storage_handle, storage_entry_poll[i].key, (int32_t *)storage_entry_poll[i].data);
//        	ret = nvs_get_i32(storage_handle, storage_entry_poll[i].key, (int32_t *)storage_entry_poll[i].data);
//        	printf("storage_save_entry_with_key - nvs_get_i32 - index: %u - ret: %04x\r\n", i, ret);
            break;
        case DATA_TYPE_UINT64:
        	nvs_get_u64(storage_handle, storage_entry_poll[i].key, (uint64_t *)storage_entry_poll[i].data);
//        	ret = nvs_get_u64(storage_handle, storage_entry_poll[i].key, (uint64_t *)storage_entry_poll[i].data);
//        	printf("storage_save_entry_with_key - nvs_get_u64 - index: %u - ret: %04x\r\n", i, ret);
            break;
        case DATA_TYPE_INT64:
        	nvs_get_i64(storage_handle, storage_entry_poll[i].key, (int64_t *)storage_entry_poll[i].data);
//        	ret = nvs_get_i64(storage_handle, storage_entry_poll[i].key, (int64_t *)storage_entry_poll[i].data);
//        	printf("storage_save_entry_with_key - nvs_get_i64 - index: %u - ret: %04x\r\n", i, ret);
            break;
        case DATA_TYPE_STRING:
        	size_t size = storage_entry_poll[i].size;
        	nvs_get_str(storage_handle, storage_entry_poll[i].key, (char *)storage_entry_poll[i].data, &size);
//        	ret = nvs_get_str(storage_handle, storage_entry_poll[i].key, (char *)storage_entry_poll[i].data, &size);
//        	printf("storage_save_entry_with_key - nvs_get_str - index: %u - ret: %04x\r\n", i, ret);
            break;
    }

    return 0;
}

static int storage_save_entry_with_key(const char* key) {
//	esp_err_t ret;
	size_t i;

	for (i = 0; i < ARRAY_SIZE(storage_entry_poll); i++ ) {
		if (!strcmp(key, storage_entry_poll[i].key)) {
			break;
		}
	}

	if (i == ARRAY_SIZE(storage_entry_poll)) {
    	printf("storage_save_entry_with_key - no entry\r\n");
		return -1;
	}

//	printf("storage_save_entry_with_key: %u - %s - %02x - %u\r\n", i, storage_entry_poll[i].key, storage_entry_poll[i].type, storage_entry_poll[i].size);

	switch(storage_entry_poll[i].type) {
        case DATA_TYPE_UINT8:
        	nvs_set_u8(storage_handle, storage_entry_poll[i].key, (uint8_t)*((uint8_t *)(storage_entry_poll[i].data)));
//        	ret = nvs_set_u8(storage_handle, storage_entry_poll[i].key, (uint8_t)*((uint8_t *)(storage_entry_poll[i].data)));
//        	printf("storage_save_entry_with_key - nvs_set_u8 - index:  %u - ret: %04x\r\n", i, ret);
            break;
        case DATA_TYPE_INT8:
        	nvs_set_i8(storage_handle, storage_entry_poll[i].key, (int8_t)*((int8_t *)(storage_entry_poll[i].data)));
//        	ret = nvs_set_i8(storage_handle, storage_entry_poll[i].key, (int8_t)*((int8_t *)(storage_entry_poll[i].data)));
//        	printf("storage_save_entry_with_key - nvs_set_i8 - index:  %u - ret: %04x\r\n", i, ret);
            break;
        case DATA_TYPE_UINT16:
        	nvs_set_u16(storage_handle, storage_entry_poll[i].key, (uint16_t)*((uint16_t *)(storage_entry_poll[i].data)));
//        	ret = nvs_set_u16(storage_handle, storage_entry_poll[i].key, (uint16_t)*((uint16_t *)(storage_entry_poll[i].data)));
//        	printf("storage_save_entry_with_key - nvs_set_u16 - index:  %u - ret: %04x\r\n", i, ret);
            break;
        case DATA_TYPE_INT16:
        	nvs_set_i16(storage_handle, storage_entry_poll[i].key, *(int16_t *)(storage_entry_poll[i].data));
//        	ret = nvs_set_i16(storage_handle, storage_entry_poll[i].key, *(int16_t *)(storage_entry_poll[i].data));
//        	printf("storage_save_entry_with_key - nvs_set_i16 - index:  %u - ret: %04x\r\n", i, ret);
            break;
        case DATA_TYPE_UINT32:
        	nvs_set_u32(storage_handle, storage_entry_poll[i].key, (uint32_t)*((uint32_t *)(storage_entry_poll[i].data)));
//        	ret = nvs_set_u32(storage_handle, storage_entry_poll[i].key, (uint32_t)*((uint32_t *)(storage_entry_poll[i].data)));
//        	printf("storage_save_entry_with_key - nvs_set_u32 - index:  %u - ret: %04x\r\n", i, ret);
            break;
        case DATA_TYPE_INT32:
        	nvs_set_i32(storage_handle, storage_entry_poll[i].key, (int32_t)*((int32_t *)(storage_entry_poll[i].data)));
//        	ret = nvs_set_i32(storage_handle, storage_entry_poll[i].key, (int32_t)*((int32_t *)(storage_entry_poll[i].data)));
//        	printf("storage_save_entry_with_key - nvs_set_i32 - index:  %u - ret: %04x\r\n", i, ret);
            break;
        case DATA_TYPE_UINT64:
        	nvs_set_u64(storage_handle, storage_entry_poll[i].key, (uint64_t)*((uint64_t *)(storage_entry_poll[i].data)));
//        	ret = nvs_set_u64(storage_handle, storage_entry_poll[i].key, (uint64_t)*((uint64_t *)(storage_entry_poll[i].data)));
//        	printf("storage_save_entry_with_key - nvs_set_u64 - index:  %u - ret: %04x\r\n", i, ret);
            break;
        case DATA_TYPE_INT64:
        	nvs_set_i64(storage_handle, storage_entry_poll[i].key, (int64_t)*((int64_t *)(storage_entry_poll[i].data)));
//        	ret = nvs_set_i64(storage_handle, storage_entry_poll[i].key, (int64_t)*((int64_t *)(storage_entry_poll[i].data)));
//        	printf("storage_save_entry_with_key - nvs_set_i64 - index:  %u - ret: %04x\r\n", i, ret);
            break;
        case DATA_TYPE_STRING:
        	nvs_set_str(storage_handle, storage_entry_poll[i].key, (const char *)(storage_entry_poll[i].data));
//        	ret = nvs_set_str(storage_handle, storage_entry_poll[i].key, (const char *)(storage_entry_poll[i].data));
//       	printf("storage_save_entry_with_key - nvs_set_str - index:  %u - ret: %04x\r\n", i, ret);
            break;
    }

	nvs_commit(storage_handle);

    return 0;
}

static int storage_save_all_entry(void) {
	size_t i;

	for (i = 0; i < ARRAY_SIZE(storage_entry_poll); i++ ) {
		storage_save_entry_with_key(storage_entry_poll[i].key);
	}

    return 0;
}
