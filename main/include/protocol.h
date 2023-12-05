/*
 * protocol.h
 *
 *  Created on: 29 nov. 2023
 *      Author: youcef.benakmoume
 */

#ifndef MAIN_INCLUDE_PROTOCOL_H_
#define MAIN_INCLUDE_PROTOCOL_H_

#include "freertos/ringbuf.h"

#include "system.h"
#include "blufi_internal.h"
#include "protocol_internal.h"

int proto_elaborate_data(uint8_t *in_data, size_t in_data_size, uint8_t *out_data, size_t out_data_size);
int proto_send_identification(uint8_t *out_data, size_t out_data_size);

#endif /* MAIN_INCLUDE_PROTOCOL_H_ */
