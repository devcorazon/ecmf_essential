/*
 * sgp40.c
 *
 *  Created on: 8 lug 2023
 *      Author: Daniele Schirosi
 */

#include "sgp40.h"
#include "sensirion_gas_index_algorithm.h"

#include <freertos/task.h>

#include "driver/i2c.h"

///
struct sgp40_config sgp40_config;
struct sgp40_data sgp40_data;

///
static uint8_t sgp40_compute_crc(uint8_t *buf, size_t size) {
    uint8_t crc = SGP40_CRC_INIT;

    for (size_t i = 0u; i < size; i++) {
        crc ^= buf[i];

        for (size_t j = 0; j < 8; j++) {
            crc = crc & 0x80 ? (crc << 1) ^ SGP40_CRC_POLY : crc << 1;
        }
    }

    return crc;
}

static int sgp40_write_command(uint16_t cmd) {
	uint8_t tx_buf[2] = { cmd >> 8, cmd & 0xff };

	if (xSemaphoreTake(sgp40_config.i2c_dev->lock, sgp40_config.i2c_dev->i2c_timeout) != pdTRUE) {
		return -1;
	}

	if (i2c_master_write_to_device(sgp40_config.i2c_dev->i2c_num, sgp40_config.i2c_dev_address, tx_buf, sizeof(tx_buf), sgp40_config.i2c_dev->i2c_timeout)) {
		xSemaphoreGive(sgp40_config.i2c_dev->lock);

		return -1;
	}

	xSemaphoreGive(sgp40_config.i2c_dev->lock);

	return 0;
}

static int sgp40_write_measure_command(float t_amb, float r_hum) {
	uint8_t tx_buf[8];
	uint16_t t_ticks =  (uint16_t)((((t_amb + 45.f) * 65535.f) + 87.f) / 175.f);
	uint16_t rh_ticks = (uint16_t)(((r_hum * 65535.f) + 50.f) / 100.f);

	tx_buf[0] = (SGP40_CMD_MEASURE_RAW >> 8) & 0xff;
	tx_buf[1] = SGP40_CMD_MEASURE_RAW & 0xff;
	tx_buf[2] = (t_ticks >> 8) & 0xff;
	tx_buf[3] = t_ticks & 0xff;
	tx_buf[4] = sgp40_compute_crc(&tx_buf[2], 2);
	tx_buf[5] = (rh_ticks >> 8) & 0xff;
	tx_buf[6] = rh_ticks & 0xff;
	tx_buf[7] = sgp40_compute_crc(&tx_buf[5], 2);


	if (xSemaphoreTake(sgp40_config.i2c_dev->lock, sgp40_config.i2c_dev->i2c_timeout) != pdTRUE) {
		return -1;
	}

	if (i2c_master_write_to_device(sgp40_config.i2c_dev->i2c_num, sgp40_config.i2c_dev_address, tx_buf, sizeof(tx_buf), sgp40_config.i2c_dev->i2c_timeout)) {
		xSemaphoreGive(sgp40_config.i2c_dev->lock);

		return -1;
	}

	xSemaphoreGive(sgp40_config.i2c_dev->lock);

	return 0;
}

static int sgp40_read_sample(void) {
	uint8_t rx_buf[3];
	uint16_t voc_raw_sample;

	if (xSemaphoreTake(sgp40_config.i2c_dev->lock, sgp40_config.i2c_dev->i2c_timeout) != pdTRUE) {
		return -1;
	}

	if (i2c_master_read_from_device(sgp40_config.i2c_dev->i2c_num, sgp40_config.i2c_dev_address, rx_buf, sizeof(rx_buf), sgp40_config.i2c_dev->i2c_timeout)) {
		xSemaphoreGive(sgp40_config.i2c_dev->lock);

		return -1;
	}

	xSemaphoreGive(sgp40_config.i2c_dev->lock);

	voc_raw_sample = ((uint16_t)rx_buf[0] << 8) | (uint16_t)rx_buf[1];

	if (sgp40_compute_crc(&rx_buf[0], 2) != rx_buf[2]) {
		printf("sgp40_compute_crc - error\r\n");
		return -1;
	}

	sgp40_data.voc_raw_sample = voc_raw_sample;

	return 0;
}

int sgp40_init(struct i2c_dev_s *i2c_dev) {
	sgp40_config.i2c_dev = i2c_dev;
	sgp40_config.i2c_dev_address = SGP40_I2C_ADDRESS;

	if (sgp40_write_command(SGP40_CMD_HEATER_OFF)) {
		return -1;
	}

	vTaskDelay(pdMS_TO_TICKS(10u));

	GasIndexAlgorithm_init(&sgp40_data.engine_algorithm_params, GasIndexAlgorithm_ALGORITHM_TYPE_VOC);

	return 0;
}

int sgp40_sample(float t_amb, float r_hum, uint16_t *voc_idx) {
	*voc_idx = GAS_U_INVALID;

	if (sgp40_write_measure_command(t_amb, r_hum)) {
//		sgp40_write_command(SGP40_CMD_HEATER_OFF);

		return -1;
	}

	vTaskDelay(pdMS_TO_TICKS(SGP40_MEASURE_WAIT_MS));

	if (sgp40_read_sample()) {
//		sgp40_write_command(SGP40_CMD_HEATER_OFF);

		return -1;
	}

//	sgp40_write_command(SGP40_CMD_HEATER_OFF);

	GasIndexAlgorithm_process(&sgp40_data.engine_algorithm_params, (int32_t)sgp40_data.voc_raw_sample, (int32_t *)voc_idx);

	return 0;
}
