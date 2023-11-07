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
#include "sht4x.h"
#include "sgp40.h"
#include "ltr303.h"

///
static esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
static esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
static esp_console_repl_t *repl = NULL;

static int testing_in_progress = false;
///
static int test_in_progress_set() {
	return testing_in_progress = true;
}

static int test_in_progress_reset() {
	return testing_in_progress = false;
}

int test_in_progress() {
	return testing_in_progress;
}

static int cmd_set_mode_speed_func(int argc, char **argv) {

	if (argc < 3) {
		printf("Invalid arguments. Usage: set_ms <index 1> <index 2>\n");
		return -1;
	}

	char *endptr;

	int8_t mode_set = (uint8_t) strtol(argv[1], &endptr, 10);
	int8_t speed_set = (uint8_t) strtol(argv[2], &endptr, 10);

	// Check for conversion errors
	if (endptr == argv[1] || *endptr != '\0' || mode_set < 0 || mode_set > 5 || speed_set < 0 || speed_set > 4) {
		printf(
				"Invalid index. Supported values mode_set = {0,1,2,3,4,5}  speed_set = {0,1,2,3,4} \n");
		return -1;
	}

	set_mode_set(mode_set);
	set_speed_set(speed_set);

	return 0;
}

static int cmd_set_factory_settings_func(int argc, char** argv) {
	storage_set_default();
	esp_restart();

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

	if ( test_in_progress() == false){
		printf("Make sure to run test_start \n");
	}
	else {
		switch (index)
		{
		case 0:
			color = RGB_LED_COLOR_NONE;
			mode = RGB_LED_MODE_NONE;
			break;
		case 1:
			color = RGB_LED_COLOR_RED;
			mode = RGB_LED_MODE_ALWAYS_ON;
			break;
		case 2:
			color = RGB_LED_COLOR_GREEN;
			mode = RGB_LED_MODE_ALWAYS_ON;
			break;
		case 3:
			color = RGB_LED_COLOR_BLUE;
			mode = RGB_LED_MODE_ALWAYS_ON;
			break;
		default: // This should never happen
		printf("Invalid index. Supported colors: 0, 1, 2, 3\n");
		return -1;
		}

		rgb_led_set(color,mode);
	}
	return 0;
}

static int cmd_test_all_func(int argc, char **argv) {

	float lux;
	float ntc_temp;

	if (test_in_progress() == false) {
		printf("Make sure to run test_start \n");
	}
	else {

		uint32_t serial_number = get_serial_number();

		printf("Serial Number: %08lx\n", serial_number);

		printf("Firmware version: v%d.%d.%d\n", FW_VERSION_MAJOR, FW_VERSION_MINOR, FW_VERSION_PATCH);

		int16_t temp = get_temperature();
		if (temp == TEMPERATURE_INVALID) {
			printf("Temperature reading error\n");
		}
		else {
			printf("Temperature =  %d.%01d C\n", TEMP_RAW_TO_INT(temp), TEMP_RAW_TO_DEC(temp));
		}

		uint16_t rh = get_relative_humidity();
		if (rh == RELATIVE_HUMIDITY_INVALID) {
			printf("Relative humidity reading error\n");
		}
		else {
			printf("Relative humidity =  %u.%01u %%\n", RH_RAW_TO_INT(rh), RH_RAW_TO_DEC(rh));
		}

	    uint16_t voc = get_voc();
	    if (voc == VOC_INVALID)
	    {
	        printf("VOC Index reading error\n");
	    }
	    else {
	        printf("VOC Index =  %u4 \n", voc);
	    }

		ltr303_measure_lux(&lux);
		uint16_t u16_lux = SET_VALUE_TO_TEMP_RAW(lux);

		if (u16_lux == LUX_INVALID) {
			printf("LUX reading error\n");
		}
		else {
			printf("LUX =  %u.%01u %%\n", TEMP_RAW_TO_INT(u16_lux), TEMP_RAW_TO_DEC(u16_lux));
		}

		sensor_ntc_sample(&ntc_temp);
		int16_t i16_ntc_temp = SET_VALUE_TO_TEMP_RAW(ntc_temp);

		if (i16_ntc_temp == TEMPERATURE_INVALID) {
			printf("NTC Temperature reading error\n");
		}
		else {
			printf("NTC Temperature =  %d.%01d C\n", TEMP_RAW_TO_INT(i16_ntc_temp), TEMP_RAW_TO_DEC(i16_ntc_temp));
		}
	}

	return 0;
}

