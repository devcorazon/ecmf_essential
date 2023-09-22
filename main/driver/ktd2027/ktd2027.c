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

	if (write_register(KTD2027_REG_EN_RST, 7) != 0) {
		printf("Failed to write to KTD2027_REG_EN_RST.\n");
		return -1;
	}

	vTaskDelay(10 / portTICK_PERIOD_MS);

	if (write_register(KTD2027_REG_EN_RST, 6) != 0) {
		printf("Failed to write to KTD2027_REG_EN_RST.\n");
		return -1;
	}

	vTaskDelay(10 / portTICK_PERIOD_MS);

	if (write_register(KTD2027_REG_EN_RST, 0) != 0) {
		printf("Failed to write to KTD2027_REG_EN_RST.\n");
		return -1;
	}

	if (write_register(KTD2027_REG_FLASH_PERIOD, 0) != 0) {
		printf("Failed to write to KTD2027_REG_FLASH_PERIOD.\n");
		return -1;
	}

	if (write_register(KTD2027_REG_PWM1_TIMER, 0) != 0) {
		printf("Failed to write to KTD2027_REG_PWM1_TIMER.\n");
		return -1;
	}

	if (write_register(KTD2027_REG_PWM2_TIMER, 0) != 0) {
		printf("Failed to write to KTD2027_REG_PWM2_TIMER.\n");
		return -1;
	}

	if (write_register(5, 0) != 0) {
		printf("Failed to write to RAMP_RATE_REG.\n");
		return -1;
	}

	if (write_register(KTD2027_REG_RED_LED, 0) != 0) {
		printf("Failed to set RED LED brightness.\n");
		return -1;
	}

	if (write_register(KTD2027_REG_GREEN_LED, 0) != 0) {
		printf("Failed to set GREEN LED brightness.\n");
		return -1;
	}

	if (write_register(KTD2027_REG_BLUE_LED, 0) != 0) {
		printf("Failed to set BLUE LED brightness.\n");
		return -1;
	}

	if (write_register(9, 0) != 0) {
		printf("Failed to write to LED4_IOUT_REG.\n");
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
//	printf("Setting LED with color: %d, mode: %d\n", color, mode);

	switch (mode) {
		case RGB_LED_MODE_OFF:
			write_register(KTD2027_REG_EN_RST, 0);
			write_register(KTD2027_REG_FLASH_PERIOD, 0);
			write_register(KTD2027_REG_PWM1_TIMER, 0);
			write_register(KTD2027_REG_PWM2_TIMER, 0);
			write_register(KTD2027_REG_LED_EN, 0);
			write_register(KTD2027_REG_LED_RAMP, 0);
			write_register(KTD2027_REG_RED_LED, 0);
			write_register(KTD2027_REG_GREEN_LED, 0);
			write_register(KTD2027_REG_BLUE_LED, 0);
			write_register(9, 0);

			break;

		case RGB_LED_MODE_ON:
			if (color == RGB_LED_COLOR_RED) {
				write_register(KTD2027_REG_RED_LED, MAX_BRIGHTNESS_VALUE);
				if (write_register(KTD2027_REG_LED_EN, KTD2027_RED_LED_EN) != 0) {
					printf("Failed to turn RED LED on.\n");
					return -1;
				}
			} else if (color == RGB_LED_COLOR_GREEN) {
				write_register(KTD2027_REG_GREEN_LED, MAX_BRIGHTNESS_VALUE);
				if (write_register(KTD2027_REG_LED_EN, KTD2027_GREEN_LED_EN) != 0) {
					printf("Failed to turn GREEN LED on.\n");
					return -1;
				}
			} else if (color == RGB_LED_COLOR_BLUE) {
				write_register(KTD2027_REG_BLUE_LED, MAX_BRIGHTNESS_VALUE);
				if (write_register(KTD2027_REG_LED_EN, KTD2027_BLUE_LED_EN) != 0) {
					printf("Failed to turn BLUE LED on.\n");
					return -1;
				}
			} else if (color == RGB_LED_COLOR_AQUA_RH) {
				write_register(KTD2027_REG_GREEN_LED, 159);		// 191
				write_register(KTD2027_REG_BLUE_LED, 159);		// 191

				write_register(KTD2027_REG_LED_EN, 0x14);
			} else if (color == RGB_LED_COLOR_GREEN_VOC) {
				write_register(KTD2027_REG_GREEN_LED, 119);		// 191

				write_register(KTD2027_REG_LED_EN, 0x4);
			} else if (color == RGB_LED_COLOR_YELLOW_LUX) {
				write_register(KTD2027_REG_RED_LED, 159);		// 191
				write_register(KTD2027_REG_GREEN_LED, 159);		// 159

				write_register(KTD2027_REG_LED_EN, 0x5);
			} else if (color == RGB_LED_COLOR_POWER_OFF) {
				write_register(KTD2027_REG_FLASH_PERIOD, FLASH_PERIOD_POWER_OFF);
				write_register(KTD2027_REG_RED_LED, 191);
				write_register(KTD2027_REG_LED_EN, 0x2);
				write_register(KTD2027_REG_PWM1_TIMER, 50);
			} else if (color == RGB_LED_COLOR_IMMISSION) {
				write_register(KTD2027_REG_RED_LED, 191);
				write_register(KTD2027_REG_BLUE_LED, 39);
				write_register(KTD2027_REG_LED_EN, 0x11);
			} else if (color == RGB_LED_COLOR_EMISSION) {
				write_register(KTD2027_REG_RED_LED, 39);
				write_register(KTD2027_REG_BLUE_LED, 191);
				write_register(KTD2027_REG_LED_EN, 0x11);
			} else if (color == RGB_LED_COLOR_FIXED_CYCLE) {
				write_register(KTD2027_REG_RED_LED, 191);
				write_register(KTD2027_REG_GREEN_LED, 191);
				write_register(KTD2027_REG_BLUE_LED, 191);
				write_register(KTD2027_REG_LED_EN, 0x15);
			} else if (color == RGB_LED_COLOR_AUTOMATIC_CYCLE) {
				write_register(KTD2027_REG_FLASH_PERIOD, FLASH_PERIOD);
				write_register(KTD2027_REG_BLUE_LED, 191);
				write_register(KTD2027_REG_LED_EN, 0x10);
			} else if (color == RGB_LED_BLANK) {
				write_register(KTD2027_REG_LED_EN, 0);
//				write_register(KTD2027_REG_FLASH_PERIOD, 0);
//				write_register(KTD2027_REG_PWM1_TIMER, 0);
//				write_register(KTD2027_REG_PWM2_TIMER, 0);
//				write_register(KTD2027_REG_LED_RAMP, 0);
//				write_register(KTD2027_REG_RED_LED, 0);
//				write_register(KTD2027_REG_GREEN_LED, 0);
//				write_register(KTD2027_REG_BLUE_LED, 0);
			} else if (color == RGB_LED_COLOR_AQUA_RH_NOT_CONF) {
				write_register(KTD2027_REG_GREEN_LED, 39);
				write_register(KTD2027_REG_BLUE_LED, 39);
				write_register(KTD2027_REG_LED_EN, 0x14);
			} else if (color == RGB_LED_COLOR_AQUA_RH_PRE) {
				write_register(KTD2027_REG_GREEN_LED, 159);
				write_register(KTD2027_REG_BLUE_LED, 159);
				write_register(KTD2027_REG_FLASH_PERIOD, 0x80 | FLASH_PERIOD);
				write_register(KTD2027_REG_LED_RAMP, 0x50);
				write_register(KTD2027_REG_LED_EN, 0x28);
				write_register(KTD2027_REG_PWM1_TIMER, 255);
			} else if (color == RGB_LED_COLOR_AQUA_RH_BLINK_INVERTED) {
				write_register(KTD2027_REG_LED_EN, 0x28);
				write_register(KTD2027_REG_PWM1_TIMER, 4);
			} else if (color == RGB_LED_COLOR_AQUA_RH_END) {
				write_register(KTD2027_REG_LED_EN, 0x28);
				write_register(KTD2027_REG_PWM1_TIMER, 255);
			} else if (color == RGB_LED_COLOR_GREEN_VOC_NOT_CONF) {
				write_register(KTD2027_REG_GREEN_LED, 39);
				write_register(KTD2027_REG_LED_EN, 0x4);
			} else if (color == RGB_LED_COLOR_GREEN_VOC_PRE) {
				write_register(KTD2027_REG_GREEN_LED, 119);
				write_register(KTD2027_REG_FLASH_PERIOD, 0x80 | FLASH_PERIOD);
				write_register(KTD2027_REG_LED_RAMP, 0x50);
				write_register(KTD2027_REG_LED_EN, 0x8);
				write_register(KTD2027_REG_PWM1_TIMER, 255);
			} else if (color == RGB_LED_COLOR_GREEN_VOC_BLINK_INVERTED) {
write_register(KTD2027_REG_LED_RAMP, 0x20);
write_register(KTD2027_REG_FLASH_PERIOD, FLASH_PERIOD);
				write_register(KTD2027_REG_LED_EN, 0x8);
				write_register(KTD2027_REG_PWM1_TIMER, 4);
			} else if (color == RGB_LED_COLOR_GREEN_VOC_END) {
				write_register(KTD2027_REG_LED_EN, 0x8);
				write_register(KTD2027_REG_PWM1_TIMER, 255);
			} else if (color == RGB_LED_COLOR_YELLOW_LUX_NOT_CONF) {
				write_register(KTD2027_REG_RED_LED, 79);
				write_register(KTD2027_REG_GREEN_LED, 39);
				write_register(KTD2027_REG_LED_EN, 0x5);
			} else if (color == RGB_LED_COLOR_YELLOW_LUX_PRE) {
				write_register(KTD2027_REG_RED_LED, 159);
				write_register(KTD2027_REG_GREEN_LED, 119);
				write_register(KTD2027_REG_FLASH_PERIOD, 0x80 | FLASH_PERIOD);
				write_register(KTD2027_REG_LED_RAMP, 0x50);
				write_register(KTD2027_REG_LED_EN, 0xa);
				write_register(KTD2027_REG_PWM1_TIMER, 255);
			} else if (color == RGB_LED_COLOR_YELLOW_LUX_BLINK_INVERTED) {
				write_register(KTD2027_REG_LED_EN, 0xa);
				write_register(KTD2027_REG_PWM1_TIMER, 4);
			} else if (color == RGB_LED_COLOR_YELLOW_LUX_END) {
				write_register(KTD2027_REG_LED_EN, 0xa);
				write_register(KTD2027_REG_PWM1_TIMER, 255);
			} else if (color == RGB_LED_COLOR_SPEED_NIGHT) {
//				write_register(KTD2027_REG_EN_RST, 0x60);
//				write_register(KTD2027_REG_BLUE_LED, 19);
//				write_register(KTD2027_REG_FLASH_PERIOD, FLASH_PERIOD_SPEED_NIGHT);
//				write_register(KTD2027_REG_LED_RAMP, 0x01);
//				write_register(KTD2027_REG_LED_EN, 0x20);
//				write_register(KTD2027_REG_PWM1_TIMER, 255);
				write_register(KTD2027_REG_BLUE_LED, 19);
				write_register(KTD2027_REG_LED_EN, 0x10);
			} else if (color == RGB_LED_COLOR_SPEED_BLINK) {
				write_register(KTD2027_REG_EN_RST, 0x60);
				write_register(KTD2027_REG_BLUE_LED, 159);
				write_register(KTD2027_REG_FLASH_PERIOD, FLASH_PERIOD_SPEED_BLINK);
				write_register(KTD2027_REG_LED_RAMP, 0x11);
				write_register(KTD2027_REG_LED_EN, 0x20);
				write_register(KTD2027_REG_PWM1_TIMER, 59);
			} else if (color == RGB_LED_COLOR_TEST) {
				write_register(KTD2027_REG_FLASH_PERIOD, FLASH_PERIOD);
				write_register(KTD2027_REG_LED_RAMP, 0x30);
				write_register(KTD2027_REG_RED_LED, 159);
				write_register(KTD2027_REG_GREEN_LED, 159);
//				write_register(KTD2027_REG_BLUE_LED, 159);

				////////
//				write_register(KTD2027_REG_LED_EN, 0x2);
//				write_register(KTD2027_REG_PWM1_TIMER, 4);

				////////
				write_register(KTD2027_REG_LED_RAMP, 0x50);
				write_register(KTD2027_REG_LED_EN, 0xa);
				write_register(KTD2027_REG_PWM1_TIMER, 255);
			} else {
				printf("Error: Invalid color value.\n");
				return -1;
			}
			break;

		default:
			printf("Error: Invalid mode.\n");
			return -1;
	}

//	printf("LED set successfully.\n");
	return 0;
}
