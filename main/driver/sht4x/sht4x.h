/*
 * sht4x.h
 *
 *  Created on: 8 lug 2023
 *      Author: Daniele Schirosi
 */

#ifndef MAIN_DRIVER_SHT4X_H_
#define MAIN_DRIVER_SHT4X_SHT4X_H_

#include "system.h"

#define SHT4X_I2C_ADDRESS		0x44

#define SHT4X_CRC_POLY			0x31
#define SHT4X_CRC_INIT			0xff

#define SHT4X_CMD_RESET			0x94
#define SHT4X_CMD_MEASURE_HIGH	0xfd

#define SHT4X_RESET_WAIT_MS		1u
#define SHT4X_MEASURE_WAIT_MS	10u

struct sht4x_config {
	struct i2c_dev_s *i2c_dev;
	uint8_t	i2c_dev_address;
};

struct sht4x_data {
	uint16_t t_sample;
	uint16_t rh_sample;
};

int sht4x_init(struct i2c_dev_s *i2c_dev);
int sht4x_sample(float *t_amb, float *r_hum);

#endif /* MAIN_DRIVER_SHT4X_SHT4X_H_ */
