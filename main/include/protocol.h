/*
 * protocol.h
 *
 *  Created on: 29 nov. 2023
 *      Author: youcef.benakmoume
 */

#ifndef MAIN_INCLUDE_PROTOCOL_H_
#define MAIN_INCLUDE_PROTOCOL_H_

#include "system.h"
#include "blufi_internal.h"
#include "protocol_internal.h"


#define PROTO_TRAME_LEN 1024

int proto_elaborate_data(uint8_t *in_data, size_t in_data_size, uint8_t *out_data, size_t *out_data_size);
int proto_prepare_identification(uint8_t *out_data, size_t *out_data_size);
int proto_prepare_answer_voluntary(uint8_t funct, uint16_t obj_id, uint16_t index, uint8_t *out_data, size_t *out_data_size);

#endif /* MAIN_INCLUDE_PROTOCOL_H_ */
