/*
 * rgb_led.c
 *
 *  Created on: 30 juin 2023
 *      Author: youcef.benakmoume
 */


#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "rgb_led.h"
#include "ktd2027.h"

#define	LED_TASK_STACK_SIZE			(configMINIMAL_STACK_SIZE * 4)
#define	LED_TASK_PRIORITY			(1)
#define	LED_TASK_PERIOD				(100ul / portTICK_PERIOD_MS)

static void led_task(void *pvParameters) {
    while (1){
        vTaskDelay(pdMS_TO_TICKS(LED_TASK_PERIOD));  // Sleep for 100 ms
    }
}

static void rgb_led_blink_task(void *pvParameters) {
	struct blink_params *params = (struct blink_params *)pvParameters;

    uint8_t led_mode = 1;  // Assuming led_mode 1 represents blinking
    uint32_t blink_cycles = params->blink_duration / params->blink_period;  // Calculate the number of blink cycles

    for (uint32_t i = 0; i < blink_cycles; i++) {
        // Turn on the LED
        if (ktd2027_led_set(params->led_color, led_mode) != 0) {
            printf("Error setting LED on.\n");
        }
        vTaskDelay(params->blink_period / portTICK_PERIOD_MS);

        // Turn off the LED
        if (ktd2027_led_set(params->led_color, 0) != 0) {
            printf("Error setting LED off.\n");
        }
        vTaskDelay(params->blink_period / portTICK_PERIOD_MS);
    }

    // Free the memory allocated for parameters
    free(params);

    vTaskDelete(NULL);
}

int rgb_led_init(struct i2c_dev_s *i2c_dev) {
    ktd2027_init(i2c_dev);

    BaseType_t task_created = xTaskCreate(led_task, "LED task ", LED_TASK_STACK_SIZE, NULL, LED_TASK_PRIORITY, NULL);

    return task_created == pdPASS ? 0 : -1;
}

int rgb_led_set(uint8_t led_color, uint8_t led_mode) {
	return ktd2027_led_set(led_color,led_mode);
}


int rgb_led_blink(uint8_t led_color, uint32_t blink_duration, uint32_t blink_period) {
	struct blink_params *params = (struct blink_params *)malloc(sizeof(struct blink_params));

    if (params == NULL) {
        return -1;
    }

    params->led_color = led_color;
    params->blink_duration = blink_duration;
    params->blink_period = blink_period;

    BaseType_t task_created = xTaskCreate(rgb_led_blink_task, "Blink task", 2048, params, 5, NULL);

    return task_created == pdPASS ? 0 : -1;
}

