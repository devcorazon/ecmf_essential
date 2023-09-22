/*
 * rgb_led.c
 *
 *  Created on: 30 juin 2023
 *      Author: youcef.benakmoume
 */

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "freertos/timers.h"

#include "ktd2027.h"
#include "rgb_led.h"

struct led_s {
    uint32_t		mode;
    uint32_t		pre_on_time;
    uint32_t		on_time;
    uint32_t		post_on_time;
    TimerHandle_t	pre_on_time_xTimer;
    TimerHandle_t	on_time_xTimer;
    TimerHandle_t	post_on_time_xTimer;
    bool			repeated;
};

static struct led_s led;

int rgb_led_mode(uint32_t mode, bool end);

static void rgb_led_set_params(uint32_t mode, bool repeated) {
	led.mode = mode;
	led.pre_on_time = 0;
	led.on_time = 0;
	led.post_on_time = 0;
	led.repeated = repeated;

	switch (mode) {
		case RGB_LED_MODE_NONE:
			ktd2027_led_set(RGB_LED_COLOR_NONE, RGB_LED_MODE_OFF);
			break;

		case RGB_LED_MODE_POWER_OFF:
			ktd2027_led_set(RGB_LED_COLOR_POWER_OFF, RGB_LED_MODE_ON);
			led.on_time = 2 * (128 * (FLASH_PERIOD_POWER_OFF + 2));
			break;

		case RGB_LED_MODE_IMMISSION:
			ktd2027_led_set(RGB_LED_COLOR_IMMISSION, RGB_LED_MODE_ON);
			led.on_time = 2048;
			break;

		case RGB_LED_MODE_EMISSION:
			ktd2027_led_set(RGB_LED_COLOR_EMISSION, RGB_LED_MODE_ON);
			led.on_time = 2048;
			break;

		case RGB_LED_MODE_FIXED_CYCLE:
			ktd2027_led_set(RGB_LED_COLOR_FIXED_CYCLE, RGB_LED_MODE_ON);
			led.on_time = 2048;
			break;

		case RGB_LED_MODE_AUTOMATIC_CYCLE:
			ktd2027_led_set(RGB_LED_COLOR_AUTOMATIC_CYCLE, RGB_LED_MODE_ON);
			led.on_time = 2 * (128 * (FLASH_PERIOD + 2));
			break;

		case RGB_LED_MODE_RH_NOT_CONF:
			ktd2027_led_set(RGB_LED_COLOR_AQUA_RH_NOT_CONF, RGB_LED_MODE_ON);
			led.on_time = 3048;
			break;
		case RGB_LED_MODE_RH_THR_LOW:
			ktd2027_led_set(RGB_LED_COLOR_AQUA_RH_PRE, RGB_LED_MODE_ON);
			led.pre_on_time = 128 * (FLASH_PERIOD + 2);
			led.on_time = 1 * (128 * (FLASH_PERIOD + 2));
			led.post_on_time = TOTAL_PERIOD - led.on_time;
			break;
		case RGB_LED_MODE_RH_THR_NORM:
			ktd2027_led_set(RGB_LED_COLOR_AQUA_RH_PRE, RGB_LED_MODE_ON);
			led.pre_on_time = 128 * (FLASH_PERIOD + 2);
			led.on_time = 2 * (128 * (FLASH_PERIOD + 2));
			led.post_on_time = TOTAL_PERIOD - led.on_time;
			break;
		case RGB_LED_MODE_RH_THR_HIGH:
			ktd2027_led_set(RGB_LED_COLOR_AQUA_RH_PRE, RGB_LED_MODE_ON);
			led.pre_on_time = 128 * (FLASH_PERIOD + 2);
			led.on_time = 3 * (128 * (FLASH_PERIOD + 2));
			led.post_on_time = TOTAL_PERIOD - led.on_time;
			break;

		case RGB_LED_MODE_VOC_NOT_CONF:
			ktd2027_led_set(RGB_LED_COLOR_GREEN_VOC_NOT_CONF, RGB_LED_MODE_ON);
			led.on_time = 3048;
			break;
		case RGB_LED_MODE_VOC_THR_LOW:
			ktd2027_led_set(RGB_LED_COLOR_GREEN_VOC_PRE, RGB_LED_MODE_ON);
			led.pre_on_time = 128 * (FLASH_PERIOD + 2);
			led.on_time = 1 * (128 * (FLASH_PERIOD + 2));
			led.post_on_time = TOTAL_PERIOD - led.on_time;
			break;
		case RGB_LED_MODE_VOC_THR_NORM:
			ktd2027_led_set(RGB_LED_COLOR_GREEN_VOC_PRE, RGB_LED_MODE_ON);
			led.pre_on_time = 128 * (FLASH_PERIOD + 2);
			led.on_time = 2 * (128 * (FLASH_PERIOD + 2));
			led.post_on_time = TOTAL_PERIOD - led.on_time;
			break;
		case RGB_LED_MODE_VOC_THR_HIGH:
			ktd2027_led_set(RGB_LED_COLOR_GREEN_VOC_PRE, RGB_LED_MODE_ON);
			led.pre_on_time = 128 * (FLASH_PERIOD + 2);
			led.on_time = 3 * (128 * (FLASH_PERIOD + 2));
			led.post_on_time = TOTAL_PERIOD - led.on_time;
			break;

		case RGB_LED_MODE_LUX_NOT_CONF:
			ktd2027_led_set(RGB_LED_COLOR_YELLOW_LUX_NOT_CONF, RGB_LED_MODE_ON);
			led.on_time = 3048;
			break;
		case RGB_LED_MODE_LUX_THR_LOW:
			ktd2027_led_set(RGB_LED_COLOR_YELLOW_LUX_PRE, RGB_LED_MODE_ON);
			led.pre_on_time = 128 * (FLASH_PERIOD + 2);
			led.on_time = 1 * (128 * (FLASH_PERIOD + 2));
			led.post_on_time = TOTAL_PERIOD - led.on_time;
			break;
		case RGB_LED_MODE_LUX_THR_NORM:
			ktd2027_led_set(RGB_LED_COLOR_YELLOW_LUX_PRE, RGB_LED_MODE_ON);
			led.pre_on_time = 128 * (FLASH_PERIOD + 2);
			led.on_time = 2 * (128 * (FLASH_PERIOD + 2));
			led.post_on_time = TOTAL_PERIOD - led.on_time;
			break;
		case RGB_LED_MODE_LUX_THR_HIGH:
			ktd2027_led_set(RGB_LED_COLOR_YELLOW_LUX_PRE, RGB_LED_MODE_ON);
			led.pre_on_time = 128 * (FLASH_PERIOD + 2);
			led.on_time = 3 * (128 * (FLASH_PERIOD + 2));
			led.post_on_time = TOTAL_PERIOD - led.on_time;
			break;

		case RGB_LED_MODE_SPEED_NIGHT:
			ktd2027_led_set(RGB_LED_COLOR_SPEED_NIGHT, RGB_LED_MODE_ON);
			led.on_time = 128 * (FLASH_PERIOD_SPEED_NIGHT + 2);
			break;
		case RGB_LED_MODE_SPEED_LOW:
			ktd2027_led_set(RGB_LED_COLOR_SPEED_BLINK, RGB_LED_MODE_ON);
			led.on_time = 1 * (128 * (FLASH_PERIOD_SPEED_BLINK + 2));
			break;
		case RGB_LED_MODE_SPEED_MEDIUM:
			ktd2027_led_set(RGB_LED_COLOR_SPEED_BLINK, RGB_LED_MODE_ON);
			led.on_time = 2 * (128 * (FLASH_PERIOD_SPEED_BLINK + 2));
			break;
		case RGB_LED_MODE_SPEED_HIGH:
			ktd2027_led_set(RGB_LED_COLOR_SPEED_BLINK, RGB_LED_MODE_ON);
			led.on_time = 3 * (128 * (FLASH_PERIOD_SPEED_BLINK + 2));
			break;

		case RGB_LED_MODE_TEST:
			ktd2027_led_set(RGB_LED_COLOR_SPEED_BLINK, RGB_LED_MODE_ON);
			break;
	}
}

