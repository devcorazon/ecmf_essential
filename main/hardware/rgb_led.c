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

#define RGB_LED_ADJUST_TIMER_MS			2

struct led_s {
    uint8_t			color;
    uint32_t		mode;
    uint32_t		pre_on_time;
    uint32_t		on_time;
    uint32_t		post_on_time;
    TimerHandle_t	pre_on_time_xTimer;
    TimerHandle_t	on_time_xTimer;
    TimerHandle_t	post_on_time_xTimer;
    uint8_t			repeated;
};

static void rgb_led_pre_on_time_callback(TimerHandle_t xTimer);
static void rgb_led_on_time_callback(TimerHandle_t xTimer);
static void rgb_led_post_on_time_callback(TimerHandle_t xTimer);

static struct led_s led;

static void rgb_led_set_params(uint8_t color, uint32_t mode, bool repeated) {
	led.color = color;
	led.mode = mode;
	led.pre_on_time = 0;
	led.on_time = 0;
	led.post_on_time = 0;
	led.repeated = repeated ? RGB_LED_MODE_FOREVER : 0;

	switch(led.mode) {
		case RGB_LED_MODE_NONE:
			ktd2027_led_set(RGB_LED_COLOR_NONE, RGB_LED_MODE_OFF);
			break;

		case RGB_LED_MODE_ALWAYS_ON:
			switch(led.color) {
				case RGB_LED_COLOR_GREEN:
					ktd2027_led_set(RGB_LED_COLOR_GREEN, RGB_LED_MODE_BLINK);
					break;
				case RGB_LED_COLOR_AUTOMATIC_CYCLE:
					ktd2027_led_set(RGB_LED_COLOR_AUTOMATIC_CYCLE, RGB_LED_MODE_BLINK);
					break;
				case RGB_LED_COLOR_TEST:
					ktd2027_led_set(RGB_LED_COLOR_TEST, RGB_LED_MODE_ON);
					break;
			}
			break;

		case RGB_LED_MODE_ONESHOOT:
			switch(led.color) {
				case RGB_LED_COLOR_RED:
					led.on_time = 512;
					ktd2027_led_set(RGB_LED_COLOR_RED, RGB_LED_MODE_ON);
					break;
				case RGB_LED_COLOR_AQUA_RH:
					led.on_time = (128 * (THR_CONFIRM_PERIOD + 2)) + 512;
					ktd2027_led_set(RGB_LED_COLOR_AQUA_RH, RGB_LED_MODE_ON);
					break;
				case RGB_LED_COLOR_GREEN_VOC:
					led.on_time = (128 * (THR_CONFIRM_PERIOD + 2)) + 512;
					ktd2027_led_set(RGB_LED_COLOR_GREEN_VOC, RGB_LED_MODE_ON);
					break;
				case RGB_LED_COLOR_YELLOW_LUX:
					led.on_time = (128 * (THR_CONFIRM_PERIOD + 2)) + 512;
					ktd2027_led_set(RGB_LED_COLOR_YELLOW_LUX, RGB_LED_MODE_ON);
					break;
				case RGB_LED_COLOR_IMMISSION:
//					led.on_time = 128 * (10 + 2);
					led.on_time = 1024;
					ktd2027_led_set(RGB_LED_COLOR_IMMISSION, RGB_LED_MODE_ON);
					break;
				case RGB_LED_COLOR_EMISSION:
//					led.on_time = 128 * (10 + 2);
					led.on_time = 1024;
					ktd2027_led_set(RGB_LED_COLOR_EMISSION, RGB_LED_MODE_ON);
					break;
				case RGB_LED_COLOR_FIXED_CYCLE:
					led.on_time = 1024;
					ktd2027_led_set(RGB_LED_COLOR_FIXED_CYCLE, RGB_LED_MODE_ON);
					break;
				case RGB_LED_COLOR_AUTOMATIC_CYCLE:
//					led.on_time = 1024;
//					ktd2027_led_set(RGB_LED_COLOR_AUTOMATIC_CYCLE, RGB_LED_MODE_ON);
					led.on_time = 1 * (128 * (FLASH_PERIOD_AUTOMATIC_CYCLE_BLINK + 2));
					ktd2027_led_set(RGB_LED_COLOR_AUTOMATIC_CYCLE, RGB_LED_MODE_BLINK);
					break;
				case RGB_LED_COLOR_WHITE:
					led.on_time = 1024;
					ktd2027_led_set(RGB_LED_COLOR_WHITE, RGB_LED_MODE_ON);
					break;
			}
			break;

		case RGB_LED_MODE_SINGLE_BLINK:
//			led.pre_on_time = 1024;
			led.on_time = 1024;
			led.post_on_time = 1024;
			ktd2027_led_set(RGB_LED_COLOR_EMISSION, RGB_LED_MODE_ON);
			break;

		case RGB_LED_MODE_DOUBLE_BLINK:
			switch(led.color) {
				case RGB_LED_COLOR_POWER_ON:
					led.on_time = 4 * (128 * (FLASH_PERIOD_POWER_ON + 2));
					ktd2027_led_set(led.color, RGB_LED_MODE_BLINK);
					break;
				case RGB_LED_COLOR_POWER_OFF:
					led.on_time = 4 * (128 * (FLASH_PERIOD_POWER_OFF + 2));
					ktd2027_led_set(led.color, RGB_LED_MODE_BLINK);
					break;
				case RGB_LED_COLOR_CONNECTIVTY:
					led.on_time = 4 * (128 * (FLASH_PERIOD_CONNECTIVITY + 2));
					ktd2027_led_set(RGB_LED_COLOR_CONNECTIVTY, RGB_LED_MODE_BLINK);
					break;
				case RGB_LED_COLOR_WHITE:
					led.on_time = 2 * (128 * (FLASH_PERIOD_CLEAR_WARNING + 2));
					ktd2027_led_set(RGB_LED_COLOR_WHITE, RGB_LED_MODE_BLINK);
					break;
			}
			break;

		///
		case RGB_LED_MODE_SPEED_NIGHT:
//			led.on_time = 128 * (FLASH_PERIOD_SPEED_NIGHT + 2);
//			ktd2027_led_set(led.color, RGB_LED_MODE_SPEED_NIGHT_FADE);
			switch(led.color) {
				case RGB_LED_COLOR_IMMISSION:
				case RGB_LED_COLOR_EMISSION:
				case RGB_LED_COLOR_FIXED_CYCLE:
					led.on_time = 128 * (FLASH_PERIOD_SPEED_NIGHT + 2);
					ktd2027_led_set(led.color, RGB_LED_MODE_SPEED_NIGHT_FADE);
					break;
				case RGB_LED_COLOR_AUTOMATIC_CYCLE:
					led.pre_on_time = (128 * (FLASH_PERIOD_SPEED_NIGHT + 2) / 2);
					led.on_time = (128 * (FLASH_PERIOD_SPEED_NIGHT + 2) / 2);
					ktd2027_led_set(RGB_LED_COLOR_EMISSION, RGB_LED_MODE_SPEED_NIGHT_FADE);
					break;
			}
			break;
		case RGB_LED_MODE_SPEED_LOW:
//			led.on_time = 1 * (128 * (FLASH_PERIOD_SPEED_BLINK + 2));
//			ktd2027_led_set(led.color, RGB_LED_MODE_SPEED_BLINK);
			switch(led.color) {
				case RGB_LED_COLOR_IMMISSION:
				case RGB_LED_COLOR_EMISSION:
				case RGB_LED_COLOR_FIXED_CYCLE:
					led.on_time = 1 * (128 * (FLASH_PERIOD_SPEED_BLINK + 2));
					ktd2027_led_set(led.color, RGB_LED_MODE_SPEED_BLINK);
					break;
				case RGB_LED_COLOR_AUTOMATIC_CYCLE:
//					led.on_time = 1 * (128 * (FLASH_PERIOD_SPEED_BLINK_AUTO + 2));
//					ktd2027_led_set(led.color, RGB_LED_MODE_SPEED_BLINK);
					led.pre_on_time = PRE_ON_PERIOD_SPEED_AUTO_MS;
					led.on_time = PRE_ON_PERIOD_SPEED_AUTO_MS;
					led.post_on_time = POST_PERIOD_SPEED_AUTO_MS;
					ktd2027_led_set(RGB_LED_COLOR_EMISSION, RGB_LED_MODE_ON);
					break;
			}
			break;
		case RGB_LED_MODE_SPEED_MEDIUM:
//			led.on_time = 2 * (128 * (FLASH_PERIOD_SPEED_BLINK + 2));
//			ktd2027_led_set(led.color, RGB_LED_MODE_SPEED_BLINK);
			switch(led.color) {
				case RGB_LED_COLOR_IMMISSION:
				case RGB_LED_COLOR_EMISSION:
				case RGB_LED_COLOR_FIXED_CYCLE:
					led.on_time = 2 * (128 * (FLASH_PERIOD_SPEED_BLINK + 2));
					ktd2027_led_set(led.color, RGB_LED_MODE_SPEED_BLINK);
					break;
				case RGB_LED_COLOR_AUTOMATIC_CYCLE:
//					led.on_time = 2 * (128 * (FLASH_PERIOD_SPEED_BLINK_AUTO + 2));
//					ktd2027_led_set(led.color, RGB_LED_MODE_SPEED_BLINK);
					led.pre_on_time = PRE_ON_PERIOD_SPEED_AUTO_MS;
					led.on_time = PRE_ON_PERIOD_SPEED_AUTO_MS;
					led.post_on_time = POST_PERIOD_SPEED_AUTO_MS;
					led.repeated = 1;
					ktd2027_led_set(RGB_LED_COLOR_EMISSION, RGB_LED_MODE_ON);
					break;
			}
			break;
		case RGB_LED_MODE_SPEED_HIGH:
//			led.on_time = 3 * (128 * (FLASH_PERIOD_SPEED_BLINK + 2));
//			ktd2027_led_set(led.color, RGB_LED_MODE_SPEED_BLINK);
			switch(led.color) {
				case RGB_LED_COLOR_IMMISSION:
				case RGB_LED_COLOR_EMISSION:
				case RGB_LED_COLOR_FIXED_CYCLE:
					led.on_time = 3 * (128 * (FLASH_PERIOD_SPEED_BLINK + 2));
					ktd2027_led_set(led.color, RGB_LED_MODE_SPEED_BLINK);
					break;
				case RGB_LED_COLOR_AUTOMATIC_CYCLE:
//					led.on_time = 3 * (128 * (FLASH_PERIOD_SPEED_BLINK_AUTO + 2));
//					ktd2027_led_set(led.color, RGB_LED_MODE_SPEED_BLINK);
					led.pre_on_time = PRE_ON_PERIOD_SPEED_AUTO_MS;
					led.on_time = PRE_ON_PERIOD_SPEED_AUTO_MS;
					led.post_on_time = POST_PERIOD_SPEED_AUTO_MS;
					led.repeated = 2;
					ktd2027_led_set(RGB_LED_COLOR_EMISSION, RGB_LED_MODE_ON);
					break;
			}
			break;

			///
		case RGB_LED_MODE_THR_NOT_CONF:
			led.on_time = THR_NOT_CONF_PERIOD_MS;
			if (led.repeated) {
				led.post_on_time = POST_FLASH_PERIOD_MS;
			}
			ktd2027_led_set(led.color, RGB_LED_MODE_NOT_CONF);
			break;
		case RGB_LED_MODE_THR_LOW:
//			led.pre_on_time = PRE_FLASH_PERIOD_MS;
			led.on_time = 1 * (128 * (FLASH_PERIOD + 2));
			if (led.repeated) {
				led.post_on_time = POST_FLASH_PERIOD_MS;
			}
//			ktd2027_led_set(led.color, RGB_LED_MODE_PRE);
			ktd2027_led_set(led.color, RGB_LED_MODE_BLINK_INVERTED);
			break;
		case RGB_LED_MODE_THR_MEDIUM:
//			led.pre_on_time = PRE_FLASH_PERIOD_MS;
			led.on_time = 2 * (128 * (FLASH_PERIOD + 2));
			if (led.repeated) {
				led.post_on_time = POST_FLASH_PERIOD_MS;
			}
//			ktd2027_led_set(led.color, RGB_LED_MODE_PRE);
			ktd2027_led_set(led.color, RGB_LED_MODE_BLINK_INVERTED);
			break;
		case RGB_LED_MODE_THR_HIGH:
//			led.pre_on_time = PRE_FLASH_PERIOD_MS;
			led.on_time = 3 * (128 * (FLASH_PERIOD + 2));
			if (led.repeated) {
				led.post_on_time = POST_FLASH_PERIOD_MS;
			}
//			ktd2027_led_set(led.color, RGB_LED_MODE_PRE);
			ktd2027_led_set(led.color, RGB_LED_MODE_BLINK_INVERTED);
			break;

		case RGB_LED_MODE_TEST:
			ktd2027_led_set(RGB_LED_COLOR_TEST, RGB_LED_MODE_ON);
			break;
	}

	/// Adjust
//	if (led.pre_on_time)
//		led.pre_on_time -= RGB_LED_ADJUST_TIMER_MS;
//	if (led.on_time)
//		led.on_time -= RGB_LED_ADJUST_TIMER_MS;
//	if (led.post_on_time)
//		led.post_on_time -= RGB_LED_ADJUST_TIMER_MS;
}

