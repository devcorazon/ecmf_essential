/*
 * sensor.c
 *
 *  Created on: 8 lug 2023
 *      Author: Daniele Schirosi
 */

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include "driver/i2c.h"
#include "driver/temperature_sensor.h"

#include "board.h"
#include "system.h"
#include "storage_internal.h"
#include "sht4x.h"
#include "sgp40.h"
#include "ltr303.h"
#include "rgb_led.h"
#include "test.h"

///
#define	SENSOR_TASK_STACK_SIZE			(configMINIMAL_STACK_SIZE * 4)
#define	SENSOR_TASK_PRIORITY			(1)
#define	SENSOR_TASK_PERIOD				(1000ul / portTICK_PERIOD_MS)

#define NTC_ADC_TEMPERATURE_SCALE		(100)

struct adc_dev_s adc_dev;

static const struct ntc_shape_s ntc_convert[] = {
   {235800, -40 * NTC_ADC_TEMPERATURE_SCALE},
   {173900, -35 * NTC_ADC_TEMPERATURE_SCALE},
   {129900, -30 * NTC_ADC_TEMPERATURE_SCALE},
   {98180, -25 * NTC_ADC_TEMPERATURE_SCALE},
   {75020, -20 * NTC_ADC_TEMPERATURE_SCALE},
   {57930, -15 * NTC_ADC_TEMPERATURE_SCALE},
   {45170, -10 * NTC_ADC_TEMPERATURE_SCALE},
   {35550, -5  * NTC_ADC_TEMPERATURE_SCALE},
   {28200,  0},
   {22600,  5  * NTC_ADC_TEMPERATURE_SCALE},
   {18230,  10 * NTC_ADC_TEMPERATURE_SCALE},
   {14820,  15 * NTC_ADC_TEMPERATURE_SCALE},
   {12130,  20 * NTC_ADC_TEMPERATURE_SCALE},
   {10000,  25 * NTC_ADC_TEMPERATURE_SCALE},
   {8295,  30 * NTC_ADC_TEMPERATURE_SCALE},
   {6922,  35 * NTC_ADC_TEMPERATURE_SCALE},
   {5810,  40 * NTC_ADC_TEMPERATURE_SCALE},
   {4903,  45 * NTC_ADC_TEMPERATURE_SCALE},
   {4160,  50 * NTC_ADC_TEMPERATURE_SCALE}
};

///
struct t_sensor_config_t {
	temperature_sensor_handle_t handle;
	temperature_sensor_config_t config;
};

static struct t_sensor_config_t t_sensor_config = { NULL, TEMPERATURE_SENSOR_CONFIG_DEFAULT(-10, 80) };

#define T_SENS_NB_SAMPLE	16u

#define T_SENS_POOL_SIZE	30u
#define T_SENS_INVALID		65535.f

static float t_sens_pool[T_SENS_POOL_SIZE] = { [0 ... T_SENS_POOL_SIZE - 1] = T_SENS_INVALID };
static size_t t_sens_idx = 0u;

///
int temperature_sensor_init(void);
int temperature_sensor_sample_get(float *t_sens);

static void add_t_sens_to_pool(float t_sens);
static float calculate_t_sens_avg(void);

///
static int sensor_i2c_binding(struct i2c_dev_s *i2c_dev) {
	sht4x_init(i2c_dev);
	sgp40_init(i2c_dev);
	ltr303_init(i2c_dev);

	return 0;
}

static int sensor_adc_binding(struct adc_dev_s *adc_d) {
	adc_dev.adc_channel = adc_d->adc_channel;
	adc_dev.adc_handle = adc_d->adc_handle;
	adc_dev.adc_cali_handle = adc_d->adc_cali_handle;
	adc_dev.lock = adc_d->lock;

	return 0;
}

int sensor_ntc_sample(float *temp) {
	int sample_raw;
	long sum_samples;
	int voltage_val;
	uint32_t tmp;
	size_t i;

	*temp = TEMP_F_INVALID;

	for (i = 0u, sum_samples = 0; i < NTC_ADC_NB_SAMPLE; i++) {
		adc_oneshot_read(adc_dev.adc_handle, adc_dev.adc_channel, &sample_raw);
		sum_samples += sample_raw;
	}

	adc_cali_raw_to_voltage(adc_dev.adc_cali_handle, sum_samples / NTC_ADC_NB_SAMPLE, &voltage_val);
//	printf("Voltage: %u\r\n", voltage_val);

	if ( voltage_val == NTC_ADC_VIN){
		return -1;
	}
	tmp = voltage_val * NTC_ADC_LEG_RESISTANCE;
	tmp /= NTC_ADC_VIN - voltage_val;

	for (i = 0u; i < ARRAY_SIZE(ntc_convert); i++) {
		if (ntc_convert[i].resistance <= tmp)
			break;
	}

	if (i == 0u) {
		*temp = ntc_convert[i].temperature;
	} else if (i == ARRAY_SIZE(ntc_convert)) {
		*temp = ntc_convert[ARRAY_SIZE(ntc_convert) - 1].temperature;
	} else {
		*temp = ntc_convert[i - 1].temperature
				+ (((int32_t)(tmp - ntc_convert[i - 1].resistance) * (ntc_convert[i - 1].temperature - ntc_convert[i].temperature))
						/ (int32_t)(ntc_convert[i - 1].resistance - ntc_convert[i].resistance));
	}

	*temp /= NTC_ADC_TEMPERATURE_SCALE;

	return 0;
}