static void rgb_led_pre_on_time_callback(TimerHandle_t xTimer) {
	switch (led.mode) {
		case RGB_LED_MODE_RH_NOT_CONF:
			ktd2027_led_set(RGB_LED_COLOR_AQUA_RH_NOT_CONF, RGB_LED_MODE_ON);
			break;
		case RGB_LED_MODE_RH_THR_LOW:
			ktd2027_led_set(RGB_LED_COLOR_AQUA_RH_BLINK_INVERTED, RGB_LED_MODE_ON);
			break;
		case RGB_LED_MODE_RH_THR_NORM:
			ktd2027_led_set(RGB_LED_COLOR_AQUA_RH_BLINK_INVERTED, RGB_LED_MODE_ON);
			break;
		case RGB_LED_MODE_RH_THR_HIGH:
			ktd2027_led_set(RGB_LED_COLOR_AQUA_RH_BLINK_INVERTED, RGB_LED_MODE_ON);
			break;

		case RGB_LED_MODE_VOC_NOT_CONF:
			ktd2027_led_set(RGB_LED_COLOR_GREEN_VOC_NOT_CONF, RGB_LED_MODE_ON);
			break;
		case RGB_LED_MODE_VOC_THR_LOW:
			ktd2027_led_set(RGB_LED_COLOR_GREEN_VOC_BLINK_INVERTED, RGB_LED_MODE_ON);
			break;
		case RGB_LED_MODE_VOC_THR_NORM:
			ktd2027_led_set(RGB_LED_COLOR_GREEN_VOC_BLINK_INVERTED, RGB_LED_MODE_ON);
			break;
		case RGB_LED_MODE_VOC_THR_HIGH:
			ktd2027_led_set(RGB_LED_COLOR_GREEN_VOC_BLINK_INVERTED, RGB_LED_MODE_ON);
			break;

		case RGB_LED_MODE_LUX_NOT_CONF:
			ktd2027_led_set(RGB_LED_COLOR_YELLOW_LUX_NOT_CONF, RGB_LED_MODE_ON);
			break;
		case RGB_LED_MODE_LUX_THR_LOW:
			ktd2027_led_set(RGB_LED_COLOR_YELLOW_LUX_BLINK_INVERTED, RGB_LED_MODE_ON);
			break;
		case RGB_LED_MODE_LUX_THR_NORM:
			ktd2027_led_set(RGB_LED_COLOR_YELLOW_LUX_BLINK_INVERTED, RGB_LED_MODE_ON);
			break;
		case RGB_LED_MODE_LUX_THR_HIGH:
			ktd2027_led_set(RGB_LED_COLOR_YELLOW_LUX_BLINK_INVERTED, RGB_LED_MODE_ON);
			break;

		default:
			ktd2027_led_set(RGB_LED_COLOR_NONE, RGB_LED_MODE_OFF);
			break;
	}

	if (led.on_time) {
		xTimerChangePeriod(led.on_time_xTimer, led.on_time / portTICK_PERIOD_MS, 0);
		xTimerStart(led.on_time_xTimer, 0);
	}
}


