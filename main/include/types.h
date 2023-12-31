/*
 * types.h
 *
 *  Created on: 1 dic 2022
 *      Author: Daniele Schirosi
 */

#ifndef SRC_INCLUDE_TYPES_H_
#define SRC_INCLUDE_TYPES_H_

#include "blufi_internal.h"

#define TEMPERATURE_INVALID						INT16_MAX
#define RELATIVE_HUMIDITY_INVALID				UINT16_MAX
#define VOC_INVALID								UINT16_MAX
#define LUX_INVALID								UINT16_MAX

#warning //Check scale
#define TEMPERATURE_SCALE						(100)
#define RELATIVE_HUMIDITY_SCALE					(100u)
#define VOC_SCALE								(1u)
#define LUX_SCALE						        (1u)

// Offset
#define TEMPERATURE_OFFSET_FIXED				(0)
#define RELATIVE_HUMIDITY_OFFSET_FIXED			(0)

#define OFFSET_BOUND_MIN						(-5 * TEMPERATURE_SCALE)
#define OFFSET_BOUND_MAX						(5 * TEMPERATURE_SCALE)

#define SET_VALUE_TO_TEMP_RAW(val)				(val == TEMP_F_INVALID ? TEMPERATURE_INVALID : (int16_t) (val * TEMPERATURE_SCALE))
#define SET_VALUE_TO_RH_RAW(val)				(val == HUM_F_INVALID ? RELATIVE_HUMIDITY_INVALID : (uint16_t) (val * RELATIVE_HUMIDITY_SCALE))
#define SET_VALUE_TO_VOC_RAW(val)				(val == GAS_U_INVALID ? VOC_INVALID : (uint16_t) (val * VOC_SCALE))
#define SET_VALUE_TO_LUX_RAW(val)				(val == LUX_F_INVALID ? LUX_INVALID : (uint16_t) (val * LUX_SCALE))

#define TEMP_RAW_TO_INT(t)						(int16_t) (t / TEMPERATURE_SCALE)
#define TEMP_RAW_TO_DEC(t)						(uint16_t) (abs((int) t) % TEMPERATURE_SCALE)

#define RH_RAW_TO_INT(rh)						(uint16_t) (rh / RELATIVE_HUMIDITY_SCALE)
#define RH_RAW_TO_DEC(rh)						(uint16_t) (rh % RELATIVE_HUMIDITY_SCALE)

#define LUX_RAW_TO_INT(lux)						(uint16_t) (lux / LUX_SCALE)
#define LUX_RAW_TO_DEC(lux)						(uint16_t) (abs((int) lux ) % LUX_SCALE)

#define OFFSET_TEMP_RAW_TO_INT(t)			    (int16_t) (t / TEMPERATURE_SCALE)
#define OFFSET_TEMP_RAW_TO_DEC(t)			    (uint16_t) (abs((int) t) % TEMPERATURE_SCALE)

#define OFFSET_RH_RAW_TO_INT(rh)				(int16_t) (rh / (int16_t) RELATIVE_HUMIDITY_SCALE)
#define OFFSET_RH_RAW_TO_DEC(rh)				(uint16_t) (abs((int) rh) % RELATIVE_HUMIDITY_SCALE)

#define SECONDS_PER_HOUR						3600u
#define DAYS_PER_WEEK                           7u
#define HOURS_PER_DAY                           24u
#define QUARTERS_HOUR_PER_DAY                   4*HOURS_PER_DAY

///  Timing
#define DURATION_IMMISSION_EMISSION				(1U * SECONDS_PER_HOUR)
#define DURATION_FIXED_CYCLE					(45U)
#define DURATION_AUTOMATIC_CYCLE_OUT			(45U)
#define DURATION_AUTOMATIC_CYCLE_IN				(150U)
#define DURATION_RESTART_AUTOMATIC_CYCLE		(10U * SECONDS_PER_HOUR)
#define DURATION_EXTRA_CYCLE_BOOST				(200U)
#define DURATION_RESTART_EXTRA_CYCLE			(1U * SECONDS_PER_HOUR)

#define ceiling_fraction(numerator, divider) (((numerator) + ((divider) - 1)) / (divider))
#define SECONDS_TO_MS(seconds) ((seconds) * 1000UL)

/// Thresholds and differentials
#define LUMINOSITY_THRESHOLD_LOW				(5U * LUX_SCALE)
#define LUMINOSITY_THRESHOLD_MEDIUM				(10U * LUX_SCALE)
#define LUMINOSITY_THRESHOLD_HIGH				(15U * LUX_SCALE)

