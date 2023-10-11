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
	printf("my version number is 1\n");
	system_init();
}
