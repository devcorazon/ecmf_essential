/*
 * sensor.h
 *
 *  Created on: 8 lug 2023
 *      Author: Daniele Schirosi
 */

#ifndef MAIN_INCLUDE_SENSOR_H_
#define MAIN_INCLUDE_SENSOR_H_

#include "system.h"

int sensor_ntc_sample(float *temp);
int sensor_init(struct i2c_dev_s *i2c_dev, struct adc_dev_s *adc_dev);

#endif /* MAIN_INCLUDE_SENSOR_H_ */
