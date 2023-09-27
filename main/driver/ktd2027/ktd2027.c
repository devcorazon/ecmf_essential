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

static int write_all_register(uint8_t Period, uint8_t Tslot, uint8_t PWM1, uint8_t PWM2, uint8_t RFscale, uint8_t Traise, uint8_t Tfall, uint8_t RampLinear,
		uint8_t LED1_I, uint8_t LED2_I, uint8_t LED3_I, uint8_t LED1_EN, uint8_t LED2_EN, uint8_t LED3_EN) {
    uint8_t data[11] = {
    		KTD2027_REG_EN_RST,
			(Tslot & 0x3) | ((RFscale << 5) & 0x60),
			(Period & 0x7f) | ((RampLinear << 7) & 0x80),
			PWM1, PWM2,
			(LED1_EN & 0x3) | ((LED2_EN << 2) & 0xc) | ((LED3_EN << 4) & 0x30),
//			0x00,
			(Traise & 0xf) | ((Tfall << 4) & 0xf0),
			LED1_I, LED2_I, LED3_I, 0x00
	};

//    uint8_t data_enable[2] = {
//    		KTD2027_REG_LED_EN,
//			(LED1_EN & 0x3) | ((LED2_EN << 2) & 0xc) | ((LED3_EN << 4) & 0x30)
//    };

    if (xSemaphoreTake(ktd2027_config.i2c_dev->lock, ktd2027_config.i2c_dev->i2c_timeout) != pdTRUE) {
        return -1;
    }

    if (i2c_master_write_to_device(ktd2027_config.i2c_dev->i2c_num, ktd2027_config.i2c_dev_address, data, sizeof(data), ktd2027_config.i2c_dev->i2c_timeout)) {
        xSemaphoreGive(ktd2027_config.i2c_dev->lock);
        return -1;
    }

//    if (i2c_master_write_to_device(ktd2027_config.i2c_dev->i2c_num, ktd2027_config.i2c_dev_address, data_enable, sizeof(data_enable), ktd2027_config.i2c_dev->i2c_timeout)) {
//        xSemaphoreGive(ktd2027_config.i2c_dev->lock);
//        return -1;
//    }

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
			write_all_register(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
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
				write_register(KTD2027_REG_BLUE_LED, 191);
				write_register(KTD2027_REG_LED_EN, 0x10);
			} else {
				printf("Error: Invalid color value.\n");
				return -1;
			}
			break;

		case RGB_LED_MODE_BLANK:
			write_register(KTD2027_REG_LED_EN, 0);
//			write_all_register(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
			break;

		case RGB_LED_MODE_BLINK_FAST:
			switch(color) {
				case RGB_LED_COLOR_POWER_ON:
					write_all_register(FLASH_PERIOD_POWER_ON, 0, 30, 0, 0, 0, 0, 0, 0, 191, 0, 0, 2, 0);
					break;
			}
			break;

		case RGB_LED_MODE_BLINK_SLOW:
			switch(color) {
				case RGB_LED_COLOR_POWER_OFF:
					write_all_register(FLASH_PERIOD_POWER_OFF, 0, 50, 0, 0, 0, 0, 0, 191, 0, 0, 2, 0, 0);
					break;
			}
			break;

		case RGB_LED_MODE_SPEED_NIGHT_FADE:
			switch(color) {
				case RGB_LED_COLOR_IMMISSION:
					write_all_register(FLASH_PERIOD_SPEED_NIGHT, 0, 200, 0, 3, 2, 2, 0, 19, 0, 9, 2, 0, 2);
					break;
				case RGB_LED_COLOR_EMISSION:
					write_all_register(FLASH_PERIOD_SPEED_NIGHT, 0, 200, 0, 3, 2, 2, 0, 9, 0, 19, 2, 0, 2);
					break;
				case RGB_LED_COLOR_FIXED_CYCLE:
					write_all_register(FLASH_PERIOD_SPEED_NIGHT, 0, 200, 0, 3, 2, 2, 0, 19, 19, 19, 2, 2, 2);
					break;
				case RGB_LED_COLOR_AUTOMATIC_CYCLE:
					write_all_register(FLASH_PERIOD_SPEED_NIGHT, 0, 200, 0, 3, 2, 2, 0, 0, 0, 19, 0, 0, 2);
					break;
			}
			break;

		case RGB_LED_MODE_SPEED_BLINK:
			switch(color) {
				case RGB_LED_COLOR_IMMISSION:
					write_all_register(FLASH_PERIOD_SPEED_BLINK, 0, 64, 0, 3, 1, 1, 0, 191, 0, 39, 2, 0, 2);
					break;
				case RGB_LED_COLOR_EMISSION:
					write_all_register(FLASH_PERIOD_SPEED_BLINK, 0, 64, 0, 3, 1, 1, 0, 39, 0, 191, 2, 0, 2);
					break;
				case RGB_LED_COLOR_FIXED_CYCLE:
					write_all_register(FLASH_PERIOD_SPEED_BLINK, 0, 64, 0, 3, 1, 1, 0, 191, 191, 191, 2, 2, 2);
					break;
				case RGB_LED_COLOR_AUTOMATIC_CYCLE:
					write_all_register(FLASH_PERIOD_SPEED_BLINK, 0, 64, 0, 3, 1, 1, 0, 0, 0, 191, 0, 0, 2);
					break;
			}
			break;

		case RGB_LED_MODE_NOT_CONF:
			switch(color) {
				case RGB_LED_COLOR_AQUA_RH:
					write_all_register(FLASH_PERIOD, 0, 255, 0, 0, 0, 0, 0, 0, 39, 39, 0, 2, 2);
					break;

				case RGB_LED_COLOR_GREEN_VOC:
					write_all_register(FLASH_PERIOD, 0, 255, 0, 0, 0, 0, 0, 0, 39, 0, 0, 2, 0);
					break;

				case RGB_LED_COLOR_YELLOW_LUX:
					write_all_register(FLASH_PERIOD, 0, 255, 0, 0, 0, 0, 0, 39, 29, 0, 2, 2, 0);
					break;
			}
			break;

		case RGB_LED_MODE_PRE:
			switch(color) {
				case RGB_LED_COLOR_AQUA_RH:
//					write_all_register(PRE_PERIOD, 0, 255, 0, 0, 0, 0, 0, 0, 159, 159, 0, 2, 2);
					write_all_register(FLASH_PERIOD, 0, 255, 0, 0, 0, 0, 0, 0, 159, 159, 0, 2, 2);
					break;

				case RGB_LED_COLOR_GREEN_VOC:
//					write_all_register(PRE_PERIOD, 0, 255, 0, 0, 0, 0, 0, 0, 159, 0, 0, 2, 0);
					write_all_register(FLASH_PERIOD, 0, 255, 0, 0, 0, 0, 0, 0, 159, 0, 0, 2, 0);
					break;

				case RGB_LED_COLOR_YELLOW_LUX:
//					write_all_register(PRE_PERIOD, 0, 255, 0, 0, 0, 0, 0, 159, 119, 0, 2, 2, 0);
					write_all_register(FLASH_PERIOD, 0, 255, 0, 0, 0, 0, 0, 159, 119, 0, 2, 2, 0);
					break;
			}
			break;

			case RGB_LED_MODE_BLINK_INVERTED:
				switch(color) {
					case RGB_LED_COLOR_AQUA_RH:
//						write_all_register(FLASH_PERIOD, 0, 4, 0, 0, 0, FLASH_FALL, 0, 0, 159, 159, 0, 2, 2);
						write_register(KTD2027_REG_LED_RAMP, FLASH_FALL << 4);
						write_register(KTD2027_REG_PWM1_TIMER, 4);
						break;

					case RGB_LED_COLOR_GREEN_VOC:
//						write_all_register(FLASH_PERIOD, 0, 4, 0, 0, 0, FLASH_FALL, 0, 0, 159, 0, 0, 2, 0);
						write_register(KTD2027_REG_LED_RAMP, FLASH_FALL << 4);
						write_register(KTD2027_REG_PWM1_TIMER, 4);
						break;

					case RGB_LED_COLOR_YELLOW_LUX:
//						write_all_register(FLASH_PERIOD, 0, 4, 0, 0, 0, FLASH_FALL, 0, 159, 119, 0, 2, 2, 0);
						write_register(KTD2027_REG_LED_RAMP, FLASH_FALL << 4);
						write_register(KTD2027_REG_PWM1_TIMER, 4);
						break;
				}
				break;

				case RGB_LED_MODE_END:
					switch(color) {
						case RGB_LED_COLOR_AQUA_RH:
//							write_all_register(END_PERIOD, 0, 255, 0, 0, 0, 0, 0, 0, 159, 159, 0, 2, 2);
							write_register(KTD2027_REG_PWM1_TIMER, 255);
							break;

						case RGB_LED_COLOR_GREEN_VOC:
//							write_all_register(END_PERIOD, 0, 255, 0, 0, 0, 0, 0, 0, 159, 0, 0, 2, 0);
							write_register(KTD2027_REG_PWM1_TIMER, 255);
							break;

						case RGB_LED_COLOR_YELLOW_LUX:
//							write_all_register(END_PERIOD, 0, 255, 0, 0, 0, 0, 0, 159, 119, 0, 2, 2, 0);
							write_register(KTD2027_REG_PWM1_TIMER, 255);
							break;
					}
					break;

		default:
			printf("Error: Invalid mode.\n");
			return -1;
	}

//	printf("LED set successfully.\n");
	return 0;
}
