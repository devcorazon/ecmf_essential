/*
 * structs.h
 *
 *  Created on: 1 dic 2022
 *      Author: Daniele Schirosi
 */

#ifndef SRC_INCLUDE_STRUCTS_H_
#define SRC_INCLUDE_STRUCTS_H_

#include "types.h"

///
struct noinit_data_s {

};

struct runtime_data_s {
	uint32_t    serial_number;
	uint16_t    fw_version_v_ctrl;
	uint8_t     mode_state;
	uint8_t     speed_state;
	uint8_t     direction_state;
	uint8_t     device_state;
	int16_t		temperature;
	uint16_t    relative_humidity;
	uint16_t    voc;
	int16_t     lux;
	int16_t     internal_temperature;
	int16_t     external_temperature;
};

///
struct configuration_settings_s {
	uint8_t		mode_set;
	uint8_t		speed_set;
	uint8_t     relative_humidity_set;
	uint8_t     lux_set;
	uint8_t     voc_set;
	int16_t		temperature_offset;
	int16_t    	relative_humidity_offset;
};

struct application_data_s {
	struct noinit_data_s				noinit_data;
	uint32_t							crc_noinit_data;
	struct runtime_data_s				runtime_data;
	struct configuration_settings_s		configuration_settings;
};

#endif /* SRC_INCLUDE_STRUCTS_H_ */
