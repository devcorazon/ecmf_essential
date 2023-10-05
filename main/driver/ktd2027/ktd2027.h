/**
 * @file ltr303.c
 *
 * Copyright (c) Youcef BENAKMOUME
 *
 */

#ifndef __KTD2026_H__
#define __KTD2026_H__

#include "system.h"
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define KTD2027_I2C_ADDR 0x30

/* KTD2027 register map */
#define KTD2027_REG_EN_RST		0x00
#define KTD2027_REG_FLASH_PERIOD	0x01
#define KTD2027_REG_PWM1_TIMER		0x02
#define KTD2027_REG_PWM2_TIMER		0x03
#define KTD2027_REG_LED_EN		    0x04
#define KTD2027_REG_LED_RAMP	    0x05
#define KTD2027_REG_RED_LED		    0x06
#define KTD2027_REG_GREEN_LED		0x07
#define KTD2027_REG_BLUE_LED		0x08
#define KTD2027_TIME_UNIT		    500

#define KTD2027_RED_LED_EN	        0x01
#define KTD2027_GREEN_LED_EN	    0x04
#define KTD2027_BLUE_LED_EN	     	0x10

#define LED_OFF			0x00

#define MAX_BRIGHTNESS_VALUE 0xFF
#define	MAX_NUM_LEDS		3

struct ktd2027_config {
	struct i2c_dev_s *i2c_dev;
	uint8_t	i2c_dev_address;
};

enum {
    RGB_LED_MODE_OFF            = 0,
    RGB_LED_MODE_ON,
    RGB_LED_MODE_BLANK,

	RGB_LED_MODE_BLINK,

	RGB_LED_MODE_SPEED_NIGHT_FADE,
	RGB_LED_MODE_SPEED_BLINK,

	RGB_LED_MODE_NOT_CONF,
    RGB_LED_MODE_PRE,
    RGB_LED_MODE_BLINK_INVERTED,
	RGB_LED_MODE_POST,
};

int ktd2027_init(struct i2c_dev_s *i2c_dev);
int ktd2027_led_set(uint8_t color, uint8_t mode);

#ifdef __cplusplus
}
#endif

/**@}*/

#endif  // __KTD2026_H__
