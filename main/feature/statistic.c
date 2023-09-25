/*
 * statistic.c
 *
 *  Created on: 25 sept. 2023
 *      Author: youcef.benakmoume
 */

#include "statistic.h"
#include "types.h"
#include "storage.h"

static uint32_t filter_count = 0; // Count for the filter

void statistic_init(void) {
    filter_count = 0;
}

void statistic_update_handler(int speed_mode) {
    switch(speed_mode) {
        case SPEED_NIGHT:
            filter_count += 0.2;
            break;
        case SPEED_LOW:
            filter_count += 0.6;
            break;
        case SPEED_MEDIUM:
            filter_count += 1.0;
            break;
        case SPEED_HIGH:
            filter_count += 1.4;
            break;
        case SPEED_BOOST:
            filter_count += 1.5;
            break;
        default:
            break;
    }

    if (filter_count >= FILTER_THRESHOLD) {
        uint8_t current_state = get_device_state();
        current_state |= (1 << 0);
        set_device_state(current_state);
    }
}

void statistic_reset_filter(void) {
    filter_count = 0;
    uint8_t current_state = get_device_state();
    current_state &= ~(1 << 0);
    set_device_state(current_state);
}


