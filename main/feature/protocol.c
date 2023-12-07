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

static uint8_t calculate_crc(const void *buf, size_t len);
static void proto_prepare_trame(uint8_t funct, const void *buf, size_t len, uint8_t *out_data, size_t *out_data_size);
static void proto_parse_query_data(const void *buf, uint8_t *out_data, size_t *out_data_size);
static void proto_parse_write_data(const void *buf, uint8_t *out_data, size_t *out_data_size);
static void proto_parse_execute_function_data(const void *buf, uint8_t *out_data, size_t *out_data_size);

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

static void proto_prepare_trame(uint8_t funct, const void *buf, size_t len, uint8_t *out_data, size_t *out_data_size) {
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

    if (out_data_size != NULL) {
        *out_data_size = index;
    }

    return;
}


static void proto_prepare_ack(uint8_t ack_code, uint8_t funct_code, uint16_t add_data , uint8_t *out_data, size_t *out_data_size) {
	struct protocol_ack_s ack;

	ack.ack_code = ack_code;
	ack.fuct_code = funct_code;
	ack.add_data = convert_big_endian_16(add_data);

	proto_prepare_trame(PROTOCOL_FUNCT_ACK, &ack, sizeof(ack),out_data, out_data_size);
}

static void proto_prepare_nack(uint8_t nack_code, uint8_t funct_code, uint16_t add_data, uint8_t *out_data, size_t *out_data_size) {
	struct protocol_nack_s nack;

	nack.nack_code = nack_code;
	nack.fuct_code = funct_code;
	nack.add_data = convert_big_endian_16(add_data);

	proto_prepare_trame(PROTOCOL_FUNCT_NACK, &nack, sizeof(nack),out_data, out_data_size);
}

static void proto_prepare_answer_voluntary(uint8_t funct, uint16_t obj_id, uint16_t index, uint8_t *out_data, size_t *out_data_size) {
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
			uint8_t addr_srv[BT_ADDRESS_LEN] = { 0x00 };

			content.data.conf.role = 0x01;
			content.data.conf.rh_setting = get_relative_humidity_set();
			content.data.conf.lux_setting = get_lux_set();
			content.data.conf.voc_setting = get_voc_set();
			content.data.conf.fc_setting = 0x00;
			content.data.conf.rotation_setting = 0x00;
			sys_memcpy_swap(content.data.conf.server_address, addr_srv, sizeof(content.data.conf.server_address));

			len = SIZEOF_CONTENT(content.data.conf);

			break;
		}

		case PROTOCOL_OBJID_ADV_CONF:
		{
			content.data.adv_conf.temperature_offset = convert_big_endian_16(get_temperature_offset());
			content.data.adv_conf.humidity_offset = convert_big_endian_16(get_relative_humidity_offset());

			len = SIZEOF_CONTENT(content.data.adv_conf);

			break;
		}

		case PROTOCOL_OBJID_WIFI_CONF:
		{
		    // Declare and initialize buffers for SSID, password, server, and port
		    uint8_t ssid[SSID_SIZE + 1] = { 0 };
		    uint8_t pwd[PASSWORD_SIZE + 1] = { 0 };
		    uint8_t server[SERVER_SIZE + 1] = { 0 };
		    uint8_t port[PORT_SIZE  + 1] = { 0 };

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

		case PROTOCOL_OBJID_PROFILE:
		{
			for (size_t idx = 0U; idx < DAYS_PER_WEEK; idx++) {
				memset(content.data.prof.profile[idx],0,sizeof(content.data.prof.profile[idx]));
			}

			len = SIZEOF_CONTENT(content.data.prof);

			break;
		}

		case PROTOCOL_OBJID_CLOCK:
		{

			content.data.clock.year = 0x00;
			content.data.clock.month = 0x00;
			content.data.clock.day = 0x00;
			content.data.clock.dow = 0x00;
			content.data.clock.hour = 0x00;
			content.data.clock.minute = 0x00;
			content.data.clock.second = 0x00;
			content.data.clock.daylight_savings_time = 0x00;

			len = SIZEOF_CONTENT(content.data.clock);

			break;
		}

		case PROTOCOL_OBJID_OPER:
		{
			content.data.oper.mode_setting = get_mode_set();
			content.data.oper.speed_setting = get_speed_set();

			len = SIZEOF_CONTENT(content.data.oper);
			break;
		}

		case PROTOCOL_OBJID_STATS:
		{
			content.data.stats.year = 0x00;
			content.data.stats.month = 0x00;
			content.data.stats.day = 0x00;

			content.data.stats.none_daily_hour = 0x00;
			content.data.stats.night_daily_hour = 0x00;
			content.data.stats.low_daily_hour = 0x00;
			content.data.stats.medium_daily_hour = 0x00;
			content.data.stats.high_daily_hour = 0x00;
			content.data.stats.boost_daily_hour = 0x00;

			content.data.stats.none_tot_hour = 0x00;
			content.data.stats.night_tot_hour = 0x00;
			content.data.stats.low_tot_hour = 0x00;
			content.data.stats.medium_tot_hour = 0x00;
			content.data.stats.high_tot_hour = 0x00;
			content.data.stats.boost_tot_hour = 0x00;

			for (size_t i = 0; i < QUARTERS_HOUR_PER_DAY; i++) {
				content.data.stats.temperature_quarter[i] = 0x00;
				content.data.stats.relative_humidity_quarter[i] = 0x00;
				content.data.stats.voc_quarter[i] = 0x00;
			}

			len = SIZEOF_CONTENT(content.data.stats);

			break;
		}

		case PROTOCOL_OBJID_MASTER_STATE:
		{
			content.data.master_state.master_mode_state = get_mode_state();
			content.data.master_state.master_speed_state = get_speed_state();
			content.data.master_state.master_direction_state = get_direction_state();
			content.data.master_state.automatic_cycle_duration = convert_big_endian_16(get_automatic_cycle_duration());

			len = SIZEOF_CONTENT(content.data.master_state);

			break;
		}

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

	proto_prepare_trame(funct, &content, len, out_data, out_data_size);
}