static void rgb_led_on_time_callback(TimerHandle_t xTimer) {
	switch (led.mode) {
		case RGB_LED_MODE_RH_NOT_CONF:
			ktd2027_led_set(RGB_LED_COLOR_AQUA_RH_NOT_CONF, RGB_LED_MODE_ON);
			break;
		case RGB_LED_MODE_RH_THR_LOW:
			ktd2027_led_set(RGB_LED_COLOR_AQUA_RH_END, RGB_LED_MODE_ON);
			break;
		case RGB_LED_MODE_RH_THR_NORM:
			ktd2027_led_set(RGB_LED_COLOR_AQUA_RH_END, RGB_LED_MODE_ON);
			break;
		case RGB_LED_MODE_RH_THR_HIGH:
			ktd2027_led_set(RGB_LED_COLOR_AQUA_RH_END, RGB_LED_MODE_ON);
			break;

		case RGB_LED_MODE_VOC_NOT_CONF:
			ktd2027_led_set(RGB_LED_COLOR_GREEN_VOC_NOT_CONF, RGB_LED_MODE_ON);
			break;
		case RGB_LED_MODE_VOC_THR_LOW:
			ktd2027_led_set(RGB_LED_COLOR_GREEN_VOC_END, RGB_LED_MODE_ON);
			break;
		case RGB_LED_MODE_VOC_THR_NORM:
			ktd2027_led_set(RGB_LED_COLOR_GREEN_VOC_END, RGB_LED_MODE_ON);
			break;
		case RGB_LED_MODE_VOC_THR_HIGH:
			ktd2027_led_set(RGB_LED_COLOR_GREEN_VOC_END, RGB_LED_MODE_ON);
			break;

		case RGB_LED_MODE_LUX_NOT_CONF:
			ktd2027_led_set(RGB_LED_COLOR_YELLOW_LUX_NOT_CONF, RGB_LED_MODE_ON);
			break;
		case RGB_LED_MODE_LUX_THR_LOW:
			ktd2027_led_set(RGB_LED_COLOR_YELLOW_LUX_END, RGB_LED_MODE_ON);
			break;
		case RGB_LED_MODE_LUX_THR_NORM:
			ktd2027_led_set(RGB_LED_COLOR_YELLOW_LUX_END, RGB_LED_MODE_ON);
			break;
		case RGB_LED_MODE_LUX_THR_HIGH:
			ktd2027_led_set(RGB_LED_COLOR_YELLOW_LUX_END, RGB_LED_MODE_ON);
			break;

		default:
			ktd2027_led_set(RGB_LED_COLOR_NONE, RGB_LED_MODE_OFF);
			break;
	}

	if (led.post_on_time) {
		xTimerChangePeriod(led.post_on_time_xTimer, led.post_on_time / portTICK_PERIOD_MS, 0);
		xTimerStart(led.post_on_time_xTimer, 0);
	}
	else {
		if (led.repeated) {
		}
		else {
			ktd2027_led_set(RGB_LED_BLANK, RGB_LED_MODE_OFF);

			led.mode = RGB_LED_MODE_NONE;
			led.pre_on_time = 0;
			led.on_time = 0;
			led.post_on_time = 0;
			led.repeated = false;
		}
	}
}

