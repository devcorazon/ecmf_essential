/**
 * @file ltr303.c
 *
 * Copyright (c) Youcef BENAKMOUME
 *
 */
#include "ltr303.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "driver/i2c.h"

struct ltr303_config ltr303_config;

static const float ltr303_gain[] = {
	1.0f,
	2.0f,
	4.0f,
	8.0f,
	0.0f,
	0.0f,
	48.0f,
	96.0f,
};
static float ltr303_integration_time[] = {
	1.0f,
	0.5f,
	2.0f,
	4.0f,
	1.5f,
	2.5f,
	3.0f,
	3.5f,
};

static int write_register(uint8_t reg, uint8_t val) {
    uint8_t data[2] = {reg, val};

    if (xSemaphoreTake(ltr303_config.i2c_dev->lock, ltr303_config.i2c_dev->i2c_timeout) != pdTRUE) {
        return -1;
    }

    if (i2c_master_write_to_device(ltr303_config.i2c_dev->i2c_num, ltr303_config.i2c_dev_address, data, sizeof(data), ltr303_config.i2c_dev->i2c_timeout)) {
        xSemaphoreGive(ltr303_config.i2c_dev->lock);
        return -1;
    }

    xSemaphoreGive(ltr303_config.i2c_dev->lock);
    return 0;
}

static int read_register(uint8_t reg, uint8_t *val) {
    if (xSemaphoreTake(ltr303_config.i2c_dev->lock, ltr303_config.i2c_dev->i2c_timeout) != pdTRUE) {
        return -1;
    }

    if (i2c_master_write_to_device(ltr303_config.i2c_dev->i2c_num, ltr303_config.i2c_dev_address, &reg, sizeof(reg), ltr303_config.i2c_dev->i2c_timeout)) {
        xSemaphoreGive(ltr303_config.i2c_dev->lock);
        return -1;
    }

    if (i2c_master_read_from_device(ltr303_config.i2c_dev->i2c_num, ltr303_config.i2c_dev_address, val, sizeof(uint8_t), ltr303_config.i2c_dev->i2c_timeout)) {
        xSemaphoreGive(ltr303_config.i2c_dev->lock);
        return -1;
    }

    xSemaphoreGive(ltr303_config.i2c_dev->lock);
    return 0;
}

static int read_register_16(uint8_t reg, uint16_t *val) {
    uint8_t rx_data[2];

    if (xSemaphoreTake(ltr303_config.i2c_dev->lock, ltr303_config.i2c_dev->i2c_timeout) != pdTRUE) {
        return -1;
    }

    if (i2c_master_write_to_device(ltr303_config.i2c_dev->i2c_num, ltr303_config.i2c_dev_address, &reg, sizeof(reg), ltr303_config.i2c_dev->i2c_timeout)) {
        xSemaphoreGive(ltr303_config.i2c_dev->lock);
        return -1;
    }

    if (i2c_master_read_from_device(ltr303_config.i2c_dev->i2c_num, ltr303_config.i2c_dev_address, rx_data, sizeof(rx_data), ltr303_config.i2c_dev->i2c_timeout)) {
        xSemaphoreGive(ltr303_config.i2c_dev->lock);
        return -1;
    }

    xSemaphoreGive(ltr303_config.i2c_dev->lock);
    *val = (rx_data[1] << 8) | rx_data[0];
    return 0;
}


int ltr303_init(struct i2c_dev_s *i2c_dev) {
	ltr303_config.i2c_dev = i2c_dev;
	ltr303_config.i2c_dev_address = LTR303_I2C_ADDR;
    int err = 0;

    // Set control register: Gain and Active Mode
    err = write_register(LTR303_CONTR, (LTR303_GAIN << 2) | LTR303_ACTIVE);
    if (err != 0) {
        printf("Failed to set control register.\n");
        return -1;
    }

    // Set measurement rate register: Integration time and Measurement Rate
    err = write_register(LTR303_MEAS_RATE, (LTR303_INTEGRATION_TIME << 3) | LTR303_MEASUREMENT_RATE);
    if (err != 0) {
        printf("Failed to set measurement rate register.\n");
        return -1;
    }

    uint8_t id = 0;
    err = read_register(LTR303_PART_ID, &id);
    if (err != 0) {
        printf("Failed to read part ID.\n");
        return -1;
    }

    id >>= 4;
    if (id == 10) {
        printf("LTR303 sensor detected with correct part ID: %u\n", id);
        return 0;
    } else {
        printf("Unknown part ID for LTR303 sensor: %u\n", id);
        return -1;
    }
}

int ltr303_measure_lux(float *lux) {
	if (lux == NULL) {
		printf("Invalid lux pointer.\n");
		return -1;
	}

	uint16_t CH0_data_raw = 0;
	uint16_t CH1_data_raw = 0;

	int err = read_register_16(LTR303_DATA_CH0_0, &CH0_data_raw);
	if (err != 0) {
		printf("Failed to read CH0 data.\n");
		return -1;
	}

	err = read_register_16(LTR303_DATA_CH0_1, &CH1_data_raw);
	if (err != 0) {
		printf("Failed to read CH1 data.\n");
		return -1;
	}

	float ratio = (float) CH1_data_raw
			/ ((float) CH0_data_raw + (float) CH1_data_raw);

	if (ratio < 0.45f) {
		*lux = (1.7743f * (float) CH0_data_raw + 1.1059f * (float) CH1_data_raw)
				/ ltr303_gain[LTR303_GAIN]
				/ ltr303_integration_time[LTR303_INTEGRATION_TIME];
	} else if (ratio < 0.64f) {
		*lux = (4.2785f * (float) CH0_data_raw - 1.9548f * (float) CH1_data_raw)
				/ ltr303_gain[LTR303_GAIN]
				/ ltr303_integration_time[LTR303_INTEGRATION_TIME];
	} else if (ratio < 0.85f) {
		*lux = (0.5926f * (float) CH0_data_raw + 0.1185f * (float) CH1_data_raw)
				/ ltr303_gain[LTR303_GAIN]
				/ ltr303_integration_time[LTR303_INTEGRATION_TIME];
	} else {
		*lux = 0.f;
	}

	return 0;
}