int proto_prepare_identification(uint8_t *out_data, size_t *out_data_size) {
	struct protocol_identification_s identification;

	identification.device_code = convert_big_endian_16(ECMF_IR_DEVICE_CODE);
	identification.serial_number = convert_big_endian_32(get_serial_number());
	proto_prepare_trame(PROTOCOL_FUNCT_IDENTIFICATION, &identification, sizeof(identification), out_data, out_data_size);

	return 0;
}


static void proto_parse_query_data(const void *buf, uint8_t *out_data, size_t *out_data_size) {
	struct protocol_content_s *content = (struct protocol_content_s *)buf;
	uint16_t obj_id;
	uint16_t index;

	obj_id = convert_big_endian_16(content->obj_id);
	index = convert_big_endian_32(content->index);

	if ((obj_id != PROTOCOL_OBJID_INFO) && (obj_id != PROTOCOL_OBJID_CONF) && (obj_id != PROTOCOL_OBJID_ADV_CONF) &&
		(obj_id != PROTOCOL_OBJID_WIFI_CONF) && (obj_id != PROTOCOL_OBJID_PROFILE) && (obj_id != PROTOCOL_OBJID_CLOCK) &&
		(obj_id != PROTOCOL_OBJID_OPER) && (obj_id != PROTOCOL_OBJID_STATS) && (obj_id != PROTOCOL_OBJID_MASTER_STATE) && (obj_id != PROTOCOL_OBJID_STATE)) {

		proto_prepare_nack(PROTOCOL_NACK_CODE_QUERY_ERR, PROTOCOL_FUNCT_QUERY, obj_id, out_data, out_data_size);
		return;
	}

	if (obj_id == PROTOCOL_OBJID_STATS) {
		proto_prepare_nack(PROTOCOL_NACK_CODE_STATS_ERR, PROTOCOL_FUNCT_QUERY, PROTOCOL_OBJID_STATS , out_data, out_data_size);
	    return;
	}

    proto_prepare_answer_voluntary(PROTOCOL_FUNCT_ANSWER, obj_id, index, out_data, out_data_size);

}

