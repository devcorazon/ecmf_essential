/*
 * test.c
 *
 *  Created on: 8 lug 2023
 *      Author: Daniele Schirosi
 */

#include "test.h"
#include "stdbool.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include "esp_console.h"

#include "board.h"
#include "system.h"
#include "storage_internal.h"
#include "sensor.h"
#include "rgb_led.h"
#include "fan.h"
#include "blufi.h"

///
static esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
static esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
static esp_console_repl_t *repl = NULL;

static int testing_in_progress = false;
///
static int test_in_progress_set()
{
	return testing_in_progress = true;
}
static int test_in_progress_reset()
{
	return testing_in_progress = false;
}

int test_in_progress(){
	return testing_in_progress;
}

static int cmd_set_mode_speed_func(int argc, char** argv) {

    if(argc < 3)
    {
        printf("Invalid arguments. Usage: set_ms <index 1> <index 2>\n");
        return -1;
    }

	char *endptr;

	uint8_t mode_set = (uint8_t)strtol(argv[1], &endptr, 10);
	uint8_t speed_set = (uint8_t)strtol(argv[2], &endptr, 10);

    // Check for conversion errors
//    if (endptr == argv[1] || *endptr != '\0' || mode_set < 0 || mode_set > 5 || speed_set < 0 || speed_set > 4) {
//        printf("Invalid index. Supported values mode_set = {0,1,2,3,4,5}  speed_set = {0,1,2,3,4} \n");
//        return -1;
//    }

	if ( mode_set < 5 ){
		set_mode_state(get_mode_state() & ~MODE_AUTOMATIC_CYCLE_EXTRA_CYCLE);  // disable extra cycle
	}
	else {
		mode_set = 4;
		set_mode_state(get_mode_state() | MODE_AUTOMATIC_CYCLE_EXTRA_CYCLE);  // Enable extra cycle
	}
	set_mode_set(mode_set);
	set_speed_set(speed_set);

	return 0;
}

static int cmd_set_factory_settings_func(int argc, char** argv) {
	storage_set_default();

	return 0;
}

static int cmd_test_led_func(int argc, char** argv) {

    uint8_t mode;
    uint8_t color;

    if(argc < 2)
    {
        printf("Invalid arguments. Usage: test_led  <index>\n");
        return -1;
    }

    char *endptr;
    long index = strtol(argv[1], &endptr, 10); // Base 10 is used.

    // Check for conversion errors
    if (endptr == argv[1] || *endptr != '\0' || index < 0 || index > 3)
    {
        printf("Invalid index. Supported colors: 0, 1, 2, 3\n");
        return -1;
    }

    switch (index)
    {
        case 0:
            color = RGB_LED_COLOR_NONE;
            mode = RGB_LED_MODE_OFF;
            break;
        case 1:
            color = RGB_LED_COLOR_RED;
            mode = RGB_LED_MODE_ON;
            break;
        case 2:
            color = RGB_LED_COLOR_GREEN;
            mode = RGB_LED_MODE_ON;
            break;
        case 3:
            color = RGB_LED_COLOR_BLUE;
            mode = RGB_LED_MODE_ON;
            break;
        default: // This should never happen
            printf("Invalid index. Supported colors: 0, 1, 2, 3\n");
            return -1;
    }

    //rgb_led_blink(color,4000,200); // Use this function to test blink ; make sure that duration > period
    rgb_led_set(color,mode);

	return 0;
}

static int cmd_test_all_func(int argc, char** argv) {

	uint32_t serial_number = get_serial_number();

    printf("Serial Number: %08lx\n", serial_number);

    printf("Firmware version: v%d.%d.%d\n", FW_VERSION_MAJOR,FW_VERSION_MINOR,FW_VERSION_PATCH);

    int16_t temp = get_temperature();
    if (temp == INT16_MAX)
    {
        printf("Temperature reading error\n");
    }
    else
    {
        printf("Temperature =  %d.%01d C\n", TEMP_RAW_TO_INT(temp), TEMP_RAW_TO_DEC(temp));
    }

    uint16_t rh = get_relative_humidity();
    if (rh == UINT16_MAX)
    {
        printf("Relative humidity reading error\n");
    }
    else
    {
        printf("Relative humidity =  %u.%01u %%\n", RH_RAW_TO_INT(rh), RH_RAW_TO_DEC(rh));
    }

    int32_t voc = get_voc();
    if (voc == UINT16_MAX)
    {
        printf("VOC Index reading error\n");
    }
    else
    {
        printf("VOC Index =  %ld \n", voc);
    }

    int16_t lux = get_lux();
    if (lux == INT16_MAX)
    {
        printf("LUX reading error\n");
    }
    else
    {
        printf("LUX =  %u.%01u %%\n", LUX_RAW_TO_INT(lux), LUX_RAW_TO_DEC(lux));
    }

    float ntc_temp;
    sensor_ntc_sample(&ntc_temp);
    if (ntc_temp == INT16_MAX)
    {
        printf("NTC Temperature reading error\n");
    }
    else
    {
        printf("NTC Temperature =  %d.%01d C\n", TEMP_RAW_TO_INT(ntc_temp), TEMP_RAW_TO_DEC(ntc_temp));
    }

	return 0;
}