static void sensor_task(void *pvParameters) {
	TickType_t sensor_task_time;
	float t_amb;
	float r_hum;
	float temp;
	float lux;
	uint16_t voc_idx;
//	float t_sens;

	sensor_task_time = xTaskGetTickCount();

	while(true) {
		if (!sht4x_sample(&t_amb, &r_hum)) {
			t_amb += (float)TEMPERATURE_OFFSET_FIXED / (float)TEMPERATURE_SCALE;
			t_amb += (float)get_temperature_offset() / (float)TEMPERATURE_SCALE;

			r_hum += (float)RELATIVE_HUMIDITY_OFFSET_FIXED / (float)RELATIVE_HUMIDITY_SCALE;
			r_hum += (float)get_relative_humidity_offset() / (float)RELATIVE_HUMIDITY_SCALE;
		}
		if (get_direction_state() != DIRECTION_IN) {
			set_temperature(SET_VALUE_TO_TEMP_RAW(t_amb));
			set_relative_humidity(SET_VALUE_TO_RH_RAW(r_hum));
		}


		sgp40_sample(t_amb, r_hum, &voc_idx);
		if (get_direction_state() != DIRECTION_IN){
			set_voc(voc_idx);
		}

		ltr303_measure_lux(&lux);
		if (!rgb_led_is_on()) {
			set_lux(SET_VALUE_TO_LUX_RAW(lux));
		}

		sensor_ntc_sample(&temp);
		if (get_direction_state() == DIRECTION_OUT){
			set_internal_temperature(SET_VALUE_TO_TEMP_RAW(temp));
		} else if (get_direction_state() == DIRECTION_IN){
			set_external_temperature(SET_VALUE_TO_TEMP_RAW(temp));
		}

//		temperature_sensor_sample_get(&t_sens);
//		add_t_sens_to_pool(t_sens);
//		printf("t_sens: %.1f - t_sens_avg: %.1f\r\n", t_sens, calculate_t_sens_avg());

//		printf("t_amb: %.1f - r_hum: %.1f - voc_idx: %u - temp: %.1f - lux: %.1f\r\n", t_amb, r_hum, voc_idx, temp, lux);

		vTaskDelayUntil(&sensor_task_time, SENSOR_TASK_PERIOD);
	}
}

///
int sensor_init(struct i2c_dev_s *i2c_dev, struct adc_dev_s *adc_dev) {

	sensor_i2c_binding(i2c_dev);
	sensor_adc_binding(adc_dev);

	temperature_sensor_init();

	BaseType_t task_created = xTaskCreate(sensor_task, "sensor_task", SENSOR_TASK_STACK_SIZE, NULL, SENSOR_TASK_PRIORITY, NULL);

	return task_created == pdPASS ? 0 : -1;
}

int temperature_sensor_init(void) {
	esp_err_t ret;

	ret = temperature_sensor_install(&t_sensor_config.config, &t_sensor_config.handle);
	if (ret != ESP_OK) {
		printf("temperature_sensor_install - error\r\n");

		return -1;
	}

	return 0;
}

int temperature_sensor_sample_get(float *t_sens) {
	esp_err_t ret;
	float t_sens_sum = 0.f;
	size_t t_sens_count = 0u;

	*t_sens = T_SENS_INVALID;

	ret = temperature_sensor_enable(t_sensor_config.handle);
	if (ret != ESP_OK) {
		printf("temperature_sensor_enable - error\r\n");

		return -1;
	}

	for (size_t i = 0u; i < T_SENS_NB_SAMPLE; i++) {
		if (temperature_sensor_get_celsius(t_sensor_config.handle, t_sens) == ESP_OK) {
			t_sens_sum += *t_sens;
			t_sens_count++;
		}
	}

	ret = temperature_sensor_disable(t_sensor_config.handle);
	if (ret != ESP_OK) {
		printf("temperature_sensor_disable - error\r\n");

		return -1;
	}

	*t_sens = (float)(t_sens_sum / (double)t_sens_count);

	return 0;
}

static void add_t_sens_to_pool(float t_sens) {
	t_sens_pool[t_sens_idx] = t_sens;

	t_sens_idx += 1u;
	t_sens_idx %= T_SENS_POOL_SIZE;
}

static float calculate_t_sens_avg(void) {
	double t_sens_sum = 0.f;
	size_t t_sens_count = 0u;

	for (size_t i = 0u; i < T_SENS_POOL_SIZE; i++) {
		if (t_sens_pool[i] != T_SENS_INVALID) {
			t_sens_sum += (double)t_sens_pool[i];
			t_sens_count++;
		}
	}

	return (float)(t_sens_sum / (double)t_sens_count);
}
