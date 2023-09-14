/*
 * controller.c
 *
 *  Created on: 4 sept. 2023
 *      Author: youcef.benakmoume
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"

#include "math.h"

#include "storage.h"
#include "fan.h"
#include "test.h"

#define	CONTROLLER_TASK_STACK_SIZE			        (configMINIMAL_STACK_SIZE * 4)
#define	CONTROLLER_TASK_PRIORITY			        (1)
#define	CONTROLLER_TASK_PERIOD				        (1000ul / portTICK_PERIOD_MS)

#define DURATION_IMMISSION_EMISSION_MS              SECONDS_TO_MS(DURATION_IMMISSION_EMISSION)
#define DURATION_FIXED_CYCLE_MS						SECONDS_TO_MS(DURATION_FIXED_CYCLE)
#define DURATION_AUTOMATIC_CYCLE_OUT_MS				SECONDS_TO_MS(DURATION_AUTOMATIC_CYCLE_OUT)
#define DURATION_AUTOMATIC_CYCLE_IN_MS				SECONDS_TO_MS(DURATION_AUTOMATIC_CYCLE_IN)
#define DURATION_RESTART_AUTOMATIC_CYCLE_MS			SECONDS_TO_MS(DURATION_RESTART_AUTOMATIC_CYCLE)

#define DURATION_EXTRA_CYCLE_BOOST_MS			    SECONDS_TO_MS(DURATION_EXTRA_CYCLE_BOOST)
#define DURATION_RESTART_EXTRA_CYCLE_MS		    	SECONDS_TO_MS(DURATION_RESTART_EXTRA_CYCLE)

#define EXTRA_CYCLE_INVERSIONS_MAX			     	4U
#define CALCULATE_DURATION_INVERSIONS_MAX	     	1U

#define EXTRA_CYCLE_COUNT_MAX				    	3U

#define CONDITION_COUNT_MAX				    		3U

#define CALCULATE_DURATION_INVERSIONS_MAX		    1U

#define CONDITION_COUNT_MAX						    3U

static uint32_t duration_automatic_cycle;
//
static uint16_t calculate_duration_automatic_cycle(int16_t emission_temperature,
		int16_t immission_temperature);
//
static void controller_task(void *pvParameters);
static void controller_set(void);
static void reset_automatic_cycle_count(void);
//
static void work_task(void *arg);

// Define Semaphore handler
static SemaphoreHandle_t extra_cycle_count_sem;

// Define the timer handle
TimerHandle_t controller_timer = NULL;
TimerHandle_t restart_automatic_cycle_timer = NULL;
TimerHandle_t restart_extra_cycle_timer = NULL;
TimerHandle_t work_timer = NULL;

static uint8_t calculate_duration_inversions_count;
static uint8_t extra_cycle_inversions_count;

/// Structure defining the temperature versus time.
struct time_shape {
	int16_t temperature;		///< Temperature (celsius)
	uint16_t time;				///< Time		 (seconds)
};

/// Constants to calculate time.
static const struct time_shape time_convert[] = {
		{ -10 * TEMPERATURE_SCALE, 35U },
		{ -6 * TEMPERATURE_SCALE, 80U },
		{ -3 * TEMPERATURE_SCALE, 150U },
		{ 3 * TEMPERATURE_SCALE, 200U },
		{10 * TEMPERATURE_SCALE, 150U },
        { 15 * TEMPERATURE_SCALE, 80U },
		{ 20 * TEMPERATURE_SCALE, 50U },
		{ 25 * TEMPERATURE_SCALE, 35U },
};

static inline uint8_t ADJUST_SPEED(uint8_t speed) {
	if (speed & SPEED_AUTOMATIC_CYCLE_FORCE_NIGHT) {
		speed = SPEED_NIGHT;
	}
	else if (speed & SPEED_AUTOMATIC_CYCLE_FORCE_BOOST) {
		speed = SPEED_BOOST;
	}

	return speed;
}
//
static void controller_task(void *pvParameters) {
	TickType_t controller_task_time;

	controller_task_time = xTaskGetTickCount();
	while (1) {

		if (test_in_progress() == false) {
			uint8_t mode_set = get_mode_set();
			uint8_t mode_state = get_mode_state();

			mode_state &= ~(MODE_AUTOMATIC_CYCLE_EXTRA_CYCLE | MODE_AUTOMATIC_CYCLE_CALCULATE_DURATION);

			if (mode_state != mode_set) {
				xTimerStop(controller_timer, 0);
				xTimerStop(restart_automatic_cycle_timer, 0);
				reset_automatic_cycle_count();

				switch (mode_set) {
				case MODE_OFF:
					set_mode_state(MODE_OFF);
					set_direction_state(DIRECTION_NONE);
					set_speed_state(SPEED_NONE);
					set_automatic_cycle_duration(0U);
					break;

				case MODE_IMMISSION:
					set_mode_state(MODE_IMMISSION);
					set_direction_state(DIRECTION_IN);
					set_automatic_cycle_duration(0U);
					xTimerChangePeriod(controller_timer,pdMS_TO_TICKS(DURATION_IMMISSION_EMISSION_MS),portMAX_DELAY);
					xTimerStart(controller_timer, 0);
					break;

				case MODE_EMISSION:
					set_mode_state(MODE_EMISSION);
					set_direction_state(DIRECTION_OUT);
					set_automatic_cycle_duration(0U);
					xTimerChangePeriod(controller_timer,pdMS_TO_TICKS(DURATION_IMMISSION_EMISSION_MS),portMAX_DELAY);
					xTimerStart(controller_timer, 0);
					break;

				case MODE_FIXED_CYCLE:
					set_mode_state(MODE_FIXED_CYCLE);
					set_direction_state(DIRECTION_OUT);
					set_automatic_cycle_duration(0U);
					xTimerChangePeriod(controller_timer,pdMS_TO_TICKS(DURATION_FIXED_CYCLE_MS),portMAX_DELAY);
					xTimerStart(controller_timer, 0);
					break;

				case MODE_AUTOMATIC_CYCLE:
					set_mode_state(MODE_AUTOMATIC_CYCLE| MODE_AUTOMATIC_CYCLE_CALCULATE_DURATION);
					set_automatic_cycle_duration(0U);
					xTaskCreate(work_task, "work task",CONTROLLER_TASK_STACK_SIZE, NULL,CONTROLLER_TASK_PRIORITY, NULL);
					break;

				default:
					break;
				}
			}
			controller_set();

			fan_set(get_direction_state(), ADJUST_SPEED(get_speed_state()));

			set_mode_state(get_mode_state());
			set_speed_state(get_speed_state());
			set_direction_state(get_direction_state());

//			static const char *mode_log_str[] = { "Off", "Immission","Emission", "Fixed cycle", "Automatic cycle" };
//			static const char *speed_log_str[] = { "None", "Night", "Vel1","Vel2", "Vel3", "Boost", "Profiled" };
//			static const char *direction_log_str[] = { "None", "Out", "In" };
//			static uint8_t mode_log = MODE_OFF;
//			static uint8_t speed_log = SPEED_NONE;
//			static uint8_t speed_set_log = SPEED_NONE;
//			static uint8_t direction_log = DIRECTION_NONE;

//			if (mode_log != get_mode_state() || speed_log != get_speed_state()
//					|| speed_set_log != get_speed_set()
//					|| direction_log != get_direction_state()) {
//				mode_log = get_mode_state();
//				speed_log = get_speed_state();
//				speed_set_log = get_speed_set();
//				direction_log = get_direction_state();

//			LOG_INF(
//					"MODE:%s (calc dur:%s) (free cool:%s) (extra cycle:%s) - SPEED:%s [%s] (force night:%s) (force boost:%s) (inc speed:%s) - DIRECTION:%s",
//					(mode_log_str[mode_log
//							& ~(MODE_AUTOMATIC_CYCLE_FREE_COOLING
//									| MODE_AUTOMATIC_CYCLE_EXTRA_CYCLE
//									| MODE_AUTOMATIC_CYCLE_CALCULATE_DURATION)]),
//					(mode_log & MODE_AUTOMATIC_CYCLE_CALCULATE_DURATION ?
//							"Y" : "N"),
//					(mode_log & MODE_AUTOMATIC_CYCLE_FREE_COOLING ? "Y" : "N"),
//					(mode_log & MODE_AUTOMATIC_CYCLE_EXTRA_CYCLE ? "Y" : "N"),
//					(speed_log_str[ADJUST_SPEED(get_speed_state())]),
//					(speed_log_str[
//							get_speed_setting() != SPEED_PROLIFED ?
//									get_speed_setting() : 6]),
//					(get_speed_state() & SPEED_AUTOMATIC_CYCLE_FORCE_NIGHT ?
//							"Y" : "N"),
//					(get_speed_state() & SPEED_AUTOMATIC_CYCLE_FORCE_BOOST ?
//							"Y" : "N"),
//					(get_speed_state() & SPEED_AUTOMATIC_CYCLE_INC_SPEED ?
//							"Y" : "N"), (direction_log_str[direction_log]));
//	}
//			}

		}
		vTaskDelayUntil(&controller_task_time, CONTROLLER_TASK_PERIOD);
	}
}

static uint16_t calculate_duration_automatic_cycle(int16_t emission_temperature,int16_t immission_temperature) {
	int16_t delta = emission_temperature - immission_temperature;
	size_t i;

	if (delta < time_convert[0U].temperature) {
		return time_convert[0U].time;
	}

	for (i = 1U; i < sizeof(time_convert); i++) {
		if (delta < time_convert[i].temperature) {
			return (uint16_t) (ceiling_fraction(((delta - time_convert[i - 1U].temperature)* ((int16_t )time_convert[i].time- (int16_t )time_convert[i - 1U].time)),(time_convert[i].temperature- time_convert[i - 1U].temperature))+ (int16_t) time_convert[i - 1U].time);
		}
	}

	return time_convert[sizeof(time_convert) - 1U].time;
}

static void controller_set(void) {
	static const uint8_t luminosity_threshold_convert[] = { 0U,LUMINOSITY_THRESHOLD_LOW, LUMINOSITY_THRESHOLD_MEDIUM,LUMINOSITY_THRESHOLD_HIGH };
	static const uint8_t relative_humidity_threshold_convert[] = { 0U,RH_THRESHOLD_LOW, RH_THRESHOLD_MEDIUM, RH_THRESHOLD_HIGH };
	static const uint8_t voc_threshold_convert[] = { 0U, VOC_THRESHOLD_LOW, VOC_THRESHOLD_MEDIUM, VOC_THRESHOLD_HIGH };
	uint8_t speed_set = get_speed_set();
	uint8_t speed_state = get_speed_state();
	int16_t luminosity = get_lux();
	uint16_t relative_humidity = get_relative_humidity();
	uint16_t voc = get_voc();

	uint8_t luminosity_threshold = luminosity_threshold_convert[get_lux_set()];
	uint8_t relative_humidity_threshold = relative_humidity_threshold_convert[get_relative_humidity_set()];
	uint8_t voc_threshold = voc_threshold_convert[get_voc_set()];
	static uint8_t cond_flags = 0U;
	static uint8_t count_luminosity = 0U;
	static uint8_t count_rh_extra_cycle = 0U;
	static uint8_t count_voc_extra_cycle = 0U;

	if ((speed_state & ~(SPEED_AUTOMATIC_CYCLE_FORCE_BOOST | SPEED_AUTOMATIC_CYCLE_FORCE_NIGHT)) != speed_set) {
		speed_state &= (SPEED_AUTOMATIC_CYCLE_FORCE_BOOST | SPEED_AUTOMATIC_CYCLE_FORCE_NIGHT);

		speed_state = speed_set;

		if ((get_mode_state() & MODE_AUTOMATIC_CYCLE_CALCULATE_DURATION) && (speed_state == SPEED_NONE)) {
			speed_state |= SPEED_AUTOMATIC_CYCLE_FORCE_NIGHT;
		}
	}

	if (get_mode_set() == MODE_AUTOMATIC_CYCLE) {

		if (!(get_mode_state() & (MODE_AUTOMATIC_CYCLE_EXTRA_CYCLE | MODE_AUTOMATIC_CYCLE_CALCULATE_DURATION))) {
			if (get_lux_set() != LUMINOSITY_SENSOR_SETTING_NOT_CONFIGURED) {
				if (luminosity > (luminosity_threshold + LUMINOSITY_DIFFERENTIAL_HIGH)) {
					if (count_luminosity) {
						count_luminosity--;
					} else {
						cond_flags &= ~COND_LUMINOSITY;
					}
				}

				if (luminosity <= (luminosity_threshold - LUMINOSITY_DIFFERENTIAL_LOW)) {
					if (count_luminosity < CONDITION_COUNT_MAX) {
						count_luminosity++;
					} else {
						cond_flags |= COND_LUMINOSITY;
					}
				}
			} else {
				cond_flags &= ~COND_LUMINOSITY;
				count_luminosity = 0U;
			}

			if (cond_flags & COND_LUMINOSITY) {
				speed_state |= SPEED_AUTOMATIC_CYCLE_FORCE_NIGHT;
			} else {
				speed_state &= ~SPEED_AUTOMATIC_CYCLE_FORCE_NIGHT;
			}

			if (get_relative_humidity_set() != RH_THRESHOLD_SETTING_NOT_CONFIGURED) {
				if (relative_humidity > (relative_humidity_threshold + RH_DIFFERENTIAL_HIGH)) {
					if (count_rh_extra_cycle < CONDITION_COUNT_MAX) {
						count_rh_extra_cycle++;
					} else {
						cond_flags |= COND_RH_EXTRA_CYCLE;
					}
				} else {
					cond_flags &= ~COND_RH_EXTRA_CYCLE;
					count_rh_extra_cycle = 0U;
				}
			} else {
				cond_flags &= ~COND_RH_EXTRA_CYCLE;
				count_rh_extra_cycle = 0U;
			}

			if (get_voc_set() != VOC_THRESHOLD_SETTING_NOT_CONFIGURED) {
				if (voc > (voc_threshold + VOC_DIFFERENTIAL_HIGH)) {
					if (count_voc_extra_cycle < CONDITION_COUNT_MAX) {
						count_voc_extra_cycle++;
					} else {
						cond_flags |= COND_VOC_EXTRA_CYCLE;
					}
				} else {
					cond_flags &= ~COND_VOC_EXTRA_CYCLE;
					count_voc_extra_cycle = 0U;
				}
			} else {
				cond_flags &= ~COND_VOC_EXTRA_CYCLE;
				count_voc_extra_cycle = 0U;
			}

			if (cond_flags & (COND_RH_EXTRA_CYCLE | COND_VOC_EXTRA_CYCLE)) {
				if (!(get_mode_state() & MODE_AUTOMATIC_CYCLE_EXTRA_CYCLE)) {
					if (xSemaphoreTake(extra_cycle_count_sem, 0) == pdPASS) {
						printf("MODE_AUTOMATIC_CYCLE_EXTRA_CYCLE - [%d]\n", EXTRA_CYCLE_COUNT_MAX - uxSemaphoreGetCount(extra_cycle_count_sem));
						set_mode_state(get_mode_state() | MODE_AUTOMATIC_CYCLE_EXTRA_CYCLE);
						if (speed_state > SPEED_NIGHT) {
							speed_state |= SPEED_AUTOMATIC_CYCLE_FORCE_BOOST;
						}
						xTaskCreate(work_task, "work task", CONTROLLER_TASK_STACK_SIZE, NULL, CONTROLLER_TASK_PRIORITY, NULL);
						if (!xTimerIsTimerActive(restart_extra_cycle_timer)) {
							xTimerStart(restart_extra_cycle_timer, 0);
						}
//							struct log_evt_extra_cycle event_data;
//							event_data.data.relative_humidity = relative_humidity;
//							event_data.data.voc = voc;
//							event_data.data.rh_extra_cycle_cond = cond_flags & COND_RH_EXTRA_CYCLE ? 1U : 0U;
//							event_data.data.voc_extra_cycle_cond = cond_flags & COND_VOC_EXTRA_CYCLE ? 1U : 0U;
//							event_data.data.extra_cycle_count = MASTER_EXTRA_CYCLE_COUNT_MAX - k_sem_count_get(&master_extra_cycle_count_sem);
//							log_save_event(EVENT_EXTRA_CYCLE, &event_data);

						cond_flags &= ~(COND_RH_EXTRA_CYCLE
								| COND_VOC_EXTRA_CYCLE);
						count_rh_extra_cycle = 0U;
						count_voc_extra_cycle = 0U;
					}
				}
			} else {
				speed_state &= ~SPEED_AUTOMATIC_CYCLE_FORCE_BOOST;
			}

		} else {
			cond_flags = 0U;
			count_luminosity = 0U;
			count_rh_extra_cycle = 0U;
			count_voc_extra_cycle = 0U;
		}
	} else {
		cond_flags = 0U;
		count_luminosity = 0U;
		count_rh_extra_cycle = 0U;
		count_voc_extra_cycle = 0U;
		speed_state &= ~(SPEED_AUTOMATIC_CYCLE_FORCE_BOOST
				| SPEED_AUTOMATIC_CYCLE_FORCE_NIGHT);
	}

	set_speed_state(speed_state);

}

static void reset_automatic_cycle_count(void) {
	calculate_duration_inversions_count = 0U;
	extra_cycle_inversions_count = 0U;
}

static void controller_timer_expiry(TimerHandle_t xTimer) {
	printf("Timer_Expired\n");
	xTaskCreate(work_task, "work task 1", CONTROLLER_TASK_STACK_SIZE, NULL,CONTROLLER_TASK_PRIORITY, NULL);

}

static void restart_automatic_cycle_timer_expiry(TimerHandle_t xTimer) {

	set_mode_state(get_mode_state() | MODE_AUTOMATIC_CYCLE_CALCULATE_DURATION);

	xTimerChangePeriod(restart_automatic_cycle_timer, pdMS_TO_TICKS(DURATION_IMMISSION_EMISSION_MS), portMAX_DELAY);
	xTimerStart(restart_automatic_cycle_timer, 0);

	printf("MODE_AUTOMATIC_CYCLE_CALCULATE_DURATION - RESET TIMER");
}

static void restart_extra_cycle_timer_expiry(TimerHandle_t xTimer) {
	while (uxSemaphoreGetCount(extra_cycle_count_sem) < EXTRA_CYCLE_COUNT_MAX) {
		xSemaphoreGive(extra_cycle_count_sem);
	}
	printf("MODE_AUTOMATIC_CYCLE_EXTRA_CYCLE - RESET COUNT\n");
}

static void work_task(void *arg) {
	uint8_t mode_state = get_mode_state();

	mode_state &= ~(MODE_AUTOMATIC_CYCLE_EXTRA_CYCLE | MODE_AUTOMATIC_CYCLE_CALCULATE_DURATION);
	switch (mode_state) {
	case MODE_OFF:
		break;

	case MODE_IMMISSION:
		set_mode_set(MODE_FIXED_CYCLE);
		break;

	case MODE_EMISSION:
		set_mode_set(MODE_FIXED_CYCLE);
		break;

	case MODE_FIXED_CYCLE:
		if (get_direction_state() == DIRECTION_OUT) {
			set_direction_state(DIRECTION_IN);
		} else if (get_direction_state() == DIRECTION_IN) {
			set_direction_state(DIRECTION_OUT);
		}
		xTimerChangePeriod(controller_timer, pdMS_TO_TICKS(DURATION_FIXED_CYCLE_MS), portMAX_DELAY);
		xTimerStart(controller_timer, 0);
		break;

	case MODE_AUTOMATIC_CYCLE:
		if (get_direction_state() == DIRECTION_OUT) {
			set_direction_state(DIRECTION_IN);
		} else if (get_direction_state() == DIRECTION_IN) {
			set_direction_state(DIRECTION_OUT);
		}

		if (get_mode_state() & MODE_AUTOMATIC_CYCLE_CALCULATE_DURATION) {
			// Extra cycle not started
			if (extra_cycle_inversions_count == 0U) {
				if (calculate_duration_inversions_count == 0U) {
					calculate_duration_inversions_count++;
					set_direction_state(DIRECTION_OUT);
					xTimerChangePeriod(controller_timer, pdMS_TO_TICKS(DURATION_AUTOMATIC_CYCLE_OUT_MS), portMAX_DELAY);
					xTimerStart(controller_timer, 0);
					printf("MODE_AUTOMATIC_CYCLE_CALCULATE_DURATION - START\n");
				} else if (calculate_duration_inversions_count <= CALCULATE_DURATION_INVERSIONS_MAX) {
					printf("inversione_count=%d\n", calculate_duration_inversions_count);
					calculate_duration_inversions_count++;
					xTimerChangePeriod(controller_timer, pdMS_TO_TICKS(DURATION_AUTOMATIC_CYCLE_IN_MS), portMAX_DELAY);
					xTimerStart(controller_timer, 0);
				} else {
					calculate_duration_inversions_count = 0U;
					set_automatic_cycle_duration( calculate_duration_automatic_cycle(get_internal_temperature(), get_external_temperature())); // VERIFY the functions inside arguments
					printf("internal_Emissione = %d\n", get_internal_temperature());
					printf("external_Imissione = %d\n", get_external_temperature());
					printf("duration = %d\n", get_automatic_cycle_duration());
//            			LOG_INF("Duration automatic cycle %" PRIu16 " sec - Emmission temperature %" PRId16 ".%02" PRId16 " �C - Immission temperature %" PRId16 ".%02" PRId16 " �C",
//            					get_automatic_cycle_duration(), get_emission_temperature() / TEMPERATURE_SCALE, abs(get_emission_temperature() % TEMPERATURE_SCALE), get_immission_temperature() / TEMPERATURE_SCALE, abs(get_immission_temperature() % TEMPERATURE_SCALE));
					duration_automatic_cycle = SECONDS_TO_MS( get_automatic_cycle_duration());
					set_mode_state( get_mode_state() & ~MODE_AUTOMATIC_CYCLE_CALCULATE_DURATION);
//						LOG_INF("MODE_AUTOMATIC_CYCLE_CALCULATE_DURATION - STOP");
//						struct log_evt_calculate_duration event_data;
//						event_data.data.emission_temperature = get_emission_temperature();
//						event_data.data.immission_temperature = get_immission_temperature();
//						event_data.data.automatic_cycle_duration = get_automatic_cycle_duration();
//						log_save_event(EVENT_CALCULATE_DURATION, &event_data);

				}
			}
		}

		if (get_mode_state() & MODE_AUTOMATIC_CYCLE_EXTRA_CYCLE) {
			// Calculate duration not started
			if (calculate_duration_inversions_count == 0U) {
				if (extra_cycle_inversions_count == 0U) {
					extra_cycle_inversions_count++;
					set_direction_state(DIRECTION_OUT);
					xTimerChangePeriod(controller_timer, pdMS_TO_TICKS(DURATION_EXTRA_CYCLE_BOOST_MS), portMAX_DELAY);
					xTimerStart(controller_timer, 0);
					printf("MODE_AUTOMATIC_CYCLE_EXTRA_CYCLE - START");
				} else if (extra_cycle_inversions_count <= EXTRA_CYCLE_INVERSIONS_MAX) {
					extra_cycle_inversions_count++;
					set_speed_state( get_speed_state() & ~SPEED_AUTOMATIC_CYCLE_FORCE_BOOST);
					xTimerChangePeriod(controller_timer, pdMS_TO_TICKS(DURATION_FIXED_CYCLE_MS), portMAX_DELAY);
					xTimerStart(controller_timer, 0);
				} else {
					extra_cycle_inversions_count = 0U;
					set_mode_state( get_mode_state() & ~MODE_AUTOMATIC_CYCLE_EXTRA_CYCLE);
					printf("MODE_AUTOMATIC_CYCLE_EXTRA_CYCLE - STOP");
				}
			}
		}

		// Calculate duration and Extra cycle not started
		if ((calculate_duration_inversions_count == 0U)
				&& (extra_cycle_inversions_count == 0U)) {
			xTimerChangePeriod(controller_timer, pdMS_TO_TICKS(duration_automatic_cycle), portMAX_DELAY); xTimerStart(controller_timer, 0);
		}

		break;

	default:
		break;
	}
	// Delete this task once the function finishes executing
	vTaskDelete(NULL);
}
int controller_init() {
	// Initialization of the semaphore
	extra_cycle_count_sem = xSemaphoreCreateCounting(EXTRA_CYCLE_COUNT_MAX, EXTRA_CYCLE_COUNT_MAX);

	// Create the timer
	controller_timer = xTimerCreate("controller_timer", pdMS_TO_TICKS(1000), pdFALSE, (void*) 0, controller_timer_expiry);

	restart_automatic_cycle_timer = xTimerCreate("restart_automatic_cycle_timer", pdMS_TO_TICKS(1000), pdFALSE, (void*) 0, restart_automatic_cycle_timer_expiry);

	restart_extra_cycle_timer = xTimerCreate("restart_extra_cycle_timer", pdMS_TO_TICKS(1000), pdFALSE, (void*) 0, restart_extra_cycle_timer_expiry);

	BaseType_t task_created = xTaskCreate(controller_task, "Controller task ", CONTROLLER_TASK_STACK_SIZE, NULL, CONTROLLER_TASK_PRIORITY, NULL);

	return task_created == pdPASS ? 0 : -1;
}
