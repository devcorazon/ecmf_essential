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
#include <freertos/timers.h>


#include "esp_console.h"
#include "esp_wifi.h"
#include "esp_idf_version.h"

#include "board.h"
#include "system.h"
#include "storage_internal.h"
#include "ir_receiver.h"
#include "sensor.h"
#include "rgb_led.h"
#include "fan.h"
#include "blufi.h"
#include "sht4x.h"
#include "sgp40.h"
#include "ltr303.h"

typedef struct {
    uint32_t cycle_time_s;
    uint8_t speed_percent;
    uint8_t direction;
} FanCycleState;

static TimerHandle_t fan_cycle_timer = NULL; // Timer handle for fan cycle
////
#define	IR_RECEIVER_TASK_STACK_SIZE			    (configMINIMAL_STACK_SIZE * 4)
#define	IR_RECEIVER_TASK_PRIORITY			    (1)
#define	IR_RECEIVER_TASK_PERIOD				    (100ul / portTICK_PERIOD_MS)

static void ir_receiver_test_task(void *pvParameters);
TaskHandle_t ir_receiver_test_task_handle = NULL;
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

void fan_cycle_timer_callback(TimerHandle_t xTimer) {
    if (!test_in_progress()) {
        return;
    }
    FanCycleState *state = (FanCycleState *)pvTimerGetTimerID(xTimer);
    fan_set_percentage(state->direction, state->speed_percent);
    state->direction = (state->direction == DIRECTION_IN) ? DIRECTION_OUT : DIRECTION_IN;

    // Restart the timer for the next cycle
    xTimerChangePeriod(xTimer, pdMS_TO_TICKS(state->cycle_time_s * 1000), 0);
    xTimerStart(xTimer, 0);
}

