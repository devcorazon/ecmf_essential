/*
 * rgb_led.h
 *
 *  Created on: 30 juin 2023
 *      Author: youcef.benakmoume
 */

#ifndef MAIN_INCLUDE_RGB_LED_H_
#define MAIN_INCLUDE_RGB_LED_H_

#include "system.h"

struct blink_params {
    uint8_t led_color;
    uint32_t blink_duration;
    uint32_t blink_period;
};

int rgb_led_init(struct i2c_dev_s *i2c_dev);
int rgb_led_set(uint8_t led_color,uint8_t led_mode);
int rgb_led_blink(uint8_t led_color, uint32_t blink_duration, uint32_t blink_period);

#endif /* MAIN_INCLUDE_RGB_LED_H_ */
