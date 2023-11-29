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
#include "fan.h"

#define SAVING_THRESHOLD_STATS				(50u)

typedef struct {
    struct {
        uint32_t night;
        uint32_t low;
        uint32_t medium;
        uint32_t high;
        uint32_t boost;
    } speed_counters_tot_sec;
} statistics_ts;

static uint32_t filter_operating = 0;
static statistics_ts statistics_current;

void statistic_init(void) {

    statistics_current.speed_counters_tot_sec.night = 0;
    statistics_current.speed_counters_tot_sec.low = 0;
    statistics_current.speed_counters_tot_sec.medium = 0;
    statistics_current.speed_counters_tot_sec.high = 0;
    statistics_current.speed_counters_tot_sec.boost = 0;
}

void statistic_update_handler(void) {
	uint8_t speed_state = ADJUST_SPEED(get_speed_state());

    switch(speed_state) {
        case SPEED_NIGHT:
            statistics_current.speed_counters_tot_sec.night++;
            break;
        case SPEED_LOW:
            statistics_current.speed_counters_tot_sec.low++;
            break;
        case SPEED_MEDIUM:
            statistics_current.speed_counters_tot_sec.medium++;
            break;
        case SPEED_HIGH:
            statistics_current.speed_counters_tot_sec.high++;
            break;
        case SPEED_BOOST:
            statistics_current.speed_counters_tot_sec.boost++;
            break;
        default:
            break;
    }

	filter_operating = ((statistics_current.speed_counters_tot_sec.night	* COEFF_NIGHT) +
						(statistics_current.speed_counters_tot_sec.low		* COEFF_LOW) +
						(statistics_current.speed_counters_tot_sec.medium	* COEFF_MEDIUM) +
						(statistics_current.speed_counters_tot_sec.high		* COEFF_HIGH) +
						(statistics_current.speed_counters_tot_sec.boost	* COEFF_BOOST)) / (COEFF_SCALE * SECONDS_PER_HOUR);

	if (filter_operating / SAVING_THRESHOLD_STATS) {
//		printf("Begin Filter Saved Thr: %ld - Actual Thr: %ld\n", get_filter_operating(),  filter_operating);
//		printf("NIGHT: %ld - LOW: %ld - MED: %ld - HIGH: %ld - BOOST: %ld\n", statistics_current.speed_counters_tot_sec.night, statistics_current.speed_counters_tot_sec.low, statistics_current.speed_counters_tot_sec.medium, statistics_current.speed_counters_tot_sec.high, statistics_current.speed_counters_tot_sec.boost);

		uint32_t filter_operating_diff = 0ul;

		filter_operating_diff += statistics_current.speed_counters_tot_sec.night / ((COEFF_SCALE * SECONDS_PER_HOUR) / COEFF_NIGHT);
		filter_operating_diff += statistics_current.speed_counters_tot_sec.low / ((COEFF_SCALE * SECONDS_PER_HOUR) / COEFF_LOW);
		filter_operating_diff += statistics_current.speed_counters_tot_sec.medium / ((COEFF_SCALE * SECONDS_PER_HOUR) / COEFF_MEDIUM);
		filter_operating_diff += statistics_current.speed_counters_tot_sec.high / ((COEFF_SCALE * SECONDS_PER_HOUR) / COEFF_HIGH);
		filter_operating_diff += statistics_current.speed_counters_tot_sec.boost / ((COEFF_SCALE * SECONDS_PER_HOUR) / COEFF_BOOST);

		set_filter_operating(get_filter_operating() + filter_operating_diff);

		statistics_current.speed_counters_tot_sec.night		%= (COEFF_SCALE * SECONDS_PER_HOUR) / COEFF_NIGHT;
		statistics_current.speed_counters_tot_sec.low		%= (COEFF_SCALE * SECONDS_PER_HOUR) / COEFF_LOW;
		statistics_current.speed_counters_tot_sec.medium	%= (COEFF_SCALE * SECONDS_PER_HOUR) / COEFF_MEDIUM;
		statistics_current.speed_counters_tot_sec.high		%= (COEFF_SCALE * SECONDS_PER_HOUR) / COEFF_HIGH;
		statistics_current.speed_counters_tot_sec.boost		%= (COEFF_SCALE * SECONDS_PER_HOUR) / COEFF_BOOST;

		filter_operating -= filter_operating_diff;

//		printf("End Filter Saved Thr: %ld - Actual Thr: %ld\n", get_filter_operating(),  filter_operating);
//		printf("NIGHT: %ld - LOW: %ld - MED: %ld - HIGH: %ld - BOOST: %ld\n", statistics_current.speed_counters_tot_sec.night, statistics_current.speed_counters_tot_sec.low, statistics_current.speed_counters_tot_sec.medium, statistics_current.speed_counters_tot_sec.high, statistics_current.speed_counters_tot_sec.boost);
	}

	// Check threshold
	if (((get_filter_operating() + filter_operating) >= FILTER_THRESHOLD) && !(get_device_state() & THRESHOLD_FILTER_WARNING)) {
		set_device_state(get_device_state() | THRESHOLD_FILTER_WARNING);
		printf("THRESHOLD_FILTER_WARNING\n");
	}

//	printf("Filter Operating Total: %ld\n", get_filter_operating() + filter_operating);
}

void statistic_reset_filter(void) {
	set_device_state(get_device_state() & ~THRESHOLD_FILTER_WARNING);
    set_filter_operating(0u);
    statistic_init();
    filter_operating = 0;
}
