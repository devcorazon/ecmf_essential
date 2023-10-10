/*
 * user_experience.c
 *
 *  Created on: 10 oct. 2023
 *      Author: youcef.benakmoume
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"

#include "storage.h"
#include "ir_receiver.h"
#include "test.h"
#include "rgb_led.h"


#define	USER_EXPERIENCE_TASK_STACK_SIZE			    (configMINIMAL_STACK_SIZE * 4)
#define	USER_EXPERIENCE_TASK_PRIORITY			    (1)
#define	USER_EXPERIENCE_TASK_PERIOD				    (100ul / portTICK_PERIOD_MS)

static void system_mode_speed_set(uint8_t mode, uint8_t speed);

static void user_experience_task(void *pvParameters);

static void user_experience_state_machine(void);

enum ux_s {
	OPERATIVE		= 0,
	RH_SETTING,
	VOC_SETTING,
	LUX_SETTING,
	SETTINGS,
};

static uint8_t user_experience_state = OPERATIVE;
static uint8_t user_experience_type = 0;

static void user_experience_task(void *pvParameters) {
	TickType_t user_experience_task_time;

	user_experience_task_time = xTaskGetTickCount();
	while (1) {

		if (test_in_progress() == false) {
           user_experience_state_machine();
		}

		vTaskDelayUntil(&user_experience_task_time, USER_EXPERIENCE_TASK_PERIOD);
	}
}

static void user_experience_state_machine(void) {
	uint32_t button = ir_receiver_take_button();

	if (user_experience_type == 0) {
		switch (user_experience_state) {
		case OPERATIVE:
			switch (button) {
			case BUTTON_1:
//				rgb_led_mode(RGB_LED_COLOR_TEST, RGB_LED_MODE_ALWAYS_ON, false);
				break;
			case BUTTON_1_LONG:
				user_experience_state = SETTINGS;
				rgb_led_mode(RGB_LED_COLOR_GREEN, RGB_LED_MODE_ALWAYS_ON, false);
				break;
			case BUTTON_4:
				if (get_mode_set() != MODE_OFF) {
					rgb_led_mode(RGB_LED_COLOR_OFFSET + get_mode_set(), RGB_LED_MODE_OFFSET + get_speed_set(), false);
				} else {
					rgb_led_mode(RGB_LED_COLOR_POWER_OFF, RGB_LED_MODE_DOUBLE_BLINK, false);
				}
				break;
			case BUTTON_3:
				system_mode_speed_set(MODE_IMMISSION, VALUE_UNMODIFIED);
				rgb_led_mode(RGB_LED_COLOR_IMMISSION, RGB_LED_MODE_ONESHOOT, false);
				break;
			case BUTTON_5:
				system_mode_speed_set(MODE_EMISSION, VALUE_UNMODIFIED);
				rgb_led_mode(RGB_LED_COLOR_EMISSION, RGB_LED_MODE_ONESHOOT, false);
				break;
			case BUTTON_2:
				system_mode_speed_set(MODE_FIXED_CYCLE, VALUE_UNMODIFIED);
				rgb_led_mode(RGB_LED_COLOR_FIXED_CYCLE, RGB_LED_MODE_ONESHOOT, false);
				break;
			case BUTTON_6:
				system_mode_speed_set(MODE_AUTOMATIC_CYCLE, VALUE_UNMODIFIED);
				rgb_led_mode(RGB_LED_COLOR_AUTOMATIC_CYCLE, RGB_LED_MODE_ONESHOOT, false);
				break;
			case BUTTON_0:
				if (get_mode_set() != MODE_OFF) {
					system_mode_speed_set(MODE_OFF, SPEED_NONE);
					rgb_led_mode(RGB_LED_COLOR_POWER_OFF, RGB_LED_MODE_DOUBLE_BLINK, false);
				} else {
					system_mode_speed_set(VALUE_UNMODIFIED, VALUE_UNMODIFIED);
					rgb_led_mode(RGB_LED_COLOR_OFFSET + get_mode_set(), RGB_LED_MODE_ONESHOOT, false);
				}
				break;

			case BUTTON_7:
				system_mode_speed_set(VALUE_UNMODIFIED, SPEED_NIGHT);
				rgb_led_mode(RGB_LED_COLOR_OFFSET + get_mode_set(), RGB_LED_MODE_SPEED_NIGHT, false);
				break;

			case BUTTON_8:
				system_mode_speed_set(VALUE_UNMODIFIED, SPEED_LOW);
				rgb_led_mode(RGB_LED_COLOR_OFFSET + get_mode_set(), RGB_LED_MODE_SPEED_LOW, false);
				break;

			case BUTTON_9:
//				rgb_led_mode(RGB_LED_COLOR_OFFSET + get_mode_set(), RGB_LED_MODE_OFFSET + get_speed_set(), false);
				break;

			case BUTTON_10:
				system_mode_speed_set(VALUE_UNMODIFIED, SPEED_MEDIUM);
				rgb_led_mode(RGB_LED_COLOR_OFFSET + get_mode_set(), RGB_LED_MODE_SPEED_MEDIUM, false);
				break;

			case BUTTON_11:
				system_mode_speed_set(VALUE_UNMODIFIED, SPEED_HIGH);
				rgb_led_mode(RGB_LED_COLOR_OFFSET + get_mode_set(), RGB_LED_MODE_SPEED_HIGH, false);
				break;

			case (BUTTON_9_LONG):
				user_experience_state = RH_SETTING;
				rgb_led_mode(RGB_LED_COLOR_AQUA_RH, RGB_LED_MODE_CONF_OFFSET + get_relative_humidity_set(), true);
			break;
		}
		break;

		case RH_SETTING:
			switch(button) {
				case BUTTON_7:
					set_relative_humidity_set(RH_THRESHOLD_SETTING_NOT_CONFIGURED);
					user_experience_state = VOC_SETTING;
					rgb_led_mode(RGB_LED_COLOR_GREEN_VOC, RGB_LED_MODE_CONF_OFFSET + get_voc_set(), true);
					break;
				case BUTTON_8:
					set_relative_humidity_set(RH_THRESHOLD_SETTING_LOW);
					user_experience_state = VOC_SETTING;
					rgb_led_mode(RGB_LED_COLOR_GREEN_VOC, RGB_LED_MODE_CONF_OFFSET + get_voc_set(), true);
					break;
				case BUTTON_9:
					user_experience_state = VOC_SETTING;
					rgb_led_mode(RGB_LED_COLOR_GREEN_VOC, RGB_LED_MODE_CONF_OFFSET + get_voc_set(), true);
					break;
				case BUTTON_10:
					set_relative_humidity_set(RH_THRESHOLD_SETTING_MEDIUM);
					user_experience_state = VOC_SETTING;
					rgb_led_mode(RGB_LED_COLOR_GREEN_VOC, RGB_LED_MODE_CONF_OFFSET + get_voc_set(), true);
					break;
				case BUTTON_11:
					set_relative_humidity_set(RH_THRESHOLD_SETTING_HIGH);
					user_experience_state = VOC_SETTING;
					rgb_led_mode(RGB_LED_COLOR_GREEN_VOC, RGB_LED_MODE_CONF_OFFSET + get_voc_set(), true);
					break;
			}
			break;

		case VOC_SETTING:
			switch(button) {
				case BUTTON_7:
					set_voc_set(VOC_THRESHOLD_SETTING_NOT_CONFIGURED);
					user_experience_state = LUX_SETTING;
					rgb_led_mode(RGB_LED_COLOR_YELLOW_LUX, RGB_LED_MODE_CONF_OFFSET + get_lux_set(), true);
					break;
				case BUTTON_8:
					set_voc_set(VOC_THRESHOLD_SETTING_LOW);
					user_experience_state = LUX_SETTING;
					rgb_led_mode(RGB_LED_COLOR_YELLOW_LUX, RGB_LED_MODE_CONF_OFFSET + get_lux_set(), true);
					break;
				case BUTTON_9:
					user_experience_state = LUX_SETTING;
					rgb_led_mode(RGB_LED_COLOR_YELLOW_LUX, RGB_LED_MODE_CONF_OFFSET + get_lux_set(), true);
					break;
				case BUTTON_10:
					set_voc_set(VOC_THRESHOLD_SETTING_MEDIUM);
					user_experience_state = LUX_SETTING;
					rgb_led_mode(RGB_LED_COLOR_YELLOW_LUX, RGB_LED_MODE_CONF_OFFSET + get_lux_set(), true);
					break;
				case BUTTON_11:
					set_voc_set(VOC_THRESHOLD_SETTING_HIGH);
					user_experience_state = LUX_SETTING;
					rgb_led_mode(RGB_LED_COLOR_YELLOW_LUX, RGB_LED_MODE_CONF_OFFSET + get_lux_set(), true);
					break;
			}
		break;

		case LUX_SETTING:
			switch(button) {
				case BUTTON_7:
					set_lux_set(LUX_THRESHOLD_SETTING_NOT_CONFIGURED);
					user_experience_state = OPERATIVE;
					rgb_led_mode(RGB_LED_COLOR_NONE, RGB_LED_MODE_NONE, false);
					break;
				case BUTTON_8:
					set_lux_set(LUX_THRESHOLD_SETTING_LOW);
					user_experience_state = OPERATIVE;
					rgb_led_mode(RGB_LED_COLOR_NONE, RGB_LED_MODE_NONE, false);
					break;
				case BUTTON_9:
					user_experience_state = OPERATIVE;
					rgb_led_mode(RGB_LED_COLOR_NONE, RGB_LED_MODE_NONE, false);
					break;
				case BUTTON_10:
					set_lux_set(LUX_THRESHOLD_SETTING_MEDIUM);
					user_experience_state = OPERATIVE;
					rgb_led_mode(RGB_LED_COLOR_NONE, RGB_LED_MODE_NONE, false);
					break;
				case BUTTON_11:
					set_lux_set(LUX_THRESHOLD_SETTING_HIGH);
					user_experience_state = OPERATIVE;
					rgb_led_mode(RGB_LED_COLOR_YELLOW_LUX, RGB_LED_MODE_CONF_OFFSET + get_lux_set(), true);
					break;
			}
		break;

		case SETTINGS:
			switch (button) {
				case BUTTON_10:
					user_experience_type = 0;
					user_experience_state = OPERATIVE;
					rgb_led_mode(RGB_LED_COLOR_AQUA_RH, RGB_LED_MODE_ONESHOOT, false);
					break;
				case BUTTON_11:
					user_experience_type = 1;
					user_experience_state = OPERATIVE;
					rgb_led_mode(RGB_LED_COLOR_YELLOW_LUX, RGB_LED_MODE_ONESHOOT, false);
					break;
			}
			break;
		}
	}
	else {
		switch (user_experience_state) {
			case OPERATIVE:
				switch (button) {
					case BUTTON_0:
						if (get_mode_set() != MODE_OFF) {
							system_mode_speed_set(MODE_OFF, SPEED_NONE);
							rgb_led_mode(RGB_LED_COLOR_POWER_OFF, RGB_LED_MODE_DOUBLE_BLINK, false);
						} else {
							system_mode_speed_set(VALUE_UNMODIFIED, VALUE_UNMODIFIED);
							rgb_led_mode((RGB_LED_COLOR_OFFSET + get_mode_set()), RGB_LED_MODE_ONESHOOT, false);
						}
						break;

					case BUTTON_1:
						if (get_mode_set() != MODE_OFF) {
							rgb_led_mode((RGB_LED_COLOR_OFFSET + get_mode_set()), (RGB_LED_MODE_OFFSET + get_speed_set()), false);
						}
						else {
							rgb_led_mode(RGB_LED_COLOR_POWER_OFF, RGB_LED_MODE_DOUBLE_BLINK, false);
						}
						break;

					case BUTTON_1_LONG:
						user_experience_state = SETTINGS;
						rgb_led_mode(RGB_LED_COLOR_GREEN, RGB_LED_MODE_ALWAYS_ON, false);
						break;
					case BUTTON_2:
						if (get_speed_set() < SPEED_HIGH) {
							system_mode_speed_set(VALUE_UNMODIFIED, get_speed_set() + 1);
						}
						rgb_led_mode((RGB_LED_COLOR_OFFSET + get_mode_set()), (RGB_LED_MODE_OFFSET + get_speed_set()), false);
						break;
					case BUTTON_3:
						system_mode_speed_set(MODE_IMMISSION, VALUE_UNMODIFIED);
						rgb_led_mode(RGB_LED_COLOR_IMMISSION, RGB_LED_MODE_ONESHOOT, false);
						break;
					case BUTTON_4:
						system_mode_speed_set(MODE_AUTOMATIC_CYCLE, VALUE_UNMODIFIED);
						rgb_led_mode(RGB_LED_COLOR_AUTOMATIC_CYCLE, RGB_LED_MODE_ONESHOOT, false);
						break;
					case BUTTON_5:
						system_mode_speed_set(MODE_EMISSION, VALUE_UNMODIFIED);
						rgb_led_mode(RGB_LED_COLOR_EMISSION, RGB_LED_MODE_ONESHOOT, false);
						break;
					case BUTTON_6:
						if (get_speed_set() > SPEED_NIGHT) {
							system_mode_speed_set(VALUE_UNMODIFIED, get_speed_set() - 1);
						}
						rgb_led_mode((RGB_LED_COLOR_OFFSET + get_mode_set()), (RGB_LED_MODE_OFFSET + get_speed_set()), false);
						break;
					case BUTTON_7:
						rgb_led_mode(RGB_LED_COLOR_AQUA_RH, RGB_LED_MODE_CONF_OFFSET + get_relative_humidity_set(), false);
						break;
					case BUTTON_7_LONG:
						user_experience_state = RH_SETTING;
						rgb_led_mode(RGB_LED_COLOR_AQUA_RH, RGB_LED_MODE_CONF_OFFSET + get_relative_humidity_set(), true);
						break;
					case BUTTON_8:
						rgb_led_mode(RGB_LED_COLOR_GREEN_VOC, RGB_LED_MODE_CONF_OFFSET + get_voc_set(), false);
						break;
					case BUTTON_8_LONG:
						user_experience_state = VOC_SETTING;
						rgb_led_mode(RGB_LED_COLOR_GREEN_VOC, RGB_LED_MODE_CONF_OFFSET + get_voc_set(), true);
						break;
					case BUTTON_9:
						break;
					case BUTTON_10:
						rgb_led_mode(RGB_LED_COLOR_YELLOW_LUX, RGB_LED_MODE_CONF_OFFSET + get_lux_set(), false);
						break;
					case BUTTON_10_LONG:
						user_experience_state = LUX_SETTING;
						rgb_led_mode(RGB_LED_COLOR_YELLOW_LUX, RGB_LED_MODE_CONF_OFFSET + get_lux_set(), true);
						break;
					case BUTTON_11:
						break;
				}
				break;

			case RH_SETTING:
				switch (button) {
					case BUTTON_7:
						set_relative_humidity_set(RH_THRESHOLD_SETTING_NOT_CONFIGURED);
						user_experience_state = OPERATIVE;
						rgb_led_mode(RGB_LED_COLOR_NONE, RGB_LED_MODE_NONE, false);
						break;
					case BUTTON_8:
						set_relative_humidity_set(RH_THRESHOLD_SETTING_LOW);
						user_experience_state = OPERATIVE;
						rgb_led_mode(RGB_LED_COLOR_NONE, RGB_LED_MODE_NONE, false);
						break;
					case BUTTON_9:
						user_experience_state = OPERATIVE;
						rgb_led_mode(RGB_LED_COLOR_NONE, RGB_LED_MODE_NONE, false);
						break;
					case BUTTON_10:
						set_relative_humidity_set(RH_THRESHOLD_SETTING_MEDIUM);
						user_experience_state = OPERATIVE;
						rgb_led_mode(RGB_LED_COLOR_NONE, RGB_LED_MODE_NONE, false);
						break;
					case BUTTON_11:
						set_relative_humidity_set(RH_THRESHOLD_SETTING_HIGH);
						user_experience_state = OPERATIVE;
						rgb_led_mode(RGB_LED_COLOR_NONE, RGB_LED_MODE_NONE, false);
						break;
				}
				break;

			case VOC_SETTING:
				switch (button) {
					case BUTTON_7:
						set_voc_set(VOC_THRESHOLD_SETTING_NOT_CONFIGURED);
						user_experience_state = OPERATIVE;
						rgb_led_mode(RGB_LED_COLOR_NONE, RGB_LED_MODE_NONE, false);
						break;
					case BUTTON_8:
						set_voc_set(VOC_THRESHOLD_SETTING_LOW);
						user_experience_state = OPERATIVE;
						rgb_led_mode(RGB_LED_COLOR_NONE, RGB_LED_MODE_NONE, false);
						break;
					case BUTTON_9:
						user_experience_state = OPERATIVE;
						rgb_led_mode(RGB_LED_COLOR_NONE, RGB_LED_MODE_NONE, false);
						break;
					case BUTTON_10:
						set_voc_set(VOC_THRESHOLD_SETTING_MEDIUM);
						user_experience_state = OPERATIVE;
						rgb_led_mode(RGB_LED_COLOR_NONE, RGB_LED_MODE_NONE, false);
						break;
					case BUTTON_11:
						set_voc_set(VOC_THRESHOLD_SETTING_HIGH);
						user_experience_state = OPERATIVE;
						rgb_led_mode(RGB_LED_COLOR_NONE, RGB_LED_MODE_NONE, false);
						break;
				}
				break;

			case LUX_SETTING:
				switch (button) {
					case BUTTON_7:
						set_lux_set(LUX_THRESHOLD_SETTING_NOT_CONFIGURED);
						user_experience_state = OPERATIVE;
						rgb_led_mode(RGB_LED_COLOR_NONE, RGB_LED_MODE_NONE, false);
						break;
					case BUTTON_8:
						set_lux_set(LUX_THRESHOLD_SETTING_LOW);
						user_experience_state = OPERATIVE;
						rgb_led_mode(RGB_LED_COLOR_NONE, RGB_LED_MODE_NONE, false);
						break;
					case BUTTON_9:
						user_experience_state = OPERATIVE;
						rgb_led_mode(RGB_LED_COLOR_NONE, RGB_LED_MODE_NONE, false);
						break;
					case BUTTON_10:
						set_lux_set(LUX_THRESHOLD_SETTING_MEDIUM);
						user_experience_state = OPERATIVE;
						rgb_led_mode(RGB_LED_COLOR_NONE, RGB_LED_MODE_NONE, false);
						break;
					case BUTTON_11:
						set_lux_set(LUX_THRESHOLD_SETTING_HIGH);
						user_experience_state = OPERATIVE;
						rgb_led_mode(RGB_LED_COLOR_NONE, RGB_LED_MODE_NONE, false);
						break;
				}
				break;

				case SETTINGS:
					switch (button) {
						case BUTTON_10:
							user_experience_type = 0;
							user_experience_state = OPERATIVE;
							rgb_led_mode(RGB_LED_COLOR_AQUA_RH, RGB_LED_MODE_ONESHOOT, false);
							break;
						case BUTTON_11:
							user_experience_type = 1;
							user_experience_state = OPERATIVE;
							rgb_led_mode(RGB_LED_COLOR_YELLOW_LUX, RGB_LED_MODE_ONESHOOT, false);
							break;
					}
					break;
		}
	}
}

int user_experience_init() {
	BaseType_t user_experience_task_created = xTaskCreate(user_experience_task, "User experience task ", USER_EXPERIENCE_TASK_STACK_SIZE, NULL, USER_EXPERIENCE_TASK_PRIORITY, NULL);

	return ((user_experience_task_created) == pdPASS ? 0 : -1);
}