static void rgb_led_post_on_time_callback(TimerHandle_t xTimer) {
	if (led.repeated) {
		switch (led.mode) {
			case RGB_LED_MODE_RH_NOT_CONF:
				ktd2027_led_set(RGB_LED_COLOR_AQUA_RH_NOT_CONF, RGB_LED_MODE_ON);
				break;
			case RGB_LED_MODE_RH_THR_LOW:
				ktd2027_led_set(RGB_LED_COLOR_AQUA_RH_BLINK_INVERTED, RGB_LED_MODE_ON);
				break;
			case RGB_LED_MODE_RH_THR_NORM:
				ktd2027_led_set(RGB_LED_COLOR_AQUA_RH_BLINK_INVERTED, RGB_LED_MODE_ON);
				break;
			case RGB_LED_MODE_RH_THR_HIGH:
				ktd2027_led_set(RGB_LED_COLOR_AQUA_RH_BLINK_INVERTED, RGB_LED_MODE_ON);
				break;

			case RGB_LED_MODE_VOC_NOT_CONF:
				ktd2027_led_set(RGB_LED_COLOR_GREEN_VOC_NOT_CONF, RGB_LED_MODE_ON);
				break;
			case RGB_LED_MODE_VOC_THR_LOW:
				ktd2027_led_set(RGB_LED_COLOR_GREEN_VOC_BLINK_INVERTED, RGB_LED_MODE_ON);
				break;
			case RGB_LED_MODE_VOC_THR_NORM:
				ktd2027_led_set(RGB_LED_COLOR_GREEN_VOC_BLINK_INVERTED, RGB_LED_MODE_ON);
				break;
			case RGB_LED_MODE_VOC_THR_HIGH:
				ktd2027_led_set(RGB_LED_COLOR_GREEN_VOC_BLINK_INVERTED, RGB_LED_MODE_ON);
				break;

			case RGB_LED_MODE_LUX_NOT_CONF:
				ktd2027_led_set(RGB_LED_COLOR_YELLOW_LUX_NOT_CONF, RGB_LED_MODE_ON);
				break;
			case RGB_LED_MODE_LUX_THR_LOW:
				ktd2027_led_set(RGB_LED_COLOR_YELLOW_LUX_BLINK_INVERTED, RGB_LED_MODE_ON);
				break;
			case RGB_LED_MODE_LUX_THR_NORM:
				ktd2027_led_set(RGB_LED_COLOR_YELLOW_LUX_BLINK_INVERTED, RGB_LED_MODE_ON);
				break;
			case RGB_LED_MODE_LUX_THR_HIGH:
				ktd2027_led_set(RGB_LED_COLOR_YELLOW_LUX_BLINK_INVERTED, RGB_LED_MODE_ON);
				break;

			default:
				ktd2027_led_set(RGB_LED_BLANK, RGB_LED_MODE_OFF);
				break;
		}

		if (led.on_time) {
			xTimerChangePeriod(led.on_time_xTimer, led.on_time / portTICK_PERIOD_MS, 0);
			xTimerStart(led.on_time_xTimer, 0);
		}
	}
	else {
		ktd2027_led_set(RGB_LED_BLANK, RGB_LED_MODE_OFF);

		led.mode = RGB_LED_MODE_NONE;
		led.pre_on_time = 0;
		led.on_time = 0;
		led.post_on_time = 0;
		led.repeated = false;
	}
}

