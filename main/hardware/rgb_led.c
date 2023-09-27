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
    uint8_t			color;
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

int rgb_led_mode(uint8_t color, uint32_t mode, bool end);

static void rgb_led_set_params(uint8_t color, uint32_t mode, bool repeated) {
	led.color = color;
	led.mode = mode;
	led.pre_on_time = 0;
	led.on_time = 0;
	led.post_on_time = 0;
	led.repeated = repeated;

	switch(led.mode) {
		case RGB_LED_MODE_NONE:
			ktd2027_led_set(RGB_LED_COLOR_NONE, RGB_LED_MODE_OFF);
			break;

		case RGB_LED_MODE_ALWAYS_ON:
			switch(led.color) {
				case RGB_LED_COLOR_TEST:
					ktd2027_led_set(RGB_LED_COLOR_TEST, RGB_LED_MODE_ON);
					break;
			}
			break;

		case RGB_LED_MODE_ONESHOOT:
			led.on_time = 2048;
			switch(led.color) {
				case RGB_LED_COLOR_IMMISSION:
					ktd2027_led_set(RGB_LED_COLOR_IMMISSION, RGB_LED_MODE_ON);
					break;
				case RGB_LED_COLOR_EMISSION:
					ktd2027_led_set(RGB_LED_COLOR_EMISSION, RGB_LED_MODE_ON);
					break;
				case RGB_LED_COLOR_FIXED_CYCLE:
					ktd2027_led_set(RGB_LED_COLOR_FIXED_CYCLE, RGB_LED_MODE_ON);
					break;
				case RGB_LED_COLOR_AUTOMATIC_CYCLE:
					ktd2027_led_set(RGB_LED_COLOR_AUTOMATIC_CYCLE, RGB_LED_MODE_ON);
					break;
			}
			break;

		case RGB_LED_MODE_DOUBLE_BLINK:
			switch(led.color) {
				case RGB_LED_COLOR_POWER_ON:
					led.on_time = 2 * (128 * (FLASH_PERIOD_POWER_ON + 2));
					ktd2027_led_set(led.color, RGB_LED_MODE_BLINK_FAST);
					break;
				case RGB_LED_COLOR_POWER_OFF:
					led.on_time = 2 * (128 * (FLASH_PERIOD_POWER_OFF + 2));
					ktd2027_led_set(led.color, RGB_LED_MODE_BLINK_SLOW);
					break;
			}
			break;

		case RGB_LED_MODE_SPEED_NIGHT:
			led.on_time = 128 * (FLASH_PERIOD_SPEED_NIGHT + 2);
			ktd2027_led_set(led.color, RGB_LED_MODE_SPEED_NIGHT_FADE);
			break;
		case RGB_LED_MODE_SPEED_LOW:
			led.on_time = 1 * (128 * (FLASH_PERIOD_SPEED_BLINK + 2));
			ktd2027_led_set(led.color, RGB_LED_MODE_SPEED_BLINK);
			break;
		case RGB_LED_MODE_SPEED_MEDIUM:
			led.on_time = 2 * (128 * (FLASH_PERIOD_SPEED_BLINK + 2));
			ktd2027_led_set(led.color, RGB_LED_MODE_SPEED_BLINK);
			break;
		case RGB_LED_MODE_SPEED_HIGH:
			led.on_time = 3 * (128 * (FLASH_PERIOD_SPEED_BLINK + 2));
			ktd2027_led_set(led.color, RGB_LED_MODE_SPEED_BLINK);
			break;

		case RGB_LED_MODE_THR_NOT_CONF:
			led.on_time = 3048;
			ktd2027_led_set(led.color, RGB_LED_MODE_NOT_CONF);
			break;

		case RGB_LED_MODE_THR_LOW:
			led.pre_on_time = 128 * (PRE_PERIOD + 2);
			led.on_time = 1 * (128 * (FLASH_PERIOD + 2));
			led.post_on_time = 128 * (END_PERIOD + 2);
			ktd2027_led_set(led.color, RGB_LED_MODE_PRE);
			break;
		case RGB_LED_MODE_THR_MEDIUM:
			led.pre_on_time = 128 * (PRE_PERIOD + 2);
			led.on_time = 2 * (128 * (FLASH_PERIOD + 2));
			led.post_on_time = 128 * (END_PERIOD + 2);
			ktd2027_led_set(led.color, RGB_LED_MODE_PRE);
			break;
		case RGB_LED_MODE_THR_HIGH:
			led.pre_on_time = 128 * (PRE_PERIOD + 2);
			led.on_time = 3 * (128 * (FLASH_PERIOD + 2));
			led.post_on_time = 128 * (END_PERIOD + 2);
			ktd2027_led_set(led.color, RGB_LED_MODE_PRE);
			break;

		case RGB_LED_MODE_TEST:
			ktd2027_led_set(RGB_LED_COLOR_TEST, RGB_LED_MODE_ON);
			break;
	}
}

