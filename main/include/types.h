/*
 * types.h
 *
 *  Created on: 1 dic 2022
 *      Author: Daniele Schirosi
 */

#ifndef SRC_INCLUDE_TYPES_H_
#define SRC_INCLUDE_TYPES_H_

#define TEMPERATURE_INVALID						INT16_MAX
#define RELATIVE_HUMIDITY_INVALID				UINT16_MAX
#define VOC_INVALID								UINT16_MAX
#define LUX_INVALID								UINT16_MAX

#define TEMPERATURE_SCALE						(10)
#define RELATIVE_HUMIDITY_SCALE					(10u)
#define VOC_SCALE								(1u)
#define LUMINOSITY_SCALE						(1000u)

// Offset
#define TEMPERATURE_OFFSET_FIXED				(0)
#define RELATIVE_HUMIDITY_OFFSET_FIXED			(0)

#define OFFSET_BOUND_MIN						(-500)
#define OFFSET_BOUND_MAX						(500)


#define SET_VALUE_TO_TEMP_RAW(val)				(val == TEMPERATURE_INVALID ? TEMPERATURE_INVALID : (int16_t) (val * TEMPERATURE_SCALE))
#define SET_VALUE_TO_RH_RAW(val)				(val == RELATIVE_HUMIDITY_INVALID ? RELATIVE_HUMIDITY_INVALID : (uint16_t) (val * RELATIVE_HUMIDITY_SCALE))

#define TEMP_RAW_TO_INT(t)						(int16_t) (t / TEMPERATURE_SCALE)
#define TEMP_RAW_TO_DEC(t)						(int16_t) (abs((int) t) % TEMPERATURE_SCALE)

#define RH_RAW_TO_INT(rh)						(uint16_t) (rh / RELATIVE_HUMIDITY_SCALE)
#define RH_RAW_TO_DEC(rh)						(uint16_t) (rh % RELATIVE_HUMIDITY_SCALE)

// Rh threshold setting
enum {
    RH_THRESHOLD_SETTING_NOT_CONFIGURED			= 0x00,
    RH_THRESHOLD_SETTING_LOW					= 0x01,
    RH_THRESHOLD_SETTING_MEDIUM					= 0x02,
    RH_THRESHOLD_SETTING_HIGH					= 0x03,
};

// Luminosity sensor setting
enum {
    LUMINOSITY_SENSOR_SETTING_NOT_CONFIGURED	= 0x00,
    LUMINOSITY_SENSOR_SETTING_LOW				= 0x01,
    LUMINOSITY_SENSOR_SETTING_MEDIUM			= 0x02,
    LUMINOSITY_SENSOR_SETTING_HIGH				= 0x03,
};

// VOC threshold setting
enum {
	VOC_THRESHOLD_SETTING_NOT_CONFIGURED		= 0x00,
	VOC_THRESHOLD_SETTING_LOW					= 0x01,
	VOC_THRESHOLD_SETTING_MEDIUM				= 0x02,
	VOC_THRESHOLD_SETTING_HIGH					= 0x03,
};

// Mode
enum {
    MODE_OFF              						= 0x00,
    MODE_IMMISSION        						= 0x01,
    MODE_EMISSION         						= 0x02,
    MODE_FIXED_CYCLE      						= 0x03,
    MODE_AUTOMATIC_CYCLE  						= 0x04,
};

// Speed
enum {
    SPEED_NONE									= 0x00,
    SPEED_NIGHT									= 0x01,
    SPEED_LOW									= 0x02,
    SPEED_MEDIUM								= 0x03,
    SPEED_HIGH									= 0x04,
    SPEED_BOOST									= 0x05,
};

// Direction of rotation state
enum {
	DIRECTION_NONE								= 0x00,
	DIRECTION_OUT								= 0x01,
	DIRECTION_IN								= 0x02,
};

enum rgb_led_color {
    RGB_LED_COLOR_NONE          = 0,
    RGB_LED_COLOR_RED           = 1,
    RGB_LED_COLOR_GREEN         = 2,
    RGB_LED_COLOR_BLUE          = 3,
	RGB_LED_COLOR_ALL           = 4,
};

enum rgb_led_mode {
    RGB_LED_MODE_OFF            = 0,
    RGB_LED_MODE_ON             = 1,
    RGB_LED_MODE_BLINK          = 2,
};

enum rgb_led_duration {
    RGB_LED_DURATION_ONESHOT    = 5,
    RGB_LED_DURATION_500MS      = 500,
    RGB_LED_DURATION_1000MS     = 1000,
    RGB_LED_DURATION_1500MS     = 1500,
    RGB_LED_DURATION_2000MS     = 2000,
    RGB_LED_DURATION_FOREVER    = 0xFF,
};

#endif /* SRC_INCLUDE_TYPES_H_ */