static int cmd_test_fan_func(int argc, char** argv) {

    uint8_t speed;
    uint8_t direction;

    if(argc < 2)
    {
        printf("Invalid arguments. Usage: test_fan <index>\n");
        return -1;
    }

    char *endptr;
    long index = strtol(argv[1], &endptr, 10); // Base 10 is used.

    // Check for conversion errors
    if (endptr == argv[1] || *endptr != '\0' || index < 0 || index > 9) {
        printf("Invalid index. Supported fan speeds: 0, 1, 2, 3, 4, 5, 6, 7, 8, 9\n");
        return -1;
    }

    switch (index)
    {
        case 0:
            speed = SPEED_NONE;
            direction = DIRECTION_NONE;
            break;
        case 1:
            speed = SPEED_NIGHT;
            direction = DIRECTION_IN;
            break;
        case 2:
            speed = SPEED_LOW;
            direction = DIRECTION_IN;
            break;
        case 3:
            speed = SPEED_MEDIUM;
            direction = DIRECTION_IN;
            break;
        case 4:
            speed = SPEED_HIGH;
            direction = DIRECTION_IN;
            break;
        case 5:
            speed = SPEED_NIGHT;
            direction = DIRECTION_OUT;
            break;
        case 6:
            speed = SPEED_LOW;
            direction = DIRECTION_OUT;
            break;
        case 7:
            speed = SPEED_MEDIUM;
            direction = DIRECTION_OUT;
            break;
        case 8:
            speed = SPEED_HIGH;
            direction = DIRECTION_OUT;
            break;
        case 9:
            speed = SPEED_BOOST;
            direction = DIRECTION_OUT;
            break;
        default: // This should never happen
            printf("Invalid index. Supported fan speeds: 0, 1, 2, 3, 4, 5, 6, 7, 8, 9\n");
            return -1;
    }

    fan_set(direction, speed);

	return 0;
}

static int cmd_test_start_func(int argc, char** argv) {
	test_in_progress_set();
	blufi_init();
	return 0;
}

static int cmd_test_stop_func(int argc, char** argv) {
	test_in_progress_reset();
	blufi_deinit();
	return 0;
}

///
int test_init(void) {
	esp_console_register_help_command();
	esp_console_new_repl_uart(&hw_config, &repl_config, &repl);
	esp_console_start_repl(repl);

	const esp_console_cmd_t cmd_set_mode_speed = {
		.command = "set_ms",
		.help = "set mode and speed",
		.hint = NULL,
		.func = cmd_set_mode_speed_func,
	};

	esp_console_cmd_register(&cmd_set_mode_speed);

	const esp_console_cmd_t cmd_set_factory_settings = {
		.command = "set_factory",
		.help = "set factory settings",
		.hint = NULL,
		.func = cmd_set_factory_settings_func,
	};

	esp_console_cmd_register(&cmd_set_factory_settings);

	const esp_console_cmd_t cmd_test_led = {
		   .command = "test_led",
		   .help = "Test LED {index}",
		   .hint = NULL,
		   .func = cmd_test_led_func,
		 };

	esp_console_cmd_register(&cmd_test_led);

    const esp_console_cmd_t cmd_test_all = {
           .command = "test_all",
           .help = "Test Sensor",
           .hint = NULL,
           .func = cmd_test_all_func,
         };

	esp_console_cmd_register(&cmd_test_all);

	 const esp_console_cmd_t cmd_test_fan = {
	       .command = "test_fan",
	       .help = "Test Fan {index}",
	       .hint = NULL,
	       .func = cmd_test_fan_func,
	     };

	 esp_console_cmd_register(&cmd_test_fan);

	 const esp_console_cmd_t cmd_test_start = {
	       .command = "test_start",
	       .help = "Test start",
	       .hint = NULL,
	       .func = cmd_test_start_func,
	     };

	 esp_console_cmd_register(&cmd_test_start);

	 const esp_console_cmd_t cmd_test_stop = {
	       .command = "test_stop",
	       .help = "Test stop",
	       .hint = NULL,
	       .func = cmd_test_stop_func,
	     };

	 esp_console_cmd_register(&cmd_test_stop);

	return 0;
}