static void proto_parse_write_data(const void *buf, uint8_t *out_data, size_t *out_data_size) {
	struct protocol_content_s *content = (struct protocol_content_s *)buf;
	uint16_t obj_id;
	uint16_t index;

	obj_id = convert_big_endian_16(content->obj_id);
	index = convert_big_endian_16(content->index);

	switch (obj_id) {
		case PROTOCOL_OBJID_CONF:
		{
			if ((content->data.conf.role > 0x01 ) && (content->data.conf.role != VALUE_UNMODIFIED)) {
				proto_prepare_nack(PROTOCOL_NACK_CODE_WRITE_ERR, PROTOCOL_FUNCT_WRITE, obj_id, out_data, out_data_size);
				return;
			}

			if ((content->data.conf.rh_setting != RH_THRESHOLD_SETTING_NOT_CONFIGURED) &&
				(content->data.conf.rh_setting  < RH_THRESHOLD_SETTING_LOW) &&
				(content->data.conf.rh_setting  > RH_THRESHOLD_SETTING_HIGH) &&
				(content->data.conf.rh_setting != VALUE_UNMODIFIED)) {

				proto_prepare_nack(PROTOCOL_NACK_CODE_WRITE_ERR, PROTOCOL_FUNCT_WRITE, obj_id, out_data, out_data_size);
				return;
			}

			if ((content->data.conf.lux_setting > LUX_THRESHOLD_SETTING_HIGH) && (content->data.conf.lux_setting != VALUE_UNMODIFIED)) {
				proto_prepare_nack(PROTOCOL_NACK_CODE_WRITE_ERR, PROTOCOL_FUNCT_WRITE, obj_id, out_data, out_data_size);
				return;
			}

			if ((content->data.conf.voc_setting != VOC_THRESHOLD_SETTING_NOT_CONFIGURED) &&
				(content->data.conf.voc_setting  < VOC_THRESHOLD_SETTING_LOW) &&
				(content->data.conf.voc_setting > VOC_THRESHOLD_SETTING_HIGH) &&
				(content->data.conf.voc_setting != VALUE_UNMODIFIED)) {

				proto_prepare_nack(PROTOCOL_NACK_CODE_WRITE_ERR, PROTOCOL_FUNCT_WRITE, obj_id, out_data, out_data_size);
				return;
			}

			if (content->data.conf.fc_setting != 0 && content->data.conf.fc_setting != VALUE_UNMODIFIED) {
				proto_prepare_nack(PROTOCOL_NACK_CODE_WRITE_ERR, PROTOCOL_FUNCT_WRITE, obj_id, out_data, out_data_size);
				return;
			}

			if ((content->data.conf.rotation_setting > 0) && (content->data.conf.rotation_setting != VALUE_UNMODIFIED)) {
				proto_prepare_nack(PROTOCOL_NACK_CODE_WRITE_ERR, PROTOCOL_FUNCT_WRITE, obj_id, out_data, out_data_size);
				return;
			}

			if (content->data.conf.rh_setting != VALUE_UNMODIFIED) {
				set_relative_humidity_set(content->data.conf.rh_setting);
			}
			if (content->data.conf.lux_setting != VALUE_UNMODIFIED) {
				set_lux_set(content->data.conf.lux_setting);
			}
			if (content->data.conf.voc_setting != VALUE_UNMODIFIED) {
				set_voc_set(content->data.conf.voc_setting);
			}
		    break;
		}

		case PROTOCOL_OBJID_ADV_CONF:
		{
			int16_t temperature_offset;
			int16_t humidity_offset;

		    temperature_offset = convert_big_endian_16(content->data.adv_conf.temperature_offset);
		    humidity_offset = convert_big_endian_16(content->data.adv_conf.humidity_offset);

			if ((((temperature_offset < OFFSET_BOUND_MIN) || (temperature_offset > OFFSET_BOUND_MAX)) && (temperature_offset != VALUE_UNMODIFIED_LONG)) ||
				(((humidity_offset < OFFSET_BOUND_MIN) || (humidity_offset > OFFSET_BOUND_MAX)) && (humidity_offset != VALUE_UNMODIFIED_LONG))) {

				proto_prepare_nack(PROTOCOL_NACK_CODE_WRITE_ERR, PROTOCOL_FUNCT_WRITE, obj_id, out_data, out_data_size);
				return;
			}

		    if (temperature_offset != VALUE_UNMODIFIED_LONG) {
		        set_temperature_offset(temperature_offset);
		    }
		    if (humidity_offset != VALUE_UNMODIFIED_LONG) {
		    	set_relative_humidity_offset(humidity_offset);
		    }

		    break;
		}

		case PROTOCOL_OBJID_WIFI_CONF:
		{
		    uint32_t port;
		    uint16_t period;

		    // Convert string to long integer for port
		    port = strtol((const char *)content->data.wifi_conf.port, NULL, 10);

		    // Convert period to big-endian format
		    period = htons(content->data.wifi_conf.period);

		    // Validate port and period values
		    if (((port < MIN_PORT_VALUE) || (port > MAX_PORT_VALUE)) && (port != VALUE_UNMODIFIED_LONG)) {
		    	proto_prepare_nack(PROTOCOL_NACK_CODE_WRITE_ERR, PROTOCOL_FUNCT_WRITE, obj_id, out_data, out_data_size);
		    }

		    // Configuration logic (currently commented out)
		    #if 0
		    if (memcmp(content->data.wifi_conf.ssid, WIFI_SSID_ANY, WIFI_SSID_LEN)) {
		        set_wifi_ssid(content->data.wifi_conf.ssid);
		    }

		    if (memcmp(content->data.wifi_conf.psk, WIFI_PSK_ANY, WIFI_PSK_LEN)) {
		        set_wifi_psk(content->data.wifi_conf.psk);
		    }
		    #endif

		    // Set WiFi server
		    if (memcmp(content->data.wifi_conf.server, SERVER_ANY, SERVER_SIZE)) {
		        set_server(content->data.wifi_conf.server);
		    }

		    // Set WiFi port
		    if (memcmp(content->data.wifi_conf.port, PORT_ANY, PORT_SIZE)) {
		        set_port(content->data.wifi_conf.port);
		    }

		    // Set WiFi period
		    if (period != VALUE_UNMODIFIED_LONG) {
		        set_wifi_period(period);
		    }

		    // Optionally log the event
		    // log_save_event(EVENT_WIFI_CONFIGURATION_SETTINGS, NULL);

		    break;
		}

		case PROTOCOL_OBJID_PROFILE:
		{
			break;
		}


		case PROTOCOL_OBJID_CLOCK:
		{
			break;
		}

		case PROTOCOL_OBJID_OPER:
		{

			if (content->data.oper.mode_setting > MODE_AUTOMATIC_CYCLE) {
				proto_prepare_nack(PROTOCOL_NACK_CODE_WRITE_ERR, PROTOCOL_FUNCT_WRITE, obj_id, out_data, out_data_size);
				return;
			}

			if (((content->data.oper.mode_setting != MODE_AUTOMATIC_CYCLE) && (content->data.oper.speed_setting > SPEED_HIGH)) ||
				((content->data.oper.mode_setting == MODE_AUTOMATIC_CYCLE) && (content->data.oper.speed_setting > SPEED_HIGH))) {

				proto_prepare_nack(PROTOCOL_NACK_CODE_WRITE_ERR, PROTOCOL_FUNCT_WRITE, obj_id , out_data, out_data_size);
				return;
			}

			set_mode_set(content->data.oper.mode_setting);
			set_speed_set(content->data.oper.speed_setting);
			break;
		}

		default:
			proto_prepare_nack(PROTOCOL_NACK_CODE_WRITE_ERR, PROTOCOL_FUNCT_WRITE, obj_id , out_data, out_data_size);
			return;
	}

	proto_prepare_ack(PROTOCOL_ACK_CODE_WRITE_OK, PROTOCOL_FUNCT_WRITE, obj_id , out_data, out_data_size);
}


