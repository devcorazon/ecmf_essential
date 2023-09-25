/*
 * statistic.h
 *
 *  Created on: 25 sept. 2023
 *      Author: youcef.benakmoume
 */

#ifndef MAIN_INCLUDE_STATISTIC_H_
#define MAIN_INCLUDE_STATISTIC_H_

#include "system.h"

void statistic_init(void);

void statistic_update_handler(int speed_mode);

void statistic_reset_filter(void);


#endif /* MAIN_INCLUDE_STATISTIC_H_ */
