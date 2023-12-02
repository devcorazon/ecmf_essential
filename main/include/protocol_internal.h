/*
 * protocol.h
 *
 *  Created on: 4 nov. 2023
 *      Author: youcef.benakmoume
 */

#ifndef MAIN_INCLUDE_PROTOCOL_INTERNAL_H_
#define MAIN_INCLUDE_PROTOCOL_INTERNAL_H_

#define TCP_RX_BUFFER_SIZE 128
///
#define PROTOCOL_TRAME_WO_SE_FIX_LEN	(8u)
#define PROTOCOL_TRAME_FIX_LEN			(PROTOCOL_TRAME_WO_SE_FIX_LEN + 2u)

/// Value of field in trame.
#define PROTOCOL_TRAME_STX				(0x0a)
#define PROTOCOL_TRAME_ETX				(0x0d)

/// Position of field in trame.
#define PROTOCOL_TRAME_ADDR_POS			(1u)
#define PROTOCOL_TRAME_LENGHT_POS		(5u)
#define PROTOCOL_TRAME_FUNCT_POS		(7u)
#define PROTOCOL_TRAME_DATA_POS			(8u)

///
enum {
	PROTOCOL_FUNCT_ACK					= 0x2b,
	PROTOCOL_FUNCT_NACK					= 0x2d,
	PROTOCOL_FUNCT_IDENTIFICATION		= 0x28,
	PROTOCOL_FUNCT_QUERY				= 0x3f,
	PROTOCOL_FUNCT_ANSWER				= 0x21,
	PROTOCOL_FUNCT_VOLUNTARY			= 0x3b,
	PROTOCOL_FUNCT_WRITE				= 0x2f,
	PROTOCOL_FUNCT_EXECUTE_FUNCTION		= 0x26,
};

///
enum {
	PROTOCOL_ACK_CODE_WRITE_OK			= 0x31,
	PROTOCOL_ACK_CODE_EXEC_F_OK			= 0x34,
};

///
enum {
	PROTOCOL_NACK_CODE_GENERIC_ERR		= 0x40,
	PROTOCOL_NACK_CODE_WRITE_ERR		= 0x41,
	PROTOCOL_NACK_CODE_QUERY_ERR		= 0x42,
	PROTOCOL_NACK_CODE_STATS_ERR		= 0x43,
	PROTOCOL_NACK_CODE_EXEC_F_ERR		= 0x44,
};

///
enum {
	PROTOCOL_OBJID_INFO					= 0x0010,
	PROTOCOL_OBJID_CONF					= 0x0020,
	PROTOCOL_OBJID_ADV_CONF				= 0x0021,
	PROTOCOL_OBJID_WIFI_CONF			= 0x0022,
	PROTOCOL_OBJID_PROFILE				= 0x0030,
	PROTOCOL_OBJID_CLOCK				= 0x0040,
	PROTOCOL_OBJID_OPER					= 0x0050,
	PROTOCOL_OBJID_STATS				= 0x0060,
	PROTOCOL_OBJID_MASTER_STATE			= 0x0070,
	PROTOCOL_OBJID_STATE				= 0x0080,
};

enum {
	PROTOCOL_EXEC_F_REBOOT				= 0x0001,
	PROTOCOL_EXEC_F_CLOSE_SOCK			= 0x000a,
	PROTOCOL_EXEC_F_CLR_FILTER_WRN		= 0x001a,
	PROTOCOL_EXEC_F_RESET_DFT_FACT		= 0x0a01,
};


/// Ack.
struct protocol_ack_s {
	uint8_t ack_code;
	uint8_t fuct_code;
	uint16_t add_data;
} __attribute__((packed));

/// Nack.
struct protocol_nack_s {
	uint8_t nack_code;
	uint8_t fuct_code;
	uint16_t add_data;
} __attribute__((packed));

/// Identification.
struct protocol_identification_s {
	uint32_t serial_number;
	uint16_t device_code;
} __attribute__((packed));

/// Info.
struct protocol_info_s {
	uint16_t firmware_version;
	uint8_t serial_numebr[9];
	uint8_t bt_address[BT_ADDRESS_LEN];					// compatibilità ECMF2 (valorizzare)
	uint8_t bt_connection_number;						// compatibilità ECMF2 (sempre 0)
	uint8_t bt_connection_state;						// compatibilità ECMF2 (sempre 0)
	uint8_t wifi_address[WIFI_ADDRESS_LEN];
	uint8_t wifi_connection_state;
} __attribute__((packed));

/// Configuration.
struct protocol_conf_s {
	uint8_t role;										// compatibilità ECMF2 (sempre 1)
	uint8_t rh_setting;
	uint8_t lux_setting;
	uint8_t voc_setting;
	uint8_t fc_setting;
	uint8_t rotation_setting;
	uint8_t server_address[BT_ADDRESS_LEN];
} __attribute__((packed));

/// Advanced configuration.
struct protocol_adv_conf_s {
	int16_t temperature_offset;
	int16_t humidity_offset;
} __attribute__((packed));

