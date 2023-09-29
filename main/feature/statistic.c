/*
 * statistic.c
 *
 *  Created on: 25 sept. 2023
 *      Author: youcef.benakmoume
 */

#include "statistic.h"
#include "types.h"
#include "storage.h"
#include "structs.h"

typedef struct {
    struct {
        uint32_t night;
        uint32_t low;
        uint32_t medium;
        uint32_t high;
        uint32_t boost;
    } speed_counters_tot_hour;
} statistics_ts;

static uint32_t filter_operating = 0;
static statistics_ts statistics_current;
static uint32_t seconds_counter = 0;

void statistic_init(void) {
    filter_operating = get_fan_runtime();  // Assuming this function exists in your storage module
    seconds_counter = filter_operating * 60;  // Convert fan runtime to seconds

    statistics_current.speed_counters_tot_hour.night = 0;
    statistics_current.speed_counters_tot_hour.low = 0;
    statistics_current.speed_counters_tot_hour.medium = 0;
    statistics_current.speed_counters_tot_hour.high = 0;
    statistics_current.speed_counters_tot_hour.boost = 0;
}


void statistic_update_handler(int speed_state) {

    // Increment seconds counter
    seconds_counter++;

    // Every minute, update the fan runtime
    if (seconds_counter % 60 == 0) {
    	storage_increment_fan_runtime();
    }

    switch(speed_state) {
        case SPEED_NIGHT:
            statistics_current.speed_counters_tot_hour.night++;
            break;
        case SPEED_LOW:
            statistics_current.speed_counters_tot_hour.low++;
            break;
        case SPEED_MEDIUM:
            statistics_current.speed_counters_tot_hour.medium++;
            break;
        case SPEED_HIGH:
            statistics_current.speed_counters_tot_hour.high++;
            break;
        case SPEED_BOOST:
            statistics_current.speed_counters_tot_hour.boost++;
            break;
        default:
            break;
    }


    // Check every hour
    if(seconds_counter >= 3600) {
        filter_operating += (statistics_current.speed_counters_tot_hour.night) * COEFF_NIGHT;
        filter_operating += (statistics_current.speed_counters_tot_hour.low) * COEFF_LOW;
        filter_operating += (statistics_current.speed_counters_tot_hour.medium) * COEFF_MEDIUM;
        filter_operating += (statistics_current.speed_counters_tot_hour.high) * COEFF_HIGH;
        filter_operating += (statistics_current.speed_counters_tot_hour.boost) * COEFF_BOOST;

        // Reset counters after applying coefficients
        statistics_current.speed_counters_tot_hour.night = 0;
        statistics_current.speed_counters_tot_hour.low = 0;
        statistics_current.speed_counters_tot_hour.medium = 0;
        statistics_current.speed_counters_tot_hour.high = 0;
        statistics_current.speed_counters_tot_hour.boost = 0;

        seconds_counter = 0; // Reset seconds counter
        storage_reset_fan_runtime(); // Reset fan runtime

    }

    // Check threshold
    if (filter_operating >= FILTER_THRESHOLD) {
        uint8_t current_state = get_device_state();
        current_state |= (1 << 0);  // Set the 0th bit directly
        set_device_state(current_state);
    }

}

void statistic_reset_filter(void) {
    filter_operating = 0;
    uint8_t current_state = get_device_state();
    current_state &= ~(1 << 0);  // Clear the 0th bit directly
    set_device_state(current_state);
}
