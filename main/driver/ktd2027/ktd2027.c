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

int ktd2027_init(struct i2c_dev_s *i2c_dev) {
	if (!i2c_dev) {
		printf("Error: Invalid i2c_dev pointer.\n");
		return -1;
	}

	ktd2027_config.i2c_dev = i2c_dev;
	ktd2027_config.i2c_dev_address = KTD2027_I2C_ADDR;
	printf("Initializing KTD2027 with I2C address: 0x%x\n", KTD2027_I2C_ADDR);

	if (write_register(KTD2027_REG_EN_RST, 2) != 0) {
		printf("Failed to write to KTD2027_REG_EN_RST.\n");
		return -1;
	}

	if (write_register(KTD2027_REG_RED_LED, MAX_BRIGHTNESS_VALUE) != 0) {
		printf("Failed to set RED LED brightness.\n");
		return -1;
	}

	if (write_register(KTD2027_REG_GREEN_LED, MAX_BRIGHTNESS_VALUE) != 0) {
		printf("Failed to set GREEN LED brightness.\n");
		return -1;
	}

	if (write_register(KTD2027_REG_BLUE_LED, MAX_BRIGHTNESS_VALUE) != 0) {
		printf("Failed to set BLUE LED brightness.\n");
		return -1;
	}

	if (write_register(KTD2027_REG_LED_EN, LED_OFF) != 0) {
		printf("Failed to turn LEDs off.\n");
		return -1;
	}

	printf("KTD2027 initialization successful.\n");
	return 0;
}

int ktd2027_led_set(uint8_t color, uint8_t mode) {
	printf("Setting LED with color: %d, mode: %d\n", color, mode);

	switch (mode) {
		case RGB_LED_MODE_OFF:
			if (write_register(KTD2027_REG_LED_EN, LED_OFF) != 0) {
				printf("Failed to turn LEDs off.\n");
				return -1;
			}
			break;

		case RGB_LED_MODE_ON:
			if (color == RGB_LED_COLOR_RED) {
				if (write_register(KTD2027_REG_LED_EN, KTD2027_RED_LED_EN) != 0) {
					printf("Failed to turn RED LED on.\n");
					return -1;
				}
			} else if (color == RGB_LED_COLOR_GREEN) {
				if (write_register(KTD2027_REG_LED_EN, KTD2027_GREEN_LED_EN) != 0) {
					printf("Failed to turn GREEN LED on.\n");
					return -1;
				}
			} else if (color == RGB_LED_COLOR_BLUE) {
				if (write_register(KTD2027_REG_LED_EN, KTD2027_BLUE_LED_EN) != 0) {
					printf("Failed to turn BLUE LED on.\n");
					return -1;
				}
			} else {
				printf("Error: Invalid color value.\n");
				return -1;
			}
			break;

		default:
			printf("Error: Invalid mode.\n");
			return -1;
	}

	printf("LED set successfully.\n");
	return 0;
}
