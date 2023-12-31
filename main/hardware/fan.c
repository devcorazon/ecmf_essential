/*
 * fan.c
 *
 *  Created on: 29 juin 2023
 *      Author: youcef.benakmoume
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_rom_gpio.h"

#include "fan.h"
#include "storage.h"

#define	FAN_TASK_STACK_SIZE			(configMINIMAL_STACK_SIZE * 4)
#define	FAN_TASK_PRIORITY			(1)
#define	FAN_TASK_PERIOD				(100ul / portTICK_PERIOD_MS)

static int fan_speed = 0;

static void fan_task(void *pvParameters) {
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(FAN_TASK_PERIOD));  // Sleep for 100 ms
    }
}

int fan_init() {
    BaseType_t task_created = xTaskCreate(fan_task, "FAN task ", FAN_TASK_STACK_SIZE, NULL, FAN_TASK_PRIORITY, NULL);

    return task_created == pdPASS ? 0 : -1;
}

int fan_set(uint8_t direction, uint8_t speed) {
#if 0
	static uint8_t last_direction = DIRECTION_NONE;

    // Check if the direction has changed
    if (last_direction != direction) {
        // Set speed to zero and update
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

        // Wait for 7 seconds
        vTaskDelay(pdMS_TO_TICKS(7000));

        // Update last direction
        last_direction = direction;
    }
#endif
	if ( direction == DIRECTION_IN) {
    gpio_set_level(FAN_DIRECTION_PIN, 0);
	} else if( direction == DIRECTION_OUT) {
	gpio_set_level(FAN_DIRECTION_PIN, 1);
	}

    if (speed > 5) {
        printf("Speed value exceeded. Setting to maximum.\n");
        speed = 5;
    }

    if (direction == DIRECTION_IN) {
        fan_speed = fan_pwm_pulse_in[speed];
    } else if( direction == DIRECTION_OUT) {
        fan_speed = fan_pwm_pulse_out[speed];
    } else if(direction == DIRECTION_NONE) {
    	fan_speed = 0;
    }

    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, fan_speed);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

  //  printf("Fan set: direction=%u, speed=%d.\n", direction, speed);

    return 0;
}

int fan_set_percentage(uint8_t direction, uint8_t speed_percent) {
#if 0
	static uint8_t last_direction = DIRECTION_NONE;

    // Check if the direction has changed
    if (last_direction != direction) {
        // Set speed to zero and update
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

        // Wait for 7 seconds
        vTaskDelay(pdMS_TO_TICKS(7000));

        // Update last direction
        last_direction = direction;
    }
#endif
    if (direction == DIRECTION_IN) {
        gpio_set_level(FAN_DIRECTION_PIN, 0);
        fan_speed = (FAN_PWM_MAX * speed_percent) / 100;
    } else if (direction == DIRECTION_OUT) {
        gpio_set_level(FAN_DIRECTION_PIN, 1);
        fan_speed = (FAN_PWM_MAX * speed_percent) / 100;
    } else if (direction == DIRECTION_NONE) {
        fan_speed = 0;
    } else {
        printf("Invalid direction.\n");
        return -1;
    }

    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, fan_speed);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

    printf("Fan set: direction=%u, speed=%u%%.\n", direction, speed_percent);

    return 0;
}
