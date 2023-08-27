/*
 * sgp40.h
 *
 *  Created on: 8 lug 2023
 *      Author: Daniele Schirosi
 */

#ifndef MAIN_DRIVER_SGP40_SGP40_H_
#define MAIN_DRIVER_SGP40_SGP40_H_

#include "system.h"
#include "sensirion_gas_index_algorithm.h"

#define SGP40_I2C_ADDRESS 		0x59

#define SGP40_CRC_POLY			0x31
#define SGP40_CRC_INIT			0xff

#define SGP40_CMD_HEATER_OFF	0x3615
#define SGP40_CMD_MEASURE_RAW	0x260f

#define SGP40_RESET_WAIT_MS		10u
#define SGP40_MEASURE_WAIT_MS	30u

struct sgp40_config {
	struct i2c_dev_s *i2c_dev;
	uint8_t	i2c_dev_address;
};

struct sgp40_data {
	uint16_t voc_raw_sample;
	GasIndexAlgorithmParams engine_algorithm_params;
};

int sgp40_init(struct i2c_dev_s *i2c_dev);
int sgp40_sample(float t_amb, float r_hum, uint16_t *voc_idx);

#endif /* MAIN_DRIVER_SGP40_SGP40_H_ */
