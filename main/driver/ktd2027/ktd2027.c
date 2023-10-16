/**
 * @file ktd2027.c
 *
 * Copyright (c) Youcef BENAKMOUME
 *
 */

#include "ktd2027.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "math.h"

struct ktd2027_config ktd2027_config;

static int write_register(uint8_t reg, uint8_t val) {
    uint8_t data[2] = {reg, val};

    if (xSemaphoreTake(ktd2027_config.i2c_dev->lock, ktd2027_config.i2c_dev->i2c_timeout) != pdTRUE) {
        return -1;
    }

    if (i2c_master_write_to_device(ktd2027_config.i2c_dev->i2c_num, ktd2027_config.i2c_dev_address, data, sizeof(data), ktd2027_config.i2c_dev->i2c_timeout)) {
        xSemaphoreGive(ktd2027_config.i2c_dev->lock);
        return -1;
    }

    xSemaphoreGive(ktd2027_config.i2c_dev->lock);
    return 0;
}

#define MOD_WRITE

static int write_all_register(uint8_t Period, uint8_t Tslot, uint8_t PWM1, uint8_t PWM2, uint8_t RFscale, uint8_t Traise, uint8_t Tfall, uint8_t RampLinear,
		uint8_t LED1_I, uint8_t LED2_I, uint8_t LED3_I, uint8_t LED1_EN, uint8_t LED2_EN, uint8_t LED3_EN) {
    uint8_t data[] = {
    		KTD2027_REG_EN_RST,
			(Tslot & 0x3) | ((RFscale << 5) & 0x60),
			(Period & 0x7f) | ((RampLinear << 7) & 0x80),
//			PWM1, PWM2,
			0x00, 0x00,
//			(LED1_EN & 0x3) | ((LED2_EN << 2) & 0xc) | ((LED3_EN << 4) & 0x30),
			0x00,
			(Traise & 0xf) | ((Tfall << 4) & 0xf0),
			LED1_I, LED2_I, LED3_I, 0x00
	};

    uint8_t data_enable[] = {
    		KTD2027_REG_LED_EN,
			(LED1_EN & 0x3) | ((LED2_EN << 2) & 0xc) | ((LED3_EN << 4) & 0x30)
    };

    uint8_t data_pwm[] = {
    		KTD2027_REG_PWM1_TIMER,
			PWM1, PWM2
    };

    if (xSemaphoreTake(ktd2027_config.i2c_dev->lock, ktd2027_config.i2c_dev->i2c_timeout) != pdTRUE) {
        return -1;
    }

    if (i2c_master_write_to_device(ktd2027_config.i2c_dev->i2c_num, ktd2027_config.i2c_dev_address, data, sizeof(data), ktd2027_config.i2c_dev->i2c_timeout)) {
        xSemaphoreGive(ktd2027_config.i2c_dev->lock);
        return -1;
    }

    if (i2c_master_write_to_device(ktd2027_config.i2c_dev->i2c_num, ktd2027_config.i2c_dev_address, data_enable, sizeof(data_enable), ktd2027_config.i2c_dev->i2c_timeout)) {
        xSemaphoreGive(ktd2027_config.i2c_dev->lock);
        return -1;
    }

    if (i2c_master_write_to_device(ktd2027_config.i2c_dev->i2c_num, ktd2027_config.i2c_dev_address, data_pwm, sizeof(data_pwm), ktd2027_config.i2c_dev->i2c_timeout)) {
        xSemaphoreGive(ktd2027_config.i2c_dev->lock);
        return -1;
    }

    xSemaphoreGive(ktd2027_config.i2c_dev->lock);

    return 0;
}

