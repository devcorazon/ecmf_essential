/*
 * statistic.h
 *
 *  Created on: 25 sept. 2023
 *      Author: youcef.benakmoume
 */

#ifndef MAIN_INCLUDE_STATISTIC_H_
#define MAIN_INCLUDE_STATISTIC_H_

#include "system.h"

// Initialize the statistic module
void statistic_init(void);

// Call this function every second when the fan is running
void statistic_update_handler(int speed_mode);

// Reset the filter count and alarm bit
void statistic_reset_filter(void);

#endif /* MAIN_INCLUDE_STATISTIC_H_ */