int rgb_led_init(struct i2c_dev_s *i2c_dev) {
    ktd2027_init(i2c_dev);

    led.mode = RGB_LED_MODE_NONE;
    led.pre_on_time = 0;
    led.on_time = 0;
    led.post_on_time = 0;
    led.pre_on_time_xTimer = xTimerCreate("rgb_led_pre_on_time_timer", 2000 / portTICK_PERIOD_MS, pdFALSE, (void *) 0, rgb_led_pre_on_time_callback);
    led.on_time_xTimer = xTimerCreate("rgb_led_on_time_timer", 2000 / portTICK_PERIOD_MS, pdFALSE, (void *) 0, rgb_led_on_time_callback);
    led.post_on_time_xTimer = xTimerCreate("rgb_led_post_on_time_timer", 2000 / portTICK_PERIOD_MS, pdFALSE, (void *) 0, rgb_led_post_on_time_callback);
    led.repeated = false;

    ktd2027_led_set(RGB_LED_BLANK, RGB_LED_MODE_OFF);

    return 0;
}

int rgb_led_set(uint8_t led_color, uint8_t led_mode) {
	return ktd2027_led_set(led_color, led_mode);
}

int rgb_led_mode(uint32_t mode, bool repeated) {
	if (led.mode == RGB_LED_MODE_NONE || led.mode != mode) {
		if (led.mode != RGB_LED_MODE_NONE) {
			ktd2027_led_set(RGB_LED_BLANK, RGB_LED_MODE_ON);
		}

		if (xTimerIsTimerActive(led.pre_on_time_xTimer) == pdTRUE) {
			xTimerStop(led.pre_on_time_xTimer, 0);
		}

		if (xTimerIsTimerActive(led.on_time_xTimer) == pdTRUE) {
			xTimerStop(led.on_time_xTimer, 0);
		}

		if (xTimerIsTimerActive(led.post_on_time_xTimer) == pdTRUE) {
			xTimerStop(led.on_time_xTimer, 0);
		}

		rgb_led_set_params(mode, repeated);

		if (led.pre_on_time) {
			xTimerChangePeriod(led.pre_on_time_xTimer, led.pre_on_time / portTICK_PERIOD_MS, 0);
			xTimerStart(led.pre_on_time_xTimer, 0);
		}
		else if (led.on_time) {
			xTimerChangePeriod(led.on_time_xTimer, led.on_time / portTICK_PERIOD_MS, 0);
			xTimerStart(led.on_time_xTimer, 0);
		}
	}

    return 0;
}
