/*
 * storage_internal.h
 *
 *  Created on: 1 dic 2022
 *      Author: dschirosi
 */

#ifndef MAIN_INCLUDE_STORAGE_INTERNAL_H_
#define MAIN_INCLUDE_STORAGE_INTERNAL_H_

#include "storage.h"

enum data_type_e {
    DATA_TYPE_UINT8 = 0,
    DATA_TYPE_INT8,
    DATA_TYPE_UINT16,
    DATA_TYPE_INT16,
    DATA_TYPE_UINT32,
    DATA_TYPE_INT32,
    DATA_TYPE_UINT64,
    DATA_TYPE_INT64,
    DATA_TYPE_STRING,

	//
	DATA_TYPE_COUNT,
};

struct storage_entry_s {
	const char	*key;
	void		*data;
	uint8_t		type;
	size_t		size;
};

static inline uint32_t crc(const void *data, size_t size) {
	/* TODO */
	// caluclate crc

	return 0;
}

int16_t get_internal_temperature(void);
int set_internal_temperature(int16_t temperature);

int16_t get_external_temperature(void);
int set_external_temperature(int16_t temperature);

uint16_t get_lux(void);
int set_lux(int16_t lux);

#endif /* MAIN_INCLUDE_STORAGE_INTERNAL_H_ */
