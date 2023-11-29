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

int proto_elaborate_data(struct protocol_trame *buf);

#endif /* MAIN_INCLUDE_PROTOCOL_H_ */
