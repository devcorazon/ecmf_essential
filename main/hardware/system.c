/*
 * system.c
 *
 *  Created on: 8 lug 2023
 *      Author: Daniele Schirosi
 */

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include "driver/i2c.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_rom_gpio.h"

#include "board.h"
#include "system.h"
#include "storage_internal.h"
#include "test.h"
#include "sensor.h"
#include "rgb_led.h"
#include "fan.h"
#include "ir_receiver.h"
#include "blufi.h"
#include "user_experience.h"

#include "esp_efuse.h"
#include "esp_efuse_table.h"
#include "esp_efuse_custom_table.h"

#include "../feature/controller.c"

typedef struct {
    uint8_t unlocked;
    uint32_t serial_number;
} device_desc_t;

///
static struct i2c_dev_s i2c_dev;
static struct adc_dev_s adc_dev;

///
static int i2c_init(void) {
	i2c_dev.i2c_num = I2C_MASTER_NUM;

	i2c_dev.i2c_conf.mode = I2C_MODE_MASTER;
	i2c_dev.i2c_conf.sda_io_num = I2C_MASTER_SDA_IO;
	i2c_dev.i2c_conf.scl_io_num = I2C_MASTER_SCL_IO;
	i2c_dev.i2c_conf.sda_pullup_en = GPIO_PULLUP_DISABLE;
	i2c_dev.i2c_conf.scl_pullup_en = GPIO_PULLUP_DISABLE;
	i2c_dev.i2c_conf.master.clk_speed = I2C_MASTER_FREQ_HZ;

	i2c_dev.i2c_timeout = pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS);

    i2c_param_config(I2C_MASTER_NUM, &i2c_dev.i2c_conf);
    i2c_driver_install(I2C_MASTER_NUM, i2c_dev.i2c_conf.mode, 0, 0, 0);

    i2c_dev.lock = xSemaphoreCreateMutex();

    return 0;
}

static int adc_init(void) {
    //-------------ADC Init---------------//
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
    };
    adc_oneshot_new_unit(&init_config, &adc_dev.adc_handle);

    //-------------ADC Config---------------//
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_11,
    };
    adc_oneshot_config_channel(adc_dev.adc_handle, ADC_CHANNEL_3, &config);

    //-------------ADC Calibration Init---------------//
    adc_cali_curve_fitting_config_t cali_config = {
			.unit_id = ADC_UNIT_1,
			.atten = ADC_ATTEN_DB_11,
			.bitwidth = ADC_BITWIDTH_DEFAULT,
    };
	adc_cali_create_scheme_curve_fitting(&cali_config, &adc_dev.adc_cali_handle);

	adc_dev.adc_channel = ADC_CHANNEL_3;

    return 0;
}

static int pwm_init(void) {
    // Prepare and then apply the LEDC PWM timer configuration
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_13_BIT,
        .freq_hz = FAN_FREQUENCE,
    };
    ledc_timer_config(&ledc_timer);

    // Prepare and then apply the LEDC PWM channel configuration
    ledc_channel_config_t pwm_config = {
        .gpio_num = FAN_SPEED_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = FAN_PWM_PULSE_SPEED_NONE_IN,
    };
    ledc_channel_config(&pwm_config);

    esp_rom_gpio_pad_select_gpio(FAN_DIRECTION_PIN);
    gpio_set_direction(FAN_DIRECTION_PIN, GPIO_MODE_OUTPUT);

    return 0;
}

int system_init(void) {
	storage_init();

	adc_init();
	i2c_init();
	pwm_init();

	test_init();
	sensor_init(&i2c_dev, &adc_dev);
	rgb_led_init(&i2c_dev);
	fan_init();
	ir_receiver_init();
	controller_init();
	user_experience_init();
	blufi_ble_init();
	blufi_wifi_init();

	uint8_t ssid[SSID_SIZE + 1] = {0};
	get_ssid(ssid);
	printf("SSID: %s\n", ssid);

	uint8_t pw[PASSWORD_SIZE + 1] = {0};
	get_password(pw);
	printf("PW: %s\n", pw);

	uint8_t server[SERVER_SIZE + 1] = {0};
	get_server(server);
	printf("Server: %s\n", server);

	uint8_t port[PORT_SIZE + 1] = {0};
	get_port(port);
	printf("Port: %s\n", port);

	uint8_t ota_url[OTA_URL_SIZE + 1] = {0};
	get_ota_url(ota_url);
	printf("OTA URL: %s\n", ota_url);

	printf("WIFI Active: %s\n", get_wifi_active() ? "Yes" : "No");

//	printf("mode_set: %d - speed_set: %d - r_hum_set: %d - lux_set: %d - voc_set: %d - temp_offset: %d - r_hum_offset: %d - filter_operating: %d\r\n",
//	    get_mode_set(),
//	    get_speed_set(),
//	    get_relative_humidity_set(),
//	    get_lux_set(),
//	    get_voc_set(),
//	    get_temperature_offset(),
//	    get_relative_humidity_offset(),
//	    get_filter_operating()
//		);

	return 0;

}