static void ir_receiver_test_task(void *pvParameters) {
	TickType_t ir_receiver_task_time;

	ir_receiver_task_time = xTaskGetTickCount();
	while (1) {
    	if (ir_receiver_take_button()) {
    		rgb_led_mode(RGB_LED_COLOR_RED, RGB_LED_MODE_ONESHOOT, false);
    	}

		vTaskDelayUntil(&ir_receiver_task_time, IR_RECEIVER_TASK_PERIOD);
	}
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
			printf("Temperature: %d.%01d C\n", TEMP_RAW_TO_INT(temp), TEMP_RAW_TO_DEC(temp));
		}

		uint16_t rh = get_relative_humidity();
		if (rh == RELATIVE_HUMIDITY_INVALID) {
			printf("Relative humidity reading error\n");
		}
		else {
			printf("Relative humidity: %u.%01u %%\n", RH_RAW_TO_INT(rh), RH_RAW_TO_DEC(rh));
		}

	    uint16_t voc = get_voc();
	    if (voc == VOC_INVALID)
	    {
	        printf("VOC Index reading error\n");
	    }
	    else {
	        printf("VOC Index: %u4 \n", voc);
	    }

		ltr303_measure_lux(&lux);
		uint16_t u16_lux = SET_VALUE_TO_TEMP_RAW(lux);

		if (u16_lux == LUX_INVALID) {
			printf("LUX reading error\n");
		}
		else {
			printf("LUX: %u.%01u %\n", TEMP_RAW_TO_INT(u16_lux), TEMP_RAW_TO_DEC(u16_lux));
		}

		sensor_ntc_sample(&ntc_temp);
		int16_t i16_ntc_temp = SET_VALUE_TO_TEMP_RAW(ntc_temp);

		if (i16_ntc_temp == TEMPERATURE_INVALID) {
			printf("NTC Temperature reading error\n");
		}
		else {
			printf("NTC Temperature: %d.%01d C\n", TEMP_RAW_TO_INT(i16_ntc_temp), TEMP_RAW_TO_DEC(i16_ntc_temp));
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

static int cmd_fan_fix_func(int argc, char **argv) {
    if (argc < 3) {
        printf("Invalid arguments. Usage: fan_fix <direction> <speed %>\n");
        return -1;
    }

    char *endptr;
    int8_t direction = (uint8_t) strtol(argv[1], &endptr, 10);
    int8_t speed_percent = (uint8_t) strtol(argv[2], &endptr, 10);

    // Check for conversion errors
    if (endptr == argv[1] || *endptr != '\0' || direction < 0 || direction > 2 || speed_percent < 0 || speed_percent > 100) {
        printf("Invalid arguments. Supported values: direction = {0,1,2}, speed = {0-100%}\n");
        return -1;
    }

    if (!test_in_progress()) {
        printf("Make sure to run test_start \n");
        return -1;
    }

    fan_set_percentage(direction, speed_percent);
    return 0;
}

static int cmd_fan_cycle_func(int argc, char **argv) {
    if (argc < 3) {
        printf("Invalid arguments. Usage: fan_cycle <time in (s)> <speed %>\n");
        return -1;
    }

    char *endptr;
    int32_t cycle_time = (uint32_t) strtol(argv[1], &endptr, 10);
    int8_t speed_percent = (uint8_t) strtol(argv[2], &endptr, 10);

    // Check for conversion errors
    if (endptr == argv[1] || *endptr != '\0' || cycle_time < 0 || speed_percent > 100) {
        printf("Invalid arguments. Supported values: {time} >= 0, speed = {0-100%}\n");
        return -1;
    }

    if (!test_in_progress() || cycle_time == 0) {
        printf("Stopping the fan cycle.\n");
        if (fan_cycle_timer != NULL) {
            xTimerStop(fan_cycle_timer, 0);
            xTimerDelete(fan_cycle_timer, 0);
            fan_cycle_timer = NULL;
        }
        fan_set_percentage(DIRECTION_NONE, 0); // Stop the fan
        return -1;
    }

    // Set initial state and start fan immediately
    static FanCycleState state;
    state.cycle_time_s = cycle_time;
    state.speed_percent = speed_percent;
    state.direction = DIRECTION_IN; // Initial direction

    fan_set_percentage(state.direction, state.speed_percent); // Start the fan immediately
    state.direction = (state.direction == DIRECTION_IN) ? DIRECTION_OUT : DIRECTION_IN; // Prepare for next cycle

    // Create or reset the timer for subsequent cycles
    if (fan_cycle_timer == NULL) {
        fan_cycle_timer = xTimerCreate("FanCycleTimer", pdMS_TO_TICKS(state.cycle_time_s * 1000), pdTRUE, (void *)&state, fan_cycle_timer_callback);
    } else {
        xTimerChangePeriod(fan_cycle_timer, pdMS_TO_TICKS(state.cycle_time_s * 1000), 0);
    }

    xTimerStart(fan_cycle_timer, 0);

    return 0;
}


static int cmd_test_start_func(int argc, char **argv) {
	esp_err_t ret;
    if (test_in_progress() == true) {
        printf("Run test_stop and start again! \n");
        return -1;
    }
    if (xTaskCreate(ir_receiver_test_task, "IR Receiver test task", IR_RECEIVER_TASK_STACK_SIZE, NULL, IR_RECEIVER_TASK_PRIORITY, &ir_receiver_test_task_handle) != pdPASS) {
            ir_receiver_test_task_handle = NULL;
            return -1;
    }

    blufi_ble_init();

	if (get_sta_connected() || get_sta_is_connecting()) {
		esp_err_t ret = esp_wifi_disconnect();
		if (ret != ESP_OK) {
			printf("Failed to disconnect WiFi STA: %s\n", esp_err_to_name(ret));
			return -1;
		}
	}

	ret = blufi_adv_start();
	if (ret != ESP_OK) {
		printf("Failed to start blufi advertisement: %s\n", esp_err_to_name(ret));
		return -1;
	}

	ret = blufi_ap_start();
	if (ret != ESP_OK) {
		printf("Failed to start blufi AP: %s\n", esp_err_to_name(ret));
		return -1;
	}

	test_in_progress_set();

    return (ir_receiver_test_task_handle != NULL ? 0 : -1);
}

static int cmd_test_stop_func(int argc, char **argv) {
	esp_err_t ret;
	if (test_in_progress() == false) {
		printf("test already in stop! \n");
	} else {
        if (ir_receiver_test_task_handle != NULL) {
            vTaskDelete(ir_receiver_test_task_handle);
            ir_receiver_test_task_handle = NULL; // Set the handle to NULL after deleting the task
        }
		blufi_ble_deinit();
		set_mode_set(0);   // reset mode
		set_speed_set(0);  //reset speed
		fan_set(0,0);      //reset fan
		rgb_led_set(0,0);  // reset led
		blufi_adv_stop();  // blufi Access point stop
		blufi_ap_stop();  // blufi adv point start
		test_in_progress_reset();
		ret = blufi_wifi_deinit();
	    if (ret != ESP_OK) {
	        printf("Failed to deinitialize WiFi: %s\n", esp_err_to_name(ret));
	        return -1;
	    }

	    if (get_wifi_active()) {
	        ret = blufi_wifi_init();
	        if (ret != ESP_OK) {
	            printf("Failed to reinitialize WiFi: %s\n", esp_err_to_name(ret));
	            return -1;
	        }
	        blufi_wifi_connect(); // Ensure this function attempts to reconnect
	    }
	}
	return 0;
}

static int cmd_info_func(int argc, char **argv) {
    uint8_t bt_addr[BT_ADDRESS_LEN];
    uint8_t wifi_addr[WIFI_ADDRESS_LEN];
	float lux;
	float ntc_temp;

    static const char* threshold_str[] = { "Not configured", "Low", "Medium", "High" };
    static const char* mode_str[] = { "Off", "Immission", "Emission", "Fixed cycle", "Automatic cycle" };
    static const char* speed_str[] = { "None", "Night", "Vel1", "Vel2", "Vel3", "Boost" };
    static const char* direction_str[] = { "None", "Out", "In" };
    static const char* bt_connection_state_str[] = { "Disconnected", "Connected"  };
    static const char* wifi_connection_state_str[] = { "Disconnected", "Connected" };

    printf("Firmware version: %d.%d.%d\n", FW_VERSION_MAJOR, FW_VERSION_MINOR, FW_VERSION_PATCH);
    printf("ESP version: %d.%d.%d\n", ESP_IDF_VERSION_MAJOR, ESP_IDF_VERSION_MINOR, ESP_IDF_VERSION_PATCH);
	printf("Device Serial Number: %08lx\n", get_serial_number());

    blufi_get_ble_address(bt_addr);
    printf("Bluetooth address: %02X:%02X:%02X:%02X:%02X:%02X\n", bt_addr[5], bt_addr[4], bt_addr[3], bt_addr[2], bt_addr[1], bt_addr[0]);
    printf("BT Connection Number: %d - Connection State: %s\n", blufi_get_ble_connection_number(), bt_connection_state_str[blufi_get_ble_connection_state()]);

    blufi_get_wifi_address(wifi_addr);
    printf("WIFI address: %02X:%02X:%02X:%02X:%02X:%02X\n", wifi_addr[5], wifi_addr[4], wifi_addr[3], wifi_addr[2], wifi_addr[1], wifi_addr[0]);
    printf("WIFI Active: %s - Connection State: %s\n", (get_wifi_active() == 0 ? "No" : "Yes"), wifi_connection_state_str[blufi_get_wifi_connection_state()]);

    printf("Relative Humidity threshold: %s\n", threshold_str[get_relative_humidity_set() & 0x7f]);
    printf("VOC threshold: %s\n", threshold_str[get_voc_set() & 0x7f]);
    printf("Luminosity threshold: %s\n", threshold_str[get_lux_set()]);

    printf("Temperature Offset: %d - Humidity Offset: %d\n", get_temperature_offset(), get_relative_humidity_offset());

    printf("Setting Mode: %s - Setting Speed: %s\n", mode_str[get_mode_set()], speed_str[get_speed_set()]);
    printf("Mode: %s [calculate duration: %s]  [extra cycle: %s]  \n", mode_str[get_mode_state() & 0x1f], get_mode_state() & 0x80 ? "Yes" : "No", get_mode_state() & 0x40 ? "Yes" : "No");

    printf("Speed: %s - Direction: %s\n", speed_str[ADJUST_SPEED(get_speed_state())], direction_str[get_direction_state()]);

	int16_t temp = get_temperature();
	if (temp == TEMPERATURE_INVALID) {
		printf("Temperature reading error\n");
	} else {
		printf("Temperature: %d.%01d C\n", TEMP_RAW_TO_INT(temp), TEMP_RAW_TO_DEC(temp));
	}

	uint16_t rh = get_relative_humidity();
	if (rh == RELATIVE_HUMIDITY_INVALID) {
		printf("Relative humidity reading error\n");
	} else {
		printf("Relative humidity: %u.%01u %%\n", RH_RAW_TO_INT(rh), RH_RAW_TO_DEC(rh));
	}

	uint16_t voc = get_voc();
	if (voc == VOC_INVALID) {
		printf("VOC Index reading error\n");
	} else {
		printf("VOC Index: %u4 \n", voc);
	}

	ltr303_measure_lux(&lux);
	uint16_t u16_lux = SET_VALUE_TO_TEMP_RAW(lux);

	if (u16_lux == LUX_INVALID) {
		printf("LUX reading error\n");
	} else {
		printf("LUX: %u.%01u %\n", TEMP_RAW_TO_INT(u16_lux), TEMP_RAW_TO_DEC(u16_lux));
	}

	sensor_ntc_sample(&ntc_temp);
	int16_t i16_ntc_temp = SET_VALUE_TO_TEMP_RAW(ntc_temp);

	if (i16_ntc_temp == TEMPERATURE_INVALID) {
		printf("NTC Temperature reading error\n");
	} else {
		printf("NTC Temperature: %d.%01d C\n", TEMP_RAW_TO_INT(i16_ntc_temp), TEMP_RAW_TO_DEC(i16_ntc_temp));
	}
	return 0;
}

static int cmd_reboot_func(int argc, char **argv) {
	 esp_restart();
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

	const esp_console_cmd_t cmd_fan_fix = {
		   .command = "fan_fix",
		   .help = "Fan fix {Dir} {% Speed}",
		   .hint = NULL,
		   .func = cmd_fan_fix_func,
		 };

	esp_console_cmd_register(&cmd_fan_fix);

	const esp_console_cmd_t cmd_fan_cycle = {
		   .command = "fan_cycle",
		   .help = "Fan cycle {tempo (s)} {% Speed}",
		   .hint = NULL,
		   .func = cmd_fan_cycle_func,
		 };

	esp_console_cmd_register(&cmd_fan_cycle);

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

	 const esp_console_cmd_t cmd_reboot = {
	       .command = "reboot",
	       .help = "reboot",
	       .hint = NULL,
	       .func = cmd_reboot_func,
	     };

	 esp_console_cmd_register(&cmd_reboot);

	 return 0;
}