int ktd2027_init(struct i2c_dev_s *i2c_dev) {
	if (!i2c_dev) {
		printf("Error: Invalid i2c_dev pointer.\n");
		return -1;
	}

	ktd2027_config.i2c_dev = i2c_dev;
	ktd2027_config.i2c_dev_address = KTD2027_I2C_ADDR;
//	printf("Initializing KTD2027 with I2C address: 0x%x\n", KTD2027_I2C_ADDR);

	write_register(KTD2027_REG_EN_RST, 7);

	vTaskDelay(10 / portTICK_PERIOD_MS);

	write_all_register(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

//	printf("KTD2027 initialization successful.\n");

	return 0;
}

int ktd2027_led_set(uint8_t color, uint8_t mode) {
//	printf("Setting LED with color: %d, mode: %d\n", color, mode);

	switch (mode) {
		case RGB_LED_MODE_OFF:
			write_all_register(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
			write_register(KTD2027_REG_EN_RST, 0x06 | 0x18);
			break;

		case RGB_LED_MODE_ON:
			if (color == RGB_LED_COLOR_RED) {
				write_all_register(0, 0, 0, 0, 0, 0, 0, 0, 191, 0, 0, 1, 0, 0);
			} else if (color == RGB_LED_COLOR_GREEN) {
				write_all_register(0, 0, 0, 0, 0, 0, 0, 0, 0, 191, 0, 0, 1, 0);
			} else if (color == RGB_LED_COLOR_BLUE) {
				write_all_register(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 191, 0, 0, 1);
			} else if (color == RGB_LED_COLOR_AQUA_RH) {
				write_all_register(0, 0, 0, 0, 0, 0, 0, 0, 0, 191, 191, 0, 1, 1);
			} else if (color == RGB_LED_COLOR_GREEN_VOC) {
				write_all_register(0, 0, 0, 0, 0, 0, 0, 0, 0, 191, 0, 0, 1, 0);
			} else if (color == RGB_LED_COLOR_YELLOW_LUX) {
				write_all_register(0, 0, 0, 0, 0, 0, 0, 0, 191, 39, 0, 1, 1, 0);
			} else if (color == RGB_LED_COLOR_IMMISSION) {
//				write_all_register(0, 0, 0, 0, 0, 0, 0, 0, 39, 0, 191, 1, 0, 1);
//				write_all_register(10, 0, 150, 0, 0, 2, 2, 0, 39, 0, 191, 2, 0, 2);
				write_all_register(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 191, 0, 0, 1);
			} else if (color == RGB_LED_COLOR_EMISSION) {
//				write_all_register(0, 0, 0, 0, 0, 0, 0, 0, 191, 0, 39, 1, 0, 1);
//				write_all_register(10, 0, 150, 0, 0, 2, 2, 0, 191, 0, 39, 2, 0, 2);
				write_all_register(0, 0, 0, 0, 0, 0, 0, 0, 191, 0, 0, 1, 0, 0);
			} else if (color == RGB_LED_COLOR_FIXED_CYCLE) {
				write_all_register(0, 0, 0, 0, 0, 0, 0, 0, 191, 191, 191, 1, 1, 1);
			} else if (color == RGB_LED_COLOR_AUTOMATIC_CYCLE) {
//				write_all_register(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 191, 0, 0, 1);
//				write_all_register(10, 0, 150, 0, 0, 2, 2, 0, 0, 0, 191, 0, 0, 2);
			} else if (color == RGB_LED_COLOR_WHITE) {
				write_all_register(0, 0, 0, 0, 0, 0, 0, 0, 191, 191, 191, 1, 1, 1);
			} else if (color == RGB_LED_COLOR_TEST) {
				write_all_register(0, 0, 0, 0, 0, 0, 0, 0, 191, 191, 191, 1, 1, 1);
			} else {
				printf("Error: Invalid color value.\n");
				return -1;
			}
			break;

		case RGB_LED_MODE_BLINK:
			switch(color) {
				case RGB_LED_COLOR_GREEN:
					write_all_register(2, 0, 15, 0, 0, 0, 0, 0, 0, 191, 0, 0, 2, 0);
					break;
				case RGB_LED_COLOR_POWER_ON:
					write_all_register(FLASH_PERIOD_POWER_ON, 0, 31, 0, 0, 0, 0, 0, 0, 191, 0, 0, 2, 0);
					break;
				case RGB_LED_COLOR_POWER_OFF:
					write_all_register(FLASH_PERIOD_POWER_OFF, 0, 63, 0, 0, 0, 0, 0, 191, 0, 0, 2, 0, 0);
					break;
				case RGB_LED_COLOR_AUTOMATIC_CYCLE:
					write_all_register(FLASH_PERIOD_AUTOMATIC_CYCLE_BLINK, 1, FLASH_PWM_AUTOMATIC_CYCLE_BLINK, FLASH_PWM_AUTOMATIC_CYCLE_BLINK, 0, 0, 0, 0, 191, 0, 191, 2, 0, 3);
					break;
				case RGB_LED_COLOR_WHITE:
					write_all_register(FLASH_PERIOD_CLEAR_WARNING, 0, 31, 0, 0, 0, 0, 0, 191, 191, 191, 2, 2, 2);
					break;
			}
			break;

		case RGB_LED_MODE_SPEED_NIGHT_FADE:
			switch(color) {
				case RGB_LED_COLOR_IMMISSION:
//					write_all_register(FLASH_PERIOD_SPEED_NIGHT, 0, 0, 0, 0, 0, 0, 0, 9, 0, 19, 1, 0, 1);
					write_all_register(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 19, 0, 0, 1);
					break;
				case RGB_LED_COLOR_EMISSION:
//					write_all_register(FLASH_PERIOD_SPEED_NIGHT, 0, 0, 0, 0, 0, 0, 0, 19, 0, 9, 1, 0, 1);
					write_all_register(0, 0, 0, 0, 0, 0, 0, 0, 19, 0, 0, 1, 0, 0);
					break;
				case RGB_LED_COLOR_FIXED_CYCLE:
					write_all_register(FLASH_PERIOD_SPEED_NIGHT, 0, 0, 0, 0, 0, 0, 0, 19, 19, 19, 1, 1, 1);
					break;
				case RGB_LED_COLOR_AUTOMATIC_CYCLE:
//					write_all_register(FLASH_PERIOD_SPEED_NIGHT, 0, 0, 0, 0, 0, 0, 0, 0, 0, 19, 0, 0, 1);
					write_all_register(0, 1, FLASH_PWM_SPEED_NIGHT, FLASH_PWM_SPEED_NIGHT, 0, 0, 0, 0, 19, 0, 19, 2, 0, 3);
					break;
			}
			break;

		case RGB_LED_MODE_SPEED_BLINK:
			switch(color) {
				case RGB_LED_COLOR_IMMISSION:
//					write_all_register(FLASH_PERIOD_SPEED_BLINK, 0, FLASH_PWM_SPEED_BLINK, 0, 0, 0, 0, 0, 39, 0, 191, 2, 0, 2);
					write_all_register(FLASH_PERIOD_SPEED_BLINK, 0, FLASH_PWM_SPEED_BLINK, 0, 0, 0, 0, 0, 0, 0, 191, 0, 0, 2);
					break;
				case RGB_LED_COLOR_EMISSION:
//					write_all_register(FLASH_PERIOD_SPEED_BLINK, 0, FLASH_PWM_SPEED_BLINK, 0, 0, 0, 0, 0, 191, 0, 39, 2, 0, 2);
					write_all_register(FLASH_PERIOD_SPEED_BLINK, 0, FLASH_PWM_SPEED_BLINK, 0, 0, 0, 0, 0, 191, 0, 0, 2, 0, 0);
					break;
				case RGB_LED_COLOR_FIXED_CYCLE:
					write_all_register(FLASH_PERIOD_SPEED_BLINK, 0, FLASH_PWM_SPEED_BLINK, 0, 0, 0, 0, 0, 191, 191, 191, 2, 2, 2);
					break;
				case RGB_LED_COLOR_AUTOMATIC_CYCLE:
//					write_all_register(FLASH_PERIOD_SPEED_BLINK, 0, FLASH_PWM_SPEED_BLINK, 0, 0, 0, 0, 0, 0, 0, 191, 0, 0, 2);
//					write_all_register(FLASH_PERIOD_SPEED_BLINK, 1, FLASH_PWM_SPEED_BLINK, FLASH_PWM_SPEED_BLINK, 0, 0, 0, 0, 191, 0, 191, 2, 0, 3);
					write_all_register(FLASH_PERIOD_SPEED_BLINK_AUTO, 1, FLASH_PWM_SPEED_BLINK_AUTO, FLASH_PWM_SPEED_BLINK_AUTO, 0, 0, 0, 0, 191, 0, 191, 2, 0, 3);
					break;
			}
			break;

		case RGB_LED_MODE_NOT_CONF:
			switch(color) {
				case RGB_LED_COLOR_AQUA_RH:
					write_all_register(FLASH_PERIOD, 0, FLASH_PWM_MAX, 0, 0, 0, 0, 0, 0, 39, 39, 0, 2, 2);
					break;

				case RGB_LED_COLOR_GREEN_VOC:
					write_all_register(FLASH_PERIOD, 0, FLASH_PWM_MAX, 0, 0, 0, 0, 0, 0, 39, 0, 0, 2, 0);
					break;

				case RGB_LED_COLOR_YELLOW_LUX:
//					write_all_register(0, 0, 0, 0, 0, 0, 0, 0, 39, 29, 0, 1, 1, 0);
					write_all_register(FLASH_PERIOD, 0, FLASH_PWM_MAX, 0, 0, 0, 0, 0, 59, 19, 0, 2, 2, 0);
					break;
			}
			break;

		case RGB_LED_MODE_PRE:
			switch(color) {
				case RGB_LED_COLOR_AQUA_RH:
//					write_all_register(FLASH_PERIOD, 0, FLASH_PWM_MAX, 0, 0, 0, FLASH_FALL, 0, 0, 159, 159, 0, 2, 2);
					write_all_register(FLASH_PERIOD, 0, FLASH_PWM_MAX, 0, 0, 0, FLASH_FALL, 0, 0, 191, 191, 0, 2, 2);
					break;

				case RGB_LED_COLOR_GREEN_VOC:
//					write_all_register(FLASH_PERIOD, 0, FLASH_PWM_MAX, 0, 0, 0, FLASH_FALL, 0, 0, 159, 0, 0, 2, 0);
					write_all_register(FLASH_PERIOD, 0, FLASH_PWM_MAX, 0, 0, 0, FLASH_FALL, 0, 0, 191, 0, 0, 2, 0);
					break;

				case RGB_LED_COLOR_YELLOW_LUX:
//					write_all_register(FLASH_PERIOD, 0, FLASH_PWM_MAX, 0, 0, 0, FLASH_FALL, 0, 159, 119, 0, 2, 2, 0);
					write_all_register(FLASH_PERIOD, 0, FLASH_PWM_MAX, 0, 0, 0, FLASH_FALL, 0, 191, 39, 0, 2, 2, 0);
					break;
			}
			break;

		case RGB_LED_MODE_BLINK_INVERTED:
//			write_register(KTD2027_REG_PWM1_TIMER, FLASH_PWM_WITH_FALL);
			switch(color) {
				case RGB_LED_COLOR_AQUA_RH:
					write_all_register(FLASH_PERIOD, 0, FLASH_PWM_WITH_FALL, 0, 0, 0, FLASH_FALL, 0, 0, 191, 191, 0, 2, 2);
					break;

				case RGB_LED_COLOR_GREEN_VOC:
					write_all_register(FLASH_PERIOD, 0, FLASH_PWM_WITH_FALL, 0, 0, 0, FLASH_FALL, 0, 0, 191, 0, 0, 2, 0);
					break;

				case RGB_LED_COLOR_YELLOW_LUX:
					write_all_register(FLASH_PERIOD, 0, FLASH_PWM_WITH_FALL, 0, 0, 0, FLASH_FALL, 0, 191, 39, 0, 2, 2, 0);
					break;
			}
			break;

		case RGB_LED_MODE_POST:
//			write_register(KTD2027_REG_PWM1_TIMER, FLASH_PWM_MAX);
			write_register(KTD2027_REG_PWM1_TIMER, FLASH_PWM_MIN);
			break;

		default:
			printf("Error: Invalid mode.\n");
			return -1;
	}

//	printf("LED set successfully.\n");
	return 0;
}