static int cmd_test_fan_func(int argc, char **argv) {

	uint8_t speed;
	uint8_t direction;

	if (argc < 2) {
		printf("Invalid arguments. Usage: test_fan <index>\n");
		return -1;
	}

	char *endptr;
	long index = strtol(argv[1], &endptr, 10); // Base 10 is used.

	// Check for conversion errors
	if (endptr == argv[1] || *endptr != '\0' || index < 0 || index > 9) {
		printf(
				"Invalid index. Supported fan speeds: 0, 1, 2, 3, 4, 5, 6, 7, 8, 9\n");
		return -1;
	}

	if (test_in_progress() == false) {
		printf("Make sure to run test_start \n");
	} else {

		switch (index) {
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
	}
	return 0;
}

static int cmd_test_start_func(int argc, char **argv) {
	if (test_in_progress() == true) {
		printf("Run test_stop and start again! \n");
	} else {
		blufi_ble_init();
		test_in_progress_set();
		blufi_adv_start();
		blufi_ap_start();
	}
	return 0;
}

static int cmd_test_stop_func(int argc, char **argv) {
	if (test_in_progress() == false) {
		printf("test already in stop! \n");
	} else {
		blufi_ble_deinit();
		set_mode_set(0);   // reset mode
		set_speed_set(0);  //reset speed
		fan_set(0,0);      //reset fan
		rgb_led_set(0,0);  // reset led
		blufi_adv_stop();  // blufi Access point stop
		blufi_ap_stop();  // blufi adv point start
		test_in_progress_reset();
	}
	return 0;
}

static int cmd_info_func(int argc, char **argv) {
	if (test_in_progress() == false) {
		printf("test already in stop! \n");
	}
	else {

    uint8_t bt_addr[BT_ADDRESS_LEN];
    uint8_t wifi_addr[WIFI_ADDRESS_LEN];

    static const char* threshold_str[] = { "Not configured", "Low", "Medium", "High" };
    static const char* mode_str[] = { "Off", "Immission", "Emission", "Fixed cycle", "Automatic cycle" };
    static const char* speed_str[] = { "None", "Night", "Vel1", "Vel2", "Vel3", "Boost" };
    static const char* direction_str[] = { "None", "Out", "In" };
    static const char* bt_connection_state_str[] = { "Disconnected", "Connected"  };
    static const char* wifi_connection_state_str[] = { "Disconnected", "Connected" };

    printf("Firmware version: %d.%d.%d\n", FW_VERSION_MAJOR, FW_VERSION_MINOR, FW_VERSION_PATCH);
//    printf("ESP version: %d.%d.%d\n", KERNEL_VERSION_MAJOR, KERNEL_VERSION_MINOR, KERNEL_PATCHLEVEL);
//    printf("Device serial number: " DEVICE_NAME_FMT "\n", *DEVICE_SERIAL_NUMBER_ADDR);

    blufi_get_ble_address(bt_addr);
    printf("Bluetooth address: %02X:%02X:%02X:%02X:%02X:%02X\n", bt_addr[5], bt_addr[4], bt_addr[3], bt_addr[2], bt_addr[1], bt_addr[0]);
    printf("BT Connection Number: %d - Connection State: %s\n", blufi_get_ble_connection_number(), bt_connection_state_str[blufi_get_ble_connection_state()]);

    blufi_get_wifi_address(wifi_addr);
    printf("WIFI address: %02X:%02X:%02X:%02X:%02X:%02X\n", wifi_addr[5], wifi_addr[4], wifi_addr[3], wifi_addr[2], wifi_addr[1], wifi_addr[0]);
    printf("WIFI Active: %s - Connection State: %s\n", (get_wifi_active() == 0 ? "No" : "Yes"), wifi_connection_state_str[blufi_get_wifi_connection_state()]);

    printf("Relative Humidity threshold: %s\n", threshold_str[get_relative_humidity_set() & 0x7f]);
    printf("Luminosity threshold: %s\n", threshold_str[get_lux_set()]);
    printf("VOC threshold: %s\n", threshold_str[get_voc_set() & 0x7f]);

    printf("Temperature Offset: %d - Humidity Offset: %d\n", get_temperature_offset(), get_relative_humidity_offset());

//    for (size_t idx = 0U; idx < BT_DEVICE_MAX; idx++) {
//        get_address_device_paired(bt_addr, idx);
//        if (memcmp(bt_addr, BT_ADDRESS_ANY, BT_ADDRESS_LEN)) {
//            printf("Device ECMF2 paired %d: %02X:%02X:%02X:%02X:%02X:%02X\n", idx, bt_addr[5], bt_addr[4], bt_addr[3], bt_addr[2], bt_addr[1], bt_addr[0]);
//        }
//    }

    printf("Setting Mode: %s - Setting Speed: %s\n", mode_str[get_mode_set()], speed_str[get_speed_set()]);
    printf("Mode: %s [calculate duration: %s]  [extra cycle: %s]  \n", mode_str[get_mode_state() & 0x1f], get_mode_state() & 0x80 ? "Yes" : "No", get_mode_state() & 0x40 ? "Yes" : "No");

    printf("Speed: %s - Direction: %s\n", speed_str[ADJUST_SPEED(get_speed_state())], direction_str[get_direction_state()]);

    printf("Temperature: %d [%.2f]\n", get_temperature(), get_temperature() * 0.1);
    printf("Humidity: %d [%.2f]\n", get_relative_humidity(), get_relative_humidity() * 0.1);
    printf("VOC: %d\n", get_voc());
    printf("Luminosity: %d\n", get_lux());
	}

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

	 const esp_console_cmd_t cmd_info = {
	       .command = "info",
	       .help = "info",
	       .hint = NULL,
	       .func = cmd_info_func,
	     };

	 esp_console_cmd_register(&cmd_info);


	 return 0;
}
