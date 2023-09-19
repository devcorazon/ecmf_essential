/*
 * sht4x.c
 *
 *  Created on: 8 lug 2023
 *      Author: Daniele Schirosi
 */

#include "sht4x.h"

#include <freertos/task.h>

#include "driver/i2c.h"

///
struct sht4x_config sht4x_config;
struct sht4x_data sht4x_data;

///
static uint8_t sht4x_compute_crc(uint8_t *buf, size_t size) {
    uint8_t crc = SHT4X_CRC_INIT;

    for (size_t i = 0u; i < size; i++) {
        crc ^= buf[i];

        for (size_t j = 0; j < 8; j++) {
            crc = crc & 0x80 ? (crc << 1) ^ SHT4X_CRC_POLY : crc << 1;
        }
    }

    return crc;
}

static int sht4x_write_command(uint8_t cmd) {
	uint8_t tx_buf[1] = { cmd };

	if (xSemaphoreTake(sht4x_config.i2c_dev->lock, sht4x_config.i2c_dev->i2c_timeout) != pdTRUE) {

		return -1;
	}

	if (i2c_master_write_to_device(sht4x_config.i2c_dev->i2c_num, sht4x_config.i2c_dev_address, tx_buf, sizeof(tx_buf), sht4x_config.i2c_dev->i2c_timeout) != ESP_OK) {
		xSemaphoreGive(sht4x_config.i2c_dev->lock);

		return -1;
	}

	xSemaphoreGive(sht4x_config.i2c_dev->lock);

	return 0;
}

static int sht4x_read_sample(void) {
	uint16_t t_sample;
	uint16_t rh_sample;
	uint8_t rx_buf[6];

	if (xSemaphoreTake(sht4x_config.i2c_dev->lock, sht4x_config.i2c_dev->i2c_timeout) != pdTRUE) {
		return -1;
	}

	if (i2c_master_read_from_device(sht4x_config.i2c_dev->i2c_num, sht4x_config.i2c_dev_address, rx_buf, sizeof(rx_buf), sht4x_config.i2c_dev->i2c_timeout)) {
		xSemaphoreGive(sht4x_config.i2c_dev->lock);

		return -1;
	}

	xSemaphoreGive(sht4x_config.i2c_dev->lock);

	t_sample = ((uint16_t)rx_buf[0] << 8) | (uint16_t)rx_buf[1];
	rh_sample = ((uint16_t)rx_buf[3] << 8) | (uint16_t)rx_buf[4];

	if ((sht4x_compute_crc(&rx_buf[0], 2) != rx_buf[2]) || (sht4x_compute_crc(&rx_buf[3], 2) != rx_buf[5])) {
		return -1;
	}

	sht4x_data.t_sample = t_sample;
	sht4x_data.rh_sample = rh_sample;

	return 0;
}

int sht4x_init(struct i2c_dev_s *i2c_dev) {
	sht4x_config.i2c_dev = i2c_dev;
	sht4x_config.i2c_dev_address = SHT4X_I2C_ADDRESS;

	if (sht4x_write_command(SHT4X_CMD_RESET)) {
		return -1;
	}

	vTaskDelay(pdMS_TO_TICKS(50u));

	return 0;
}

int sht4x_sample(float *t_amb, float *r_hum) {
	*t_amb = TEMP_F_INVALID;
	*r_hum = HUM_F_INVALID;

	if (sht4x_write_command(SHT4X_CMD_MEASURE_HIGH)) {
		return -1;
	}

	vTaskDelay(pdMS_TO_TICKS(SHT4X_MEASURE_WAIT_MS));

	if (sht4x_read_sample()) {
		return -1;
	}

	*t_amb = sht4x_data.t_sample * 175.0f / 65535.0f - 45.0f;
	*r_hum = sht4x_data.rh_sample * 125.0f / 65535.0f - 6.0f;

	return 0;
}
