/*
 * system.h
 *
 *  Created on: 8 lug 2023
 *      Author: Daniele Schirosi
 */

#ifndef MAIN_INCLUDE_SYSTEM_H_
#define MAIN_INCLUDE_SYSTEM_H_

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "driver/i2c.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"

#include <stdio.h>
#include <stdarg.h>

#define DEBUG_MODE 1 // Set to 1 for debug mode, 0 to disable

#if DEBUG_MODE
    static inline void custom_printf(const char* format, ...) {
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
    }
    #define printf(...) custom_printf(__VA_ARGS__)
#else
    #define printf(...) (void)0
#endif

//#define FW_VERSION_MAJOR 				0
//#define FW_VERSION_MINOR 				0
//#define FW_VERSION_PATCH 				2

#define FIRMWARE_VERSION				(FW_VERSION_MAJOR << 12) + (FW_VERSION_MINOR << 6) + FW_VERSION_PATCH

#define POWER_MAIN_12V

#ifdef POWER_MAIN_12V
#define NTC_ADC_VIN						(3300ul)
#else
#define NTC_ADC_VIN						(3000ul)
#endif

#define NTC_ADC_LEG_RESISTANCE			(66500ul)

#define ARRAY_SIZE(array) 				(sizeof(array) / sizeof((array)[0]))

#define NTC_ADC_NB_SAMPLE				(32u)

#define TEMP_F_INVALID					65535.f
#define HUM_F_INVALID					65535.f
#define LUX_F_INVALID                   65535.f
#define GAS_U_INVALID					65535u

#define PRE_FLASH_PERIOD_MS				1024
#define FLASH_PERIOD					8
#define POST_FLASH_PERIOD_MS			2048

#define FLASH_FALL						6
#define FLASH_PWM_WITH_FALL				4

#define FLASH_PWM_NO_FALL				31
#define FLASH_PWM_MAX					255

#define FLASH_PERIOD_POWER_ON			4
#define FLASH_PERIOD_POWER_OFF			6

#define FLASH_PERIOD_SPEED_NIGHT		14
#define FLASH_PERIOD_SPEED_BLINK		4
#define FLASH_PWM_SPEED_BLINK			59


struct i2c_dev_s {
	i2c_port_t i2c_num;
	i2c_config_t i2c_conf;
	TickType_t i2c_timeout;
	SemaphoreHandle_t lock;
};

struct adc_dev_s {
	adc_channel_t adc_channel;
	adc_oneshot_unit_handle_t adc_handle;
	adc_cali_handle_t adc_cali_handle;
	SemaphoreHandle_t lock;
};

struct ntc_shape_s {
   uint32_t resistance;
   int16_t temperature;
};

int system_init(void);


#endif /* MAIN_INCLUDE_SYSTEM_H_ */