static void rgb_led_pre_on_time_callback(TimerHandle_t xTimer) {
	switch (led.mode) {
		case RGB_LED_MODE_SPEED_NIGHT:
			ktd2027_led_set(RGB_LED_COLOR_IMMISSION, RGB_LED_MODE_SPEED_NIGHT_FADE);
			break;
		case RGB_LED_MODE_SPEED_LOW:
		case RGB_LED_MODE_SPEED_MEDIUM:
		case RGB_LED_MODE_SPEED_HIGH:
			ktd2027_led_set(RGB_LED_COLOR_IMMISSION, RGB_LED_MODE_ON);
			break;
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
		case RGB_LED_MODE_SPEED_NIGHT:
		case RGB_LED_MODE_SPEED_LOW:
		case RGB_LED_MODE_SPEED_MEDIUM:
		case RGB_LED_MODE_SPEED_HIGH:
			ktd2027_led_set(RGB_LED_COLOR_NONE, RGB_LED_MODE_OFF);
			break;
		case RGB_LED_MODE_THR_NOT_CONF:
		case RGB_LED_MODE_THR_LOW:
		case RGB_LED_MODE_THR_MEDIUM:
		case RGB_LED_MODE_THR_HIGH:
			ktd2027_led_set(led.color, RGB_LED_MODE_POST);
			break;
		case RGB_LED_MODE_SINGLE_BLINK:
			ktd2027_led_set(RGB_LED_COLOR_IMMISSION, RGB_LED_MODE_ON);
			break;
	}

	if (led.post_on_time) {
		xTimerChangePeriod(led.post_on_time_xTimer, led.post_on_time / portTICK_PERIOD_MS, 0);
		xTimerStart(led.post_on_time_xTimer, 0);
	}
	else if (led.repeated == 0) {
		ktd2027_led_set(RGB_LED_COLOR_NONE, RGB_LED_MODE_OFF);

		led.color = RGB_LED_COLOR_NONE;
		led.mode = RGB_LED_MODE_NONE;
		led.pre_on_time = 0;
		led.on_time = 0;
		led.post_on_time = 0;
		led.repeated = 0;
	}
}

