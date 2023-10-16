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

#endif /* MAIN_INCLUDE_STORAGE_INTERNAL_H_ */