static void proto_parse_execute_function_data(const void *buf, uint8_t *out_data, size_t *out_data_size) {
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
            proto_prepare_nack(PROTOCOL_NACK_CODE_EXEC_F_ERR, PROTOCOL_FUNCT_EXECUTE_FUNCTION, exec_funct_id, out_data, out_data_size);
            return;
    }

    proto_prepare_ack(PROTOCOL_ACK_CODE_EXEC_F_OK, PROTOCOL_FUNCT_EXECUTE_FUNCTION, exec_funct_id, out_data, out_data_size);
}

int proto_elaborate_data(uint8_t *in_data, size_t in_data_size, uint8_t *out_data, size_t *out_data_size) {
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

            if (in_data_size < idx + length) {
                return 0; // incomplete trame
            }

            if (in_data[idx + length - 1U] != PROTOCOL_TRAME_ETX) {
                return -1; // invalide trame
            }

            uint8_t crc = in_data[idx + length - 2U];
            if (crc != calculate_crc(&in_data[idx + PROTOCOL_TRAME_ADDR_POS], length - 3U)) {
                return -1; // invalide trame
            }

            if ((address == get_serial_number()) && (crc == calculate_crc(&in_data[idx + PROTOCOL_TRAME_ADDR_POS], length - 3U))) {

            	printf("CRC: %02x\n", crc);
                printf("Receiving data (length = %zu):\n", length);
                for (size_t i = idx; i < idx + length; ++i) {
                	printf("%02x ", in_data[i]);
                    if ((i - idx + 1) % 16 == 0) printf("\n");
                }
                printf("\n");

                uint8_t funct = in_data[idx + PROTOCOL_TRAME_FUNCT_POS];
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
                        proto_prepare_nack(PROTOCOL_NACK_CODE_GENERIC_ERR, funct, 0U , out_data, out_data_size);
                        break;
                }
                processed = length;
                break;
            }
        }
    }

    return processed;
}