static void rgb_led_post_on_time_callback(TimerHandle_t xTimer) {
	if (led.repeated) {
		if (led.repeated != RGB_LED_MODE_FOREVER) {
			led.repeated--;
		}

		switch (led.mode) {
			case RGB_LED_MODE_SPEED_LOW:
			case RGB_LED_MODE_SPEED_MEDIUM:
			case RGB_LED_MODE_SPEED_HIGH:
				ktd2027_led_set(RGB_LED_COLOR_EMISSION, RGB_LED_MODE_ON);
				if (led.pre_on_time) {
					xTimerChangePeriod(led.pre_on_time_xTimer, led.pre_on_time / portTICK_PERIOD_MS, 0);
					xTimerStart(led.pre_on_time_xTimer, 0);
				}
				break;
			case RGB_LED_MODE_THR_NOT_CONF:
				ktd2027_led_set(led.color, RGB_LED_MODE_NOT_CONF);
				if (led.on_time) {
					xTimerChangePeriod(led.on_time_xTimer, led.on_time / portTICK_PERIOD_MS, 0);
					xTimerStart(led.on_time_xTimer, 0);
				}
				break;
			case RGB_LED_MODE_THR_LOW:
			case RGB_LED_MODE_THR_MEDIUM:
			case RGB_LED_MODE_THR_HIGH:
				ktd2027_led_set(led.color, RGB_LED_MODE_BLINK_INVERTED);
				if (led.on_time) {
					xTimerChangePeriod(led.on_time_xTimer, led.on_time / portTICK_PERIOD_MS, 0);
					xTimerStart(led.on_time_xTimer, 0);
				}
				break;
		}
	}
	else {
		ktd2027_led_set(RGB_LED_COLOR_NONE, RGB_LED_MODE_OFF);

	    led.color = RGB_LED_COLOR_NONE;
		led.mode = RGB_LED_MODE_NONE;
		led.pre_on_time = 0;
		led.on_time = 0;
		led.post_on_time = 0;
		led.repeated = 0;
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
    led.repeated = 0;

    ktd2027_led_set(RGB_LED_COLOR_NONE, RGB_LED_MODE_OFF);

    return 0;
}

int rgb_led_set(uint8_t led_color, uint8_t led_mode) {
	return ktd2027_led_set(led_color, led_mode == RGB_LED_MODE_NONE ? RGB_LED_MODE_OFF : RGB_LED_MODE_ON);
}

int rgb_led_mode(uint8_t color, uint32_t mode, bool repeated) {
	if ((led.mode == RGB_LED_MODE_NONE && led.color == RGB_LED_COLOR_NONE) || led.mode != mode || led.color != color || led.repeated != repeated) {
		if (led.mode != RGB_LED_MODE_NONE || led.color != RGB_LED_COLOR_NONE) {
			if (led.color != color) {
				ktd2027_led_set(RGB_LED_COLOR_NONE, RGB_LED_MODE_OFF);
			}

			xTimerStop(led.pre_on_time_xTimer, 0);
			xTimerStop(led.on_time_xTimer, 0);
			xTimerStop(led.post_on_time_xTimer, 0);
		}

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

bool rgb_led_is_on(void) {
	return (led.mode != RGB_LED_MODE_NONE || led.color != RGB_LED_COLOR_NONE);
}
