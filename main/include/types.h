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
#define LUX_SCALE						        (1000u)

// Offset
#define TEMPERATURE_OFFSET_FIXED				(0)
#define RELATIVE_HUMIDITY_OFFSET_FIXED			(0)

#define OFFSET_BOUND_MIN						(-500)
#define OFFSET_BOUND_MAX						(500)

#define SET_VALUE_TO_TEMP_RAW(val)				(val == TEMP_F_INVALID ? TEMPERATURE_INVALID : (int16_t) (val * TEMPERATURE_SCALE))
#define SET_VALUE_TO_RH_RAW(val)				(val == HUM_F_INVALID ? RELATIVE_HUMIDITY_INVALID : (uint16_t) (val * RELATIVE_HUMIDITY_SCALE))
#define SET_VALUE_TO_LUX_RAW(val)				(val == LUX_F_INVALID ? LUX_INVALID : (uint16_t) (val * LUX_SCALE))

#define TEMP_RAW_TO_INT(t)						(int16_t) (t / TEMPERATURE_SCALE)
#define TEMP_RAW_TO_DEC(t)						(int16_t) (abs((int) t) % TEMPERATURE_SCALE)

#define RH_RAW_TO_INT(rh)						(uint16_t) (rh / RELATIVE_HUMIDITY_SCALE)
#define RH_RAW_TO_DEC(rh)						(uint16_t) (rh % RELATIVE_HUMIDITY_SCALE)

#define LUX_RAW_TO_INT(lux)						(int16_t) (lux / LUX_SCALE)
#define LUX_RAW_TO_DEC(lux)						(int16_t) (abs((int) lux ) % LUX_SCALE)

#define SECONDS_PER_HOUR 30 //3600
#define SECONDS_TO_MS(seconds) ((seconds) * 1000UL)

#define ceiling_fraction(numerator, divider) (((numerator) + ((divider) - 1)) / (divider))

/// Timing
#define DURATION_IMMISSION_EMISSION				(1U * SECONDS_PER_HOUR)
#define DURATION_FIXED_CYCLE					(45U)//(5U)
#define DURATION_AUTOMATIC_CYCLE_OUT			(45U)//(10U)
#define DURATION_AUTOMATIC_CYCLE_IN				(150U)//(20U)
#define DURATION_RESTART_AUTOMATIC_CYCLE		(10U * SECONDS_PER_HOUR)
#define DURATION_EXTRA_CYCLE_BOOST				(200U)//(10U)
#define DURATION_RESTART_EXTRA_CYCLE			(60U * 60U)

/// Thresholds and differentials
#define LUMINOSITY_THRESHOLD_LOW				(100U * LUX_SCALE / 1000U)
#define LUMINOSITY_THRESHOLD_MEDIUM				(125U * LUX_SCALE / 1000U)
#define LUMINOSITY_THRESHOLD_HIGH				(150U * LUX_SCALE / 1000U)

#define LUMINOSITY_DIFFERENTIAL_LOW				(0U * LUX_SCALE / 1000U)
#define LUMINOSITY_DIFFERENTIAL_HIGH			(5U * LUX_SCALE / 1000U)

#define RH_THRESHOLD_LOW						(55U * RELATIVE_HUMIDITY_SCALE)
#define RH_THRESHOLD_MEDIUM						(60U * RELATIVE_HUMIDITY_SCALE)
#define RH_THRESHOLD_HIGH						(65U * RELATIVE_HUMIDITY_SCALE)

#define RH_DIFFERENTIAL_LOW						(5U * RELATIVE_HUMIDITY_SCALE)
#define RH_DIFFERENTIAL_HIGH					(0U * RELATIVE_HUMIDITY_SCALE)

#define RH_THRESHOLD_ADV_CTRL_PERCENTAGE		(90U)

#define VOC_THRESHOLD_LOW						(250U * VOC_SCALE)
#define VOC_THRESHOLD_MEDIUM					(300U * VOC_SCALE)
#define VOC_THRESHOLD_HIGH						(350U * VOC_SCALE)

#define VOC_DIFFERENTIAL_LOW					(10U * VOC_SCALE)
#define VOC_DIFFERENTIAL_HIGH					(0U * VOC_SCALE)