#define LUMINOSITY_DIFFERENTIAL_LOW				(0U * LUX_SCALE)
#define LUMINOSITY_DIFFERENTIAL_HIGH			(5U * LUX_SCALE)

#define RH_THRESHOLD_LOW						(55U * RELATIVE_HUMIDITY_SCALE)
#define RH_THRESHOLD_MEDIUM						(60U * RELATIVE_HUMIDITY_SCALE)
#define RH_THRESHOLD_HIGH						(65U * RELATIVE_HUMIDITY_SCALE)

#define RH_DIFFERENTIAL_LOW						(5U * RELATIVE_HUMIDITY_SCALE)
#define RH_DIFFERENTIAL_HIGH					(0U * RELATIVE_HUMIDITY_SCALE)

#define RH_THRESHOLD_ADV_CTRL_PERCENTAGE		(90U)

#define VOC_THRESHOLD_LOW						(150U * VOC_SCALE)
#define VOC_THRESHOLD_MEDIUM					(200U * VOC_SCALE)
#define VOC_THRESHOLD_HIGH						(250U * VOC_SCALE)

#define VOC_DIFFERENTIAL_LOW					(10U * VOC_SCALE)
#define VOC_DIFFERENTIAL_HIGH					(0U * VOC_SCALE)

// Flags
#define COND_LUMINOSITY							BIT(0)
#define COND_RH_EXTRA_CYCLE						BIT(1)
#define COND_VOC_EXTRA_CYCLE					BIT(2)

// Offset
#define TEMPERATURE_OFFSET_FIXED				(0)
#define RELATIVE_HUMIDITY_OFFSET_FIXED			(0)

// Filters
#define FILTER_THRESHOLD						(1000u)

#define THRESHOLD_FILTER_WARNING				BIT(0)

#define VALUE_UNMODIFIED                         0x7F
#define VALUE_UNMODIFIED_LONG                    0x7FFF

// Filter coefficients
#define COEFF_NIGHT  							(200u)
#define COEFF_LOW								(600u)
#define COEFF_MEDIUM							(1000u)
#define COEFF_HIGH								(1400u)
#define COEFF_BOOST 							(1500u)

#define COEFF_SCALE								(1000u)

#define RGB_LED_MODE_FOREVER					(UINT8_MAX)

// WIFI Activation data
#define WIFI_UNLOCKED_VALUE                     0xAA

// Enumerations

// Rh threshold setting
enum {
    RH_THRESHOLD_SETTING_NOT_CONFIGURED			= 0x00,
    RH_THRESHOLD_SETTING_LOW					= 0x01,
    RH_THRESHOLD_SETTING_MEDIUM					= 0x02,
    RH_THRESHOLD_SETTING_HIGH					= 0x03,
};

// Luminosity sensor setting
enum {
    LUX_THRESHOLD_SETTING_NOT_CONFIGURED		= 0x00,
	LUX_THRESHOLD_SETTING_LOW					= 0x01,
	LUX_THRESHOLD_SETTING_MEDIUM				= 0x02,
	LUX_THRESHOLD_SETTING_HIGH					= 0x03,
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

	RGB_LED_COLOR_POWER_ON,
	RGB_LED_COLOR_POWER_OFF,

	RGB_LED_COLOR_IMMISSION,
	RGB_LED_COLOR_EMISSION,
	RGB_LED_COLOR_FIXED_CYCLE,
	RGB_LED_COLOR_AUTOMATIC_CYCLE,

	RGB_LED_COLOR_CONNECTIVTY,
	RGB_LED_COLOR_FILTER_WARNING,
	RGB_LED_COLOR_CLEAR_WARNING,

	RGB_LED_COLOR_TEST,

	RGB_LED_BLANK,
};

enum rgb_led_mode {
	RGB_LED_MODE_NONE = 0,

	RGB_LED_MODE_ALWAYS_ON,
	RGB_LED_MODE_ONESHOOT,
	RGB_LED_MODE_SINGLE_BLINK,
	RGB_LED_MODE_DOUBLE_BLINK,

	RGB_LED_MODE_SPEED_NIGHT,
	RGB_LED_MODE_SPEED_LOW,
	RGB_LED_MODE_SPEED_MEDIUM,
	RGB_LED_MODE_SPEED_HIGH,

	RGB_LED_MODE_THR_NOT_CONF,
	RGB_LED_MODE_THR_LOW,
	RGB_LED_MODE_THR_MEDIUM,
	RGB_LED_MODE_THR_HIGH,

	RGB_LED_MODE_TEST,
};

#endif /* SRC_INCLUDE_TYPES_H_ */
