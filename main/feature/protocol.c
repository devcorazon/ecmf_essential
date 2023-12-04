/*
 * protocol.c
 *
 *  Created on: 4 nov. 2023
 *      Author: youcef.benakmoume
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include <lwip/def.h>

#include <storage.h>
#include <blufi.h>
#include "protocol.h"
#include "protocol_internal.h"

static uint8_t calculate_crc(const void *buf, size_t len);
static void proto_send_data(uint8_t funct, const void *buf, size_t len, uint8_t *out_data, size_t out_data_size);
static void proto_parse_query_data(const void *buf, uint8_t *out_data, size_t out_data_size);
static void proto_parse_write_data(const void *buf, uint8_t *out_data, size_t out_data_size);
static void proto_parse_execute_function_data(const void *buf, uint8_t *out_data, size_t out_data_size);

static uint8_t calculate_crc(const void *buf, size_t len) {
	uint8_t *data = (uint8_t *)buf;
	uint8_t crc = 0xff;
	uint8_t polynom = 0x31;

	for (size_t i = 0; i < len; i++) {
		crc = crc ^ *data++;
		for (size_t j = 0; j < 8; j++) {
			if (crc & 0x80) {
				crc = (crc << 1) ^ polynom;
			} else {
				crc = crc << 1;
			}
		}
	}

	return crc;
}

static void proto_send_data(uint8_t funct, const void *buf, size_t len, uint8_t *out_data, size_t out_data_size) {
    if (out_data == NULL || out_data_size < PROTOCOL_TRAME_FIX_LEN + len) {
        printf("Output buffer is null or not large enough\n");
        return;
    }

    size_t index = 0;

    // STX
    out_data[index++] = PROTOCOL_TRAME_STX;

    // ADDR
    uint32_t serial_num = convert_big_endian_32(get_serial_number());
    memcpy(&out_data[index], &serial_num, sizeof(serial_num));
    index += sizeof(serial_num);

    // LEN (STX and ETX excluded)
    uint16_t length_field = convert_big_endian_16(PROTOCOL_TRAME_WO_SE_FIX_LEN + len);
    memcpy(&out_data[index], &length_field, sizeof(length_field));
    index += sizeof(length_field);

    // FUNCT
    out_data[index++] = funct;

    // DATA
    if (buf != NULL && len > 0) {
        memcpy(&out_data[index], buf, len);
        index += len;
    }

    // CRC
    out_data[index++] = calculate_crc(&out_data[PROTOCOL_TRAME_ADDR_POS], PROTOCOL_TRAME_WO_SE_FIX_LEN + len - 1);

    // ETX
    out_data[index++] = PROTOCOL_TRAME_ETX;

    // Send trame
    if (tcp_send_data(out_data, index) < 0) {
        printf("Failed to send data\n");
    }

    return;
}


static void proto_send_ack(uint8_t ack_code, uint8_t funct_code, uint16_t add_data , uint8_t *out_data, size_t out_data_size) {
	struct protocol_ack_s ack;

	ack.ack_code = ack_code;
	ack.fuct_code = funct_code;
	ack.add_data = convert_big_endian_16(add_data);

	proto_send_data(PROTOCOL_FUNCT_ACK, &ack, sizeof(ack),out_data, out_data_size);
}

static void proto_send_nack(uint8_t nack_code, uint8_t funct_code, uint16_t add_data, uint8_t *out_data, size_t out_data_size) {
	struct protocol_nack_s nack;

	nack.nack_code = nack_code;
	nack.fuct_code = funct_code;
	nack.add_data = convert_big_endian_16(add_data);

	proto_send_data(PROTOCOL_FUNCT_NACK, &nack, sizeof(nack),out_data, out_data_size);
}

static void proto_send_answer_voluntary(uint8_t funct, uint16_t obj_id, uint16_t index, uint8_t *out_data, size_t out_data_size) {
	struct protocol_content_s content;
	size_t len;

	content.obj_id = convert_big_endian_16(obj_id);
	content.index = convert_big_endian_16(index);

	switch(obj_id) {
		case PROTOCOL_OBJID_INFO:
		{
			uint8_t bt_addr[BT_ADDRESS_LEN];
			uint8_t wifi_addr[WIFI_ADDRESS_LEN];

			blufi_get_ble_address(bt_addr);
			blufi_get_wifi_address(wifi_addr);

			custom_put_be32(get_serial_number(), content.data.info.serial_numebr);
			content.data.info.firmware_version = convert_big_endian_16(FIRMWARE_VERSION);
			sys_memcpy_swap(content.data.info.bt_address, bt_addr, sizeof(content.data.info.bt_address));
			content.data.info.bt_connection_number = blufi_get_ble_connection_number();
			content.data.info.bt_connection_state = blufi_get_ble_connection_state();
			sys_memcpy_swap(content.data.info.wifi_address, wifi_addr, sizeof(content.data.info.wifi_address));
			content.data.info.wifi_connection_state = blufi_get_wifi_connection_state();

			len = SIZEOF_CONTENT(content.data.info);
			break;
		}

		case PROTOCOL_OBJID_CONF:
		{
			uint8_t addr_srv[BT_ADDRESS_LEN];

//			get_slave_setting_server_address(addr_srv);

//			content.data.conf.role = get_role();
			content.data.conf.rh_setting = get_relative_humidity_set();
			content.data.conf.lux_setting = get_lux_set();
			content.data.conf.voc_setting = get_voc_set();
//			content.data.conf.fc_setting = (get_fc_enable_setting() << 4) | (get_fc_threshold_setting() & 0xf);
//			content.data.conf.rotation_setting = get_slave_rotation_setting();
			sys_memcpy_swap(content.data.conf.server_address, addr_srv, sizeof(content.data.conf.server_address));

			len = SIZEOF_CONTENT(content.data.conf);

			break;
		}

//		case PROTOCOL_OBJID_ADV_CONF:
//		{
//			content.data.adv_conf.temperature_offset = sys_be16_to_cpu(get_temperature_offset());
//			content.data.adv_conf.humidity_offset = sys_be16_to_cpu(get_humidity_offset());
//
//			len = SIZEOF_CONTENT(content.data.adv_conf);
//
//			break;
//		}

		case PROTOCOL_OBJID_WIFI_CONF:
		{
		    // Declare and initialize buffers for SSID, password, server, and port
		    uint8_t ssid[SSID_SIZE] = {0};  // Assuming SSID_SIZE is the maximum size of SSID
		    uint8_t pwd[PASSWORD_SIZE] = {0};  // Assuming PASSWORD_SIZE is the max size of password
		    uint8_t server[SERVER_SIZE] = {0};  // Assuming SERVER_SIZE is the max size of server address
		    uint8_t port[PORT_SIZE] = {0};  // Assuming PORT_SIZE is the max size for port

		    // Retrieve the current SSID, password, server, and port values
		    get_ssid(ssid);
		    get_password(pwd);
		    get_server(server);
		    get_port(port);

		    // Copy the retrieved values into the content structure
		    memcpy(content.data.wifi_conf.ssid, ssid, SSID_SIZE);
		    memcpy(content.data.wifi_conf.pwd, pwd, PASSWORD_SIZE);
		    memcpy(content.data.wifi_conf.server, server, SERVER_SIZE);
		    memcpy(content.data.wifi_conf.port, port, PORT_SIZE);

		    // Calculate the length of the content
		    len = SIZEOF_CONTENT(content.data.wifi_conf);

		    break;
		}

//		case PROTOCOL_OBJID_PROFILE:
//		{
//			for (size_t idx = 0U; idx < DAYS_PER_WEEK; idx++) {
//				get_profile(content.data.prof.profile[idx], idx);
//			}
//
//			len = SIZEOF_CONTENT(content.data.prof);
//
//			break;
//		}

//		case PROTOCOL_OBJID_CLOCK:
//		{
//			date_time_ts date_time;
//
//			calendar_get_time(&date_time);
//
//			content.data.clock.year = date_time.year;
//			content.data.clock.month = date_time.month;
//			content.data.clock.day = date_time.day;
//			content.data.clock.dow = date_time.dow;
//			content.data.clock.hour = date_time.hour;
//			content.data.clock.minute = date_time.minute;
//			content.data.clock.second = date_time.second;
//			content.data.clock.daylight_savings_time = get_daylight_savings_time();
//
//			len = SIZEOF_CONTENT(content.data.clock);
//
//			break;
//		}

		case PROTOCOL_OBJID_OPER:
		{
			content.data.oper.mode_setting = get_mode_set();
			content.data.oper.speed_setting = get_speed_set();

			len = SIZEOF_CONTENT(content.data.oper);
			break;
		}

//		case PROTOCOL_OBJID_STATS:
//		{
//			statistics_ts statistics;
//
//			if (index == 0) {
//				get_statistics_current(&statistics);
//			}
//			else {
//				statistics_load_data(get_statistic_idx() - index + 1U, &statistics);
//			}
//
//			content.data.stats.year = statistics.date_time.year;
//			content.data.stats.month = statistics.date_time.month;
//			content.data.stats.day = statistics.date_time.day;
//
//			content.data.stats.none_daily_hour = sys_be16_to_cpu(statistics.speed_counters_daily_hour.none);
//			content.data.stats.night_daily_hour = sys_be16_to_cpu(statistics.speed_counters_daily_hour.night);
//			content.data.stats.low_daily_hour = sys_be16_to_cpu(statistics.speed_counters_daily_hour.low);
//			content.data.stats.medium_daily_hour = sys_be16_to_cpu(statistics.speed_counters_daily_hour.medium);
//			content.data.stats.high_daily_hour = sys_be16_to_cpu(statistics.speed_counters_daily_hour.high);
//			content.data.stats.boost_daily_hour = sys_be16_to_cpu(statistics.speed_counters_daily_hour.boost);
//
//			content.data.stats.none_tot_hour = sys_be16_to_cpu(statistics.speed_counters_tot_hour.none);
//			content.data.stats.night_tot_hour = sys_be16_to_cpu(statistics.speed_counters_tot_hour.night);
//			content.data.stats.low_tot_hour = sys_be16_to_cpu(statistics.speed_counters_tot_hour.low);
//			content.data.stats.medium_tot_hour = sys_be16_to_cpu(statistics.speed_counters_tot_hour.medium);
//			content.data.stats.high_tot_hour = sys_be16_to_cpu(statistics.speed_counters_tot_hour.high);
//			content.data.stats.boost_tot_hour = sys_be16_to_cpu(statistics.speed_counters_tot_hour.boost);
//
//			for (size_t i = 0; i < QUARTERS_HOUR_PER_DAY; i++) {
//				content.data.stats.temperature_quarter[i] = sys_be16_to_cpu(statistics.statistics_measures[i].temperature);
//				content.data.stats.relative_humidity_quarter[i] = (statistics.statistics_measures[i].relative_humidity / HUMIDITY_SCALE) + ((statistics.statistics_measures[i].relative_humidity % HUMIDITY_SCALE) < (HUMIDITY_SCALE / 2U) ? 0U : 1U);
//				content.data.stats.voc_quarter[i] = sys_be16_to_cpu(statistics.statistics_measures[i].voc);
//			}
//
//			len = SIZEOF_CONTENT(content.data.stats);
//
//			break;
//		}

//		case PROTOCOL_OBJID_MASTER_STATE:
//		{
//			content.data.master_state.master_mode_state = get_master_mode_state();
//			content.data.master_state.master_speed_state = get_master_speed_state();
//			content.data.master_state.master_direction_state = get_master_direction_state();
//			content.data.master_state.automatic_cycle_duration = convert_big_endian_16(get_automatic_cycle_duration());
//
//			len = SIZEOF_CONTENT(content.data.master_state);
//
//			break;
//		}

		case PROTOCOL_OBJID_STATE:
		{
			content.data.state.mode_state = get_mode_state();
			content.data.state.speed_state = get_speed_state();
			content.data.state.direction_state = get_direction_state();
			content.data.state.ambient_temperature = convert_big_endian_16(get_temperature());
			content.data.state.relative_humidity = convert_big_endian_16(get_relative_humidity());
			content.data.state.voc = convert_big_endian_16(get_voc());

			len = SIZEOF_CONTENT(content.data.state);

			break;
		}

		default:
			return;
	}

	proto_send_data(funct, &content, len, out_data, out_data_size);


}

//static void proto_send_identification(void) {
//	struct protocol_identification_s identification;
//
//	identification.device_code = convert_big_endian_16(ECMF_IR_DEVICE_CODE);
//	identification.serial_number = convert_big_endian_32(get_serial_number());
//	proto_send_data(PROTOCOL_FUNCT_IDENTIFICATION, identification, sizeof(identification), out_data, out_data_size);
//}



static void proto_parse_query_data(const void *buf, uint8_t *out_data, size_t out_data_size) {
	struct protocol_content_s *content = (struct protocol_content_s *)buf;
	uint16_t obj_id;
	uint16_t index;

	obj_id = convert_big_endian_16(content->obj_id);
	index = convert_big_endian_32(content->index);

	if ((obj_id != PROTOCOL_OBJID_INFO) && (obj_id != PROTOCOL_OBJID_CONF) && (obj_id != PROTOCOL_OBJID_ADV_CONF) &&
		(obj_id != PROTOCOL_OBJID_WIFI_CONF) && (obj_id != PROTOCOL_OBJID_PROFILE) && (obj_id != PROTOCOL_OBJID_CLOCK) &&
		(obj_id != PROTOCOL_OBJID_OPER) && (obj_id != PROTOCOL_OBJID_STATS) && (obj_id != PROTOCOL_OBJID_MASTER_STATE) && (obj_id != PROTOCOL_OBJID_STATE)) {

		proto_send_nack(PROTOCOL_NACK_CODE_QUERY_ERR, PROTOCOL_FUNCT_QUERY, obj_id, out_data, out_data_size);
		return;
	}

//	if (obj_id == PROTOCOL_OBJID_STATS) {
//		if ((index > statistic_data_max()) || ((get_statistic_idx() < statistic_data_max()) && (index > get_statistic_idx()))) {
//			proto_send_nack(PROTOCOL_NACK_CODE_STATS_ERR, PROTOCOL_FUNCT_QUERY, (get_statistic_idx() < statistic_data_max()) ? get_statistic_idx() : statistic_data_max());
//	    	return;
//		}
//	}

    proto_send_answer_voluntary(PROTOCOL_FUNCT_ANSWER, obj_id, index, out_data, out_data_size);

}

static void proto_parse_write_data(const void *buf, uint8_t *out_data, size_t out_data_size) {
	struct protocol_content_s *content = (struct protocol_content_s *)buf;
	uint16_t obj_id;
	uint16_t index;

	obj_id = convert_big_endian_16(content->obj_id);
	index = convert_big_endian_16(content->index);

	switch (obj_id) {
//		case PROTOCOL_OBJID_CONF:
//		{
//#warning
//			if ((content->data.conf.role > ROLE_SLAVE) && (content->data.conf.role != UNMODIFIED_VALUE)) {
//				proto_send_nack(PROTOCOL_NACK_CODE_WRITE_ERR, PROTOCOL_FUNCT_WRITE, obj_id);
//				return;
//			}
//
//			if ((content->data.conf.rh_setting != RH_THRESHOLD_SETTING_NOT_CONFIGURED) &&
//				((content->data.conf.rh_setting & ~RH_SETTING_ADV_CTRL) < RH_THRESHOLD_SETTING_LOW) &&
//				((content->data.conf.rh_setting & ~RH_SETTING_ADV_CTRL) > RH_THRESHOLD_SETTING_HIGH) &&
//				(content->data.conf.rh_setting != UNMODIFIED_VALUE)) {
//
//				proto_send_nack(PROTOCOL_NACK_CODE_WRITE_ERR, PROTOCOL_FUNCT_WRITE, obj_id);
//				return;
//			}
//
//#warning ???
//			if ((content->data.conf.lux_setting > LUX_THRESHOLD_SETTING_HIGH) && (content->data.conf.lux_setting != UNMODIFIED_VALUE)) {
//				proto_send_nack(PROTOCOL_NACK_CODE_WRITE_ERR, PROTOCOL_FUNCT_WRITE, obj_id);
//				return;
//			}
//
//			if ((content->data.conf.voc_setting != VOC_THRESHOLD_SETTING_NOT_CONFIGURED) &&
//				((content->data.conf.voc_setting & ~VOC_THRESHOLD_SETTING_ADV_CTRL) < VOC_THRESHOLD_SETTING_LOW) &&
//				((content->data.conf.voc_setting & ~VOC_THRESHOLD_SETTING_ADV_CTRL) > VOC_THRESHOLD_SETTING_HIGH) &&
//				(content->data.conf.voc_setting != UNMODIFIED_VALUE)) {
//
//				proto_send_nack(PROTOCOL_NACK_CODE_WRITE_ERR, PROTOCOL_FUNCT_WRITE, obj_id);
//				return;
//			}
//
//			if ((content->data.conf.fc_setting != 0) && ((((content->data.conf.fc_setting & 0xf) > FC_THRESHOLD_SETTING_HIGH) && ((content->data.conf.fc_setting & 0xf) != 0xf)) || (((content->data.conf.fc_setting >> 4) > FC_ENABLE) && ((content->data.conf.fc_setting >> 4) != 0x7))) && (content->data.conf.fc_setting != UNMODIFIED_VALUE)) {
//				proto_send_nack(PROTOCOL_NACK_CODE_WRITE_ERR, PROTOCOL_FUNCT_WRITE, obj_id);
//				return;
//			}
//
//			if ((content->data.conf.rotation_setting > ROTATION_SETTING_OPPOSITE) && (content->data.conf.rotation_setting != UNMODIFIED_VALUE)) {
//				proto_send_nack(PROTOCOL_NACK_CODE_WRITE_ERR, PROTOCOL_FUNCT_WRITE, obj_id);
//				return;
//			}
//
//#warning
//			if (((get_role() == ROLE_MASTER) && (content->data.conf.role == ROLE_SLAVE)) ||
//				((get_role() == ROLE_SLAVE) && (content->data.conf.role == ROLE_MASTER))) {
//
//				proto_send_nack(PROTOCOL_NACK_CODE_WRITE_ERR, PROTOCOL_FUNCT_WRITE, obj_id);
//				return;
//			}
//
//#warning
//		    if (content->data.conf.role != UNMODIFIED_VALUE) {
//				set_role(content->data.conf.role);
//			}
//			if (content->data.conf.rh_setting != UNMODIFIED_VALUE) {
//				set_rh_threshold_setting(content->data.conf.rh_setting);
//			}
//			if (content->data.conf.lux_setting != UNMODIFIED_VALUE) {
//				set_luminosity_sensor_setting(content->data.conf.lux_setting);
//			}
//			if (content->data.conf.voc_setting != UNMODIFIED_VALUE) {
//				set_voc_threshold_setting(content->data.conf.voc_setting);
//			}
//#warning
//			if (content->data.conf.fc_setting != UNMODIFIED_VALUE) {
//				if ((content->data.conf.fc_setting & 0xf) != 0xf) {
//					set_fc_threshold_setting(content->data.conf.fc_setting & 0xf);
//				}
//				if ((content->data.conf.fc_setting >> 4) != 0x7) {
//					set_fc_enable_setting(content->data.conf.fc_setting >> 4);
//				}
//			}
//
//#warning
//			switch (get_role()) {
//				case ROLE_NOT_CONFIGURED:
//				{
//					set_slave_rotation_setting(ROTATION_SETTING_NOT_CONFIGURED);
//					set_slave_setting_server_address(BT_ADDRESS_ANY);
//
//				    set_mode_setting(MODE_OFF);
//					set_speed_setting(SPEED_NONE);
//					set_master_mode_state(MODE_OFF);
//					set_master_speed_state(SPEED_NONE);
//					set_master_direction_state(DIRECTION_NONE);
//
//					bluetooth_stop_scan();
//					bluetooth_clear_bond_ecmf2();
//
//					break;
//				}
//
//		case ROLE_MASTER: {
//			set_slave_rotation_setting(ROTATION_SETTING_NOT_CONFIGURED);
//			set_slave_setting_server_address(BT_ADDRESS_ANY);
//
//			break;
//		}
//
//		case ROLE_SLAVE: {
//			uint8_t addr[BT_ADDRESS_LEN];
//			uint8_t addr_srv[BT_ADDRESS_LEN];
//
//			get_slave_setting_server_address(addr_srv);
//
//			sys_memcpy_swap(addr, content->data.conf.server_address,
//					sizeof(addr));
//
//			if (content->data.conf.rotation_setting != UNMODIFIED_VALUE) {
//				set_slave_rotation_setting(content->data.conf.rotation_setting);
//			}
//			if ((memcmp(addr, BT_ADDRESS_ANY, sizeof(addr)) != 0)
//					&& (memcmp(addr, addr_srv, sizeof(addr)) != 0)) {
//				set_slave_setting_server_address(addr);
//
//				bluetooth_stop_scan();
//				bluetooth_clear_bond_ecmf2();
//				bluetooth_start_scan();
//			}
//
//			break;
//		}
//			}
//
//		    break;
//		}
//
//		case PROTOCOL_OBJID_ADV_CONF:
//		{
//			int16_t temperature_offset;
//			int16_t humidity_offset;
//
//		    temperature_offset = sys_cpu_to_be16(content->data.adv_conf.temperature_offset);
//		    humidity_offset = sys_cpu_to_be16(content->data.adv_conf.humidity_offset);
//
//			if ((((temperature_offset < OFFSET_BOUND_MIN) || (temperature_offset > OFFSET_BOUND_MAX)) && (temperature_offset != UNMODIFIED_VALUE_LONG)) ||
//				(((humidity_offset < OFFSET_BOUND_MIN) || (humidity_offset > OFFSET_BOUND_MAX)) && (humidity_offset != UNMODIFIED_VALUE_LONG))) {
//
//				proto_send_nack(PROTOCOL_NACK_CODE_WRITE_ERR, PROTOCOL_FUNCT_WRITE, obj_id);
//				return;
//			}
//
//		    if (temperature_offset != UNMODIFIED_VALUE_LONG) {
//		        set_temperature_offset(temperature_offset);
//		    }
//		    if (humidity_offset != UNMODIFIED_VALUE_LONG) {
//		    	set_relative_humidity_offset(humidity_offset);
//		    }
//
//		    break;
//		}
//
//		case PROTOCOL_OBJID_WIFI_CONF:
//		{
//		    uint32_t port;
//		    uint16_t period;
//
//		    // Convert string to long integer for port
//		    port = strtol((const char *)content->data.wifi_conf.port, NULL, 10);
//
//		    // Convert period to big-endian format
//		    period = htons(content->data.wifi_conf.period);
//
//		    // Validate port and period values
//		    if ((((port < WIFI_PORT_BOUND_MIN) || (port > WIFI_PORT_BOUND_MAX)) && (port != UNMODIFIED_VALUE_LONG)) ||
//		        ((period != 0) && ((period < WIFI_PERIOD_BOUND_MIN) || (period > WIFI_PERIOD_BOUND_MAX)) && (period != UNMODIFIED_VALUE_LONG))) {
//		        proto_send_nack(PROTOCOL_NACK_CODE_WRITE_ERR, PROTOCOL_FUNCT_WRITE, obj_id);
//		    }
//
//		    // Configuration logic (currently commented out)
//		    #if 0
//		    if (memcmp(content->data.wifi_conf.ssid, WIFI_SSID_ANY, WIFI_SSID_LEN)) {
//		        set_wifi_ssid(content->data.wifi_conf.ssid);
//		    }
//
//		    if (memcmp(content->data.wifi_conf.psk, WIFI_PSK_ANY, WIFI_PSK_LEN)) {
//		        set_wifi_psk(content->data.wifi_conf.psk);
//		    }
//		    #endif
//
//		    // Set WiFi server
//		    if (memcmp(content->data.wifi_conf.server, WIFI_SERVER_ANY, WIFI_SERVER_LEN)) {
//		        set_wifi_server(content->data.wifi_conf.server);
//		    }
//
//		    // Set WiFi port
//		    if (memcmp(content->data.wifi_conf.port, WIFI_PORT_ANY, WIFI_PORT_LEN)) {
//		        set_wifi_port(content->data.wifi_conf.port);
//		    }
//
//		    // Set WiFi period
//		    if (period != UNMODIFIED_VALUE_LONG) {
//		        set_wifi_period(period);
//		    }
//
//		    // Optionally log the event
//		    // log_save_event(EVENT_WIFI_CONFIGURATION_SETTINGS, NULL);
//
//		    break;
//		}
//
//		case PROTOCOL_OBJID_PROFILE:
//		{
//			for (size_t idx = 0U; idx < DAYS_PER_WEEK; idx++) {
//				if (memcmp(content->data.prof.profile[idx], PROFILE_ANY, HOURS_PER_DAY) != 0) {
//					set_profile(content->data.prof.profile[idx], idx);
//				}
//			}
//
//			break;
//		}
//
//#warning
//		case PROTOCOL_OBJID_CLOCK:
//		{
//			date_time_ts date_time;
//
//			if (((content->data.clock.month < 1) || (content->data.clock.month > 12)) ||
//				((content->data.clock.day < 1) || (content->data.clock.day > calendar_days_of_month(content->data.clock.year, content->data.clock.month))) ||
//				((content->data.clock.hour > 23) || (content->data.clock.minute > 59) || (content->data.clock.second > 59))) {
//
//				proto_send_nack(PROTOCOL_NACK_CODE_WRITE_ERR, PROTOCOL_FUNCT_WRITE, obj_id);
//				return;
//			}
//
//			date_time.year   = content->data.clock.year;
//			date_time.month  = content->data.clock.month;
//			date_time.day    = content->data.clock.day;
//			date_time.dow    = content->data.clock.dow;
//			date_time.hour   = content->data.clock.hour;
//			date_time.minute = content->data.clock.minute;
//			date_time.second = content->data.clock.second;
//
//		    set_daylight_savings_time(content->data.clock.daylight_savings_time);
//		    calendar_set_time(&date_time);
//
//			break;
//		}
//
		case PROTOCOL_OBJID_OPER:
		{

			if (content->data.oper.mode_setting > MODE_AUTOMATIC_CYCLE) {
				proto_send_nack(PROTOCOL_NACK_CODE_WRITE_ERR, PROTOCOL_FUNCT_WRITE, obj_id , out_data, out_data_size);
				return;
			}

//#warning UNMODIFIED_VALUE
			if (((content->data.oper.mode_setting != MODE_AUTOMATIC_CYCLE) && (content->data.oper.speed_setting > SPEED_HIGH)) ||
				((content->data.oper.mode_setting == MODE_AUTOMATIC_CYCLE) && (content->data.oper.speed_setting > SPEED_HIGH))) {
		//	&& (content->data.oper.speed_setting != SPEED_PROLIFED))) {

				proto_send_nack(PROTOCOL_NACK_CODE_WRITE_ERR, PROTOCOL_FUNCT_WRITE, obj_id , out_data, out_data_size);
				return;
			}

			set_mode_set(content->data.oper.mode_setting);
			set_speed_set(content->data.oper.speed_setting);
			break;
		}
//
		default:
			proto_send_nack(PROTOCOL_NACK_CODE_WRITE_ERR, PROTOCOL_FUNCT_WRITE, obj_id , out_data, out_data_size);
			return;
	}
//
	proto_send_ack(PROTOCOL_ACK_CODE_WRITE_OK, PROTOCOL_FUNCT_WRITE, obj_id , out_data, out_data_size);
}


static void proto_parse_execute_function_data(const void *buf, uint8_t *out_data, size_t out_data_size) {
    uint8_t *data = (uint8_t *)buf;
    uint16_t exec_funct_id = convert_big_endian_16(*(uint16_t*)&data[0]);
    uint8_t *add_data = &data[2];

    switch (exec_funct_id) {
        case PROTOCOL_EXEC_F_CLOSE_SOCK:
        {
            uint32_t period = convert_big_endian_32(*(uint32_t*)add_data) + 30U;
            printf("F_CLOSE_SOCK_PERIOD: %lu\n", (unsigned long)period);
            break;
        }

        default:
            proto_send_nack(PROTOCOL_NACK_CODE_EXEC_F_ERR, PROTOCOL_FUNCT_EXECUTE_FUNCTION, exec_funct_id, out_data, out_data_size);
            return;
    }

    proto_send_ack(PROTOCOL_ACK_CODE_EXEC_F_OK, PROTOCOL_FUNCT_EXECUTE_FUNCTION, exec_funct_id, out_data, out_data_size);
}

int proto_elaborate_data(uint8_t *in_data, size_t in_data_size,uint8_t *out_data, size_t out_data_size) {
    int processed = 0;

    for (size_t idx = 0; idx < in_data_size; ++idx) {
        if ((in_data[idx] == PROTOCOL_TRAME_STX) && (in_data_size >= (idx + PROTOCOL_TRAME_FUNCT_POS))) {
            uint32_t address = (in_data[idx + PROTOCOL_TRAME_ADDR_POS] << 24) |
                               (in_data[idx + PROTOCOL_TRAME_ADDR_POS + 1] << 16) |
                               (in_data[idx + PROTOCOL_TRAME_ADDR_POS + 2] << 8) |
                               (in_data[idx + PROTOCOL_TRAME_ADDR_POS + 3]);

            uint16_t length = (in_data[idx + PROTOCOL_TRAME_LENGHT_POS] << 8) |
                              (in_data[idx + PROTOCOL_TRAME_LENGHT_POS + 1]);

            length += 2U;

            if ((in_data_size >= (idx + length)) && (in_data[idx + length - 1U] == PROTOCOL_TRAME_ETX)) {
                uint8_t crc = in_data[idx + length - 2U];

                // CRC validation (uncomment to enable)
                printf("CRC: %02x\n", calculate_crc(&in_data[idx + PROTOCOL_TRAME_ADDR_POS], length - 3U));

                if ((address == get_serial_number()) && (crc == calculate_crc(&in_data[idx + PROTOCOL_TRAME_ADDR_POS], length - 3U))) {
                    // Debug log (uncomment to enable)
                    // printf("Received data: "); // Followed by hex dump code

                    uint8_t funct = in_data[idx + PROTOCOL_TRAME_FUNCT_POS];

                    printf("length:%d, address:%d, function:%d, crc:%d\n", length, address, funct, crc);

                    switch (funct) {
                        case PROTOCOL_FUNCT_QUERY:
                            proto_parse_query_data(&in_data[idx + PROTOCOL_TRAME_DATA_POS], out_data, out_data_size);
                            break;

                        case PROTOCOL_FUNCT_WRITE:
                             proto_parse_write_data(&in_data[idx + PROTOCOL_TRAME_DATA_POS], out_data, out_data_size);
                            break;

                        case PROTOCOL_FUNCT_EXECUTE_FUNCTION:
                             proto_parse_execute_function_data(&in_data[idx + PROTOCOL_TRAME_DATA_POS], out_data, out_data_size);
                            break;

                        default:
                            proto_send_nack(PROTOCOL_NACK_CODE_GENERIC_ERR, funct, 0U , out_data, out_data_size);
                            break;
                    }
                    processed = 1;
                    break;
                }
            }
        }
    }

    return processed ? 0 : -1;
}