// Flags
#define COND_LUMINOSITY							BIT(0)
#define COND_RH_EXTRA_CYCLE						BIT(1)
#define COND_VOC_EXTRA_CYCLE					BIT(2)

// Offset
#define TEMPERATURE_OFFSET_FIXED				(0)
#define RELATIVE_HUMIDITY_OFFSET_FIXED			(0)

#define OFFSET_BOUND_MIN						(-500)
#define OFFSET_BOUND_MAX						(500)

// FIlters
#define FILTER_THRESHOLD						(1000U)

// Enumerations

// Rotation setting
enum {
    ROTATION_SETTING_NOT_CONFIGURED				= 0x00,
    ROTATION_SETTING_CONCORDANT					= 0x01,
    ROTATION_SETTING_OPPOSITE					= 0x02,
};

// Rh threshold setting
enum {
    RH_THRESHOLD_SETTING_NOT_CONFIGURED			= 0x00,
    RH_THRESHOLD_SETTING_LOW					= 0x01,
    RH_THRESHOLD_SETTING_MEDIUM					= 0x02,
    RH_THRESHOLD_SETTING_HIGH					= 0x03,
};

// Luminosity sensor setting
enum {
    LUX_THRESHOLD_SETTING_NOT_CONFIGURED	= 0x00,
	LUX_THRESHOLD_SETTING_LOW				= 0x01,
	LUX_THRESHOLD_SETTING_MEDIUM			= 0x02,
	LUX_THRESHOLD_SETTING_HIGH				= 0x03,
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
	MODE_AUTOMATIC_CYCLE_EXTRA_CYCLE			= BIT(6),
	MODE_AUTOMATIC_CYCLE_CALCULATE_DURATION		= BIT(7),
};

// Speed
enum {
    SPEED_NONE									= 0x00,
    SPEED_NIGHT									= 0x01,
    SPEED_LOW									= 0x02,
    SPEED_MEDIUM								= 0x03,
    SPEED_HIGH									= 0x04,
    SPEED_BOOST									= 0x05,
	SPEED_AUTOMATIC_CYCLE_FORCE_BOOST			= BIT(6),
	SPEED_AUTOMATIC_CYCLE_FORCE_NIGHT			= BIT(7),
};

// Direction of rotation state
enum {
	DIRECTION_NONE								= 0x00,
	DIRECTION_IN								= 0x01,
	DIRECTION_OUT								= 0x02,
};

enum rgb_led_color {
    RGB_LED_COLOR_NONE          = 0,
    RGB_LED_COLOR_RED,
    RGB_LED_COLOR_GREEN,
    RGB_LED_COLOR_BLUE,

	RGB_LED_COLOR_AQUA_RH,
	RGB_LED_COLOR_GREEN_VOC,
	RGB_LED_COLOR_YELLOW_LUX,

	RGB_LED_COLOR_POWER_OFF,
	RGB_LED_COLOR_IMMISSION,
	RGB_LED_COLOR_EMISSION,
	RGB_LED_COLOR_FIXED_CYCLE,
	RGB_LED_COLOR_AUTOMATIC_CYCLE,

	RGB_LED_BLANK,

	RGB_LED_COLOR_AQUA_RH_NOT_CONF,
	RGB_LED_COLOR_AQUA_RH_PRE,
	RGB_LED_COLOR_AQUA_RH_BLINK_INVERTED,
	RGB_LED_COLOR_AQUA_RH_END,

	RGB_LED_COLOR_GREEN_VOC_NOT_CONF,
	RGB_LED_COLOR_GREEN_VOC_PRE,
	RGB_LED_COLOR_GREEN_VOC_BLINK_INVERTED,
	RGB_LED_COLOR_GREEN_VOC_END,

	RGB_LED_COLOR_YELLOW_LUX_NOT_CONF,
	RGB_LED_COLOR_YELLOW_LUX_PRE,
	RGB_LED_COLOR_YELLOW_LUX_BLINK_INVERTED,
	RGB_LED_COLOR_YELLOW_LUX_END,

	RGB_LED_COLOR_SPEED_NIGHT,
	RGB_LED_COLOR_SPEED_BLINK,

	RGB_LED_COLOR_TEST,
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