static void rgb_led_pre_on_time_callback(TimerHandle_t xTimer) {
	switch (led.mode) {
		case RGB_LED_MODE_THR_LOW:
		case RGB_LED_MODE_THR_MEDIUM:
		case RGB_LED_MODE_THR_HIGH:
			ktd2027_led_set(led.color, RGB_LED_MODE_BLINK_INVERTED);
			break;
	}

	if (led.on_time) {
		xTimerChangePeriod(led.on_time_xTimer, led.on_time / portTICK_PERIOD_MS, 0);
		xTimerStart(led.on_time_xTimer, 0);
	}
}


static void rgb_led_on_time_callback(TimerHandle_t xTimer) {
	switch (led.mode) {
		case RGB_LED_MODE_THR_NOT_CONF:
			break;

		case RGB_LED_MODE_THR_LOW:
		case RGB_LED_MODE_THR_MEDIUM:
		case RGB_LED_MODE_THR_HIGH:
			ktd2027_led_set(led.color, RGB_LED_MODE_END);
			break;

		default:
			ktd2027_led_set(RGB_LED_COLOR_NONE, RGB_LED_MODE_BLANK);
			break;
	}

	if (led.post_on_time) {
		xTimerChangePeriod(led.post_on_time_xTimer, led.post_on_time / portTICK_PERIOD_MS, 0);
		xTimerStart(led.post_on_time_xTimer, 0);
	}
	else if (led.repeated == false) {
		ktd2027_led_set(RGB_LED_COLOR_NONE, RGB_LED_MODE_OFF);

		led.color = RGB_LED_COLOR_NONE;
		led.mode = RGB_LED_MODE_NONE;
		led.pre_on_time = 0;
		led.on_time = 0;
		led.post_on_time = 0;
		led.repeated = false;
	}
}

static void rgb_led_post_on_time_callback(TimerHandle_t xTimer) {
	if (led.repeated) {
		switch (led.mode) {
			case RGB_LED_MODE_THR_LOW:
			case RGB_LED_MODE_THR_MEDIUM:
			case RGB_LED_MODE_THR_HIGH:
				ktd2027_led_set(led.color, RGB_LED_MODE_BLINK_INVERTED);
				break;
		}

		if (led.on_time) {
			xTimerChangePeriod(led.on_time_xTimer, led.on_time / portTICK_PERIOD_MS, 0);
			xTimerStart(led.on_time_xTimer, 0);
		}
	}
	else {
		ktd2027_led_set(RGB_LED_COLOR_NONE, RGB_LED_MODE_OFF);

	    led.color = RGB_LED_COLOR_NONE;
		led.mode = RGB_LED_MODE_NONE;
		led.pre_on_time = 0;
		led.on_time = 0;
		led.post_on_time = 0;
		led.repeated = false;
	}
}

int rgb_led_init(struct i2c_dev_s *i2c_dev) {
    ktd2027_init(i2c_dev);

    led.color = RGB_LED_COLOR_NONE;
    led.mode = RGB_LED_MODE_NONE;
    led.pre_on_time = 0;
    led.on_time = 0;
    led.post_on_time = 0;
    led.pre_on_time_xTimer = xTimerCreate("rgb_led_pre_on_time_timer", 2000 / portTICK_PERIOD_MS, pdFALSE, (void *) 0, rgb_led_pre_on_time_callback);
    led.on_time_xTimer = xTimerCreate("rgb_led_on_time_timer", 2000 / portTICK_PERIOD_MS, pdFALSE, (void *) 0, rgb_led_on_time_callback);
    led.post_on_time_xTimer = xTimerCreate("rgb_led_post_on_time_timer", 2000 / portTICK_PERIOD_MS, pdFALSE, (void *) 0, rgb_led_post_on_time_callback);
    led.repeated = false;

    ktd2027_led_set(RGB_LED_COLOR_NONE, RGB_LED_MODE_OFF);

    return 0;
}

int rgb_led_set(uint8_t led_color, uint8_t led_mode) {
	return ktd2027_led_set(led_color, led_mode == RGB_LED_MODE_NONE ? RGB_LED_MODE_OFF : RGB_LED_MODE_ON);
}

int rgb_led_mode(uint8_t color, uint32_t mode, bool repeated) {
	if ((led.mode == RGB_LED_MODE_NONE && led.color == RGB_LED_COLOR_NONE) || led.mode != mode || led.color != color) {
		if (led.mode != RGB_LED_MODE_NONE || led.color != RGB_LED_COLOR_NONE) {
			ktd2027_led_set(RGB_LED_COLOR_NONE, RGB_LED_MODE_BLANK);

			xTimerStop(led.pre_on_time_xTimer, 0);
			xTimerStop(led.on_time_xTimer, 0);
			xTimerStop(led.on_time_xTimer, 0);
		}

//		if (xTimerIsTimerActive(led.pre_on_time_xTimer) == pdTRUE) {
//			xTimerStop(led.pre_on_time_xTimer, 0);
//		}
//
//		if (xTimerIsTimerActive(led.on_time_xTimer) == pdTRUE) {
//			xTimerStop(led.on_time_xTimer, 0);
//		}
//
//		if (xTimerIsTimerActive(led.post_on_time_xTimer) == pdTRUE) {
//			xTimerStop(led.on_time_xTimer, 0);
//		}

		rgb_led_set_params(color, mode, repeated);

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
