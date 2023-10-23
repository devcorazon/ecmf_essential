/*
 * storage.h
 *
 *  Created on: 1 dic 2022
 *      Author: Daniele Schirosi
 */

#ifndef MAIN_INCLUDE_STORAGE_H_
#define MAIN_INCLUDE_STORAGE_H_

#include "structs.h"

int storage_init(void);
int storage_set_default(void);

/// runtime data
uint32_t get_serial_number(void);

uint16_t get_fw_version(void);

uint8_t get_mode_state(void);
int set_mode_state(uint8_t mode_state);

uint8_t get_speed_state(void);
int set_speed_state(uint8_t speed_state);

uint8_t get_direction_state(void);
int set_direction_state(uint8_t direction_state);

uint8_t get_device_state(void);
int set_device_state(uint8_t device_state);

int16_t get_temperature(void);
int set_temperature(int16_t temperature);

uint16_t get_relative_humidity(void);
int set_relative_humidity(uint16_t relative_humidity);

uint16_t get_voc(void);
int set_voc(uint16_t voc);

uint16_t get_lux(void);
int set_lux(uint16_t lux);

int16_t get_internal_temperature(void);
int set_internal_temperature(int16_t temperature);

int16_t get_external_temperature(void);
int set_external_temperature(int16_t temperature);

/// configuration settings
uint8_t get_mode_set(void);
int set_mode_set(uint8_t mode_set);

uint8_t get_speed_set(void);
int set_speed_set(uint8_t speed_set);

uint8_t get_relative_humidity_set(void);
int set_relative_humidity_set(uint8_t relative_humidity_set);

uint8_t get_lux_set(void);
int set_lux_set(uint8_t lux_set);

uint8_t get_voc_set(void);
int set_voc_set(uint8_t voc_set);

int16_t get_temperature_offset(void);
int set_temperature_offset(int16_t temperature_offset);

int16_t get_relative_humidity_offset(void);
int set_relative_humidity_offset(int16_t relative_humidity_offset);

uint16_t get_automatic_cycle_duration(void);
int set_automatic_cycle_duration(uint16_t automatic_cycle_duration);

uint32_t get_filter_operating(void);
int set_filter_operating(uint32_t filter_operating);

void get_ssid(uint8_t *ssid);
int set_ssid(const uint8_t *ssid);

void get_password(uint8_t *password);
int set_password(const uint8_t *password);

void get_server(uint8_t *server);
int set_server(const uint8_t *server);

void get_port(uint8_t *port);
int set_port(const uint8_t *port);

uint8_t get_wifi_active(void);
int set_wifi_active(uint8_t active);


#endif /* MAIN_INCLUDE_STORAGE_H_ */
