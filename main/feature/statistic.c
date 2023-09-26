/*
 * statistic.c
 *
 *  Created on: 25 sept. 2023
 *      Author: youcef.benakmoume
 */

#include "statistic.h"
#include "types.h"
#include "storage.h"

typedef struct {
    struct {
        uint32_t night;
        uint32_t low;
        uint32_t medium;
        uint32_t high;
        uint32_t boost;
    } speed_counters_tot_hour;
} statistics_ts;

static float filter_operating = 0; // Count for the filter operation in hours

static statistics_ts statistics_current;
static uint32_t seconds_elapsed = 0; // Count of seconds elapsed

void statistic_init(void) {
    filter_operating = 0;
    statistics_current.speed_counters_tot_hour.night = 0;
    statistics_current.speed_counters_tot_hour.low = 0;
    statistics_current.speed_counters_tot_hour.medium = 0;
    statistics_current.speed_counters_tot_hour.high = 0;
    statistics_current.speed_counters_tot_hour.boost = 0;
}

void statistic_update_handler(int speed_mode) {
    switch(speed_mode) {
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
    if(seconds_elapsed >= 3600) {
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

        seconds_elapsed = 0; // Reset seconds counter
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