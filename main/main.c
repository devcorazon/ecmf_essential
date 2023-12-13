/*
 * main.c
 *
 *  Created on: 8 lug 2023
 *      Author: Daniele Schirosi
 */

///
#include <freertos/FreeRTOS.h>

#include "system.h"

///
void app_main(void) {
	printf("Starting v%u.%u.%u firmware\n", FW_VERSION_MAJOR, FW_VERSION_MINOR, FW_VERSION_PATCH);
	system_init();
}