/// Wifi configuration.
struct protocol_wifi_conf_s {
	uint8_t ssid[SSID_SIZE];
	uint8_t pwd[PASSWORD_SIZE];
	uint8_t server[SERVER_SIZE];
	uint8_t port[PORT_SIZE];
	uint16_t period;
} __attribute__((packed));

///// Profile.
//struct protocol_prof_s {									// compatibilità ECMF2 (sempre tutto a 0)
//	uint8_t profile[DAYS_PER_WEEK][HOURS_PER_DAY];
//} __attribute__((packed));

/// Clock.
struct protocol_clock_s {
	uint8_t year;
	uint8_t month;
	uint8_t day;
	uint8_t dow;
	uint8_t hour;
	uint8_t minute;
	uint8_t second;
	uint8_t daylight_savings_time;
} __attribute__((packed));

/// Operation mode.
struct protocol_oper_s {
	uint8_t mode_setting;
	uint8_t speed_setting;
} __attribute__((packed));

/// Statistics data.
struct protocol_stats_data_s {									// compatibilità ECMF2 (sempre 0)
	uint8_t year;
	uint8_t month;
	uint8_t day;

	uint16_t none_daily_hour;
	uint16_t night_daily_hour;
	uint16_t low_daily_hour;
	uint16_t medium_daily_hour;
	uint16_t high_daily_hour;
	uint16_t boost_daily_hour;

	uint16_t none_tot_hour;
	uint16_t night_tot_hour;
	uint16_t low_tot_hour;
	uint16_t medium_tot_hour;
	uint16_t high_tot_hour;
	uint16_t boost_tot_hour;

//	int16_t temperature_quarter[QUARTERS_HOUR_PER_DAY];
//	uint8_t relative_humidity_quarter[QUARTERS_HOUR_PER_DAY];
//	uint16_t voc_quarter[QUARTERS_HOUR_PER_DAY];
} __attribute__((packed));

/// Master state.
struct protocol_master_state_s {								// compatibilità ECMF2 (sempre 0)
	uint8_t master_mode_state;
	uint8_t master_speed_state;
	uint8_t master_direction_state;
	uint16_t automatic_cycle_duration;
} __attribute__((packed));

/// State.
struct protocol_state_s {
	uint8_t mode_state;
	uint8_t speed_state;
	uint8_t direction_state;
	int16_t ambient_temperature;
	uint16_t relative_humidity;
	uint16_t voc;
} __attribute__((packed));

union protocol_data_u {
    struct protocol_info_s info;
    struct protocol_conf_s conf;
    struct protocol_adv_conf_s adv_conf;
    struct protocol_wifi_conf_s wifi_conf;
//    struct protocol_prof_s prof;
    struct protocol_clock_s clock;
    struct protocol_oper_s oper;
    struct protocol_stats_data_s stats;
    struct protocol_master_state_s master_state;
    struct protocol_state_s state;
} __attribute__((packed));

struct protocol_content_s {
	uint16_t obj_id;
	uint16_t index;
	union protocol_data_u data;
} __attribute__((packed));

#define SIZEOF_CONTENT(x)			(sizeof(struct protocol_content_s) - sizeof(union protocol_data_u) + sizeof((x)))

typedef struct {
    uint8_t *data;
    size_t len;
    size_t size;
} simple_buf_t;

struct protocol_trame {
    uint8_t *data;
    uint16_t len;
};

// Inline function to convert a 16-bit value to big-endian format
static inline uint16_t convert_big_endian_16(uint16_t data) {
    return (data >> 8) | (data << 8);
}

// Inline function to convert a 32-bit value to big-endian format
static inline uint32_t convert_big_endian_32(uint32_t data) {
    return ((data >> 24) & 0xff) |
           ((data << 8) & 0xff0000) |
           ((data >> 8) & 0xff00) |
           ((data << 24) & 0xff000000);
}

// Inline function to store a 16-bit value in big-endian format in a byte array
static inline void custom_put_be16(uint16_t val, uint8_t dst[2]) {
    uint16_t big_endian_val = convert_big_endian_16(val);
    memcpy(dst, &big_endian_val, sizeof(big_endian_val));
}

// Inline function to store a 32-bit value in big-endian format in a byte array
static inline void custom_put_be32(uint32_t val, uint8_t dst[4]) {
    uint32_t big_endian_val = convert_big_endian_32(val);
    memcpy(dst, &big_endian_val, sizeof(big_endian_val));
}

static inline void sys_memcpy_swap(void *dst, const void *src, size_t length){
	uint8_t *pdst = (uint8_t *)dst;
	const uint8_t *psrc = (const uint8_t *)src;

	psrc += length - 1;

	for (; length > 0; length--) {
		*pdst++ = *psrc--;
	}
}

static inline void simple_buf_add_mem(simple_buf_t *buf, const uint8_t *mem, size_t len) {
    if (buf->len + len > buf->size) {
        // handle buffer overflow
    }
    memcpy(buf->data + buf->len, mem, len);
    buf->len += len;
}


#endif /* MAIN_INCLUDE_PROTOCOL_INTERNAL_H_ */
