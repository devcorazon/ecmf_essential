/*
 * rgb_led.h
 *
 *  Created on: 30 juin 2023
 *      Author: youcef.benakmoume
 */

#ifndef MAIN_INCLUDE_RGB_LED_H_
#define MAIN_INCLUDE_RGB_LED_H_

#include "system.h"

#define RGB_LED_COLOR_OFFSET		8

#define RGB_LED_MODE_OFFSET			3
#define RGB_LED_MODE_CONF_OFFSET	8

int rgb_led_init(struct i2c_dev_s *i2c_dev);
int rgb_led_set(uint8_t led_color, uint8_t led_mode);
int rgb_led_mode(uint8_t color, uint32_t mode, bool end);

#endif /* MAIN_INCLUDE_RGB_LED_H_ */
