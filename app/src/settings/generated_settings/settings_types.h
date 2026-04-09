#ifndef SETTINGS_TYPES_H_
#define SETTINGS_TYPES_H_

#include <stdint.h>
#include <zephyr/kernel.h>

/* ENUMS */

// Conversion types
typedef enum conversion_t {
	UINT8_T = 1,
	UINT16_T = 2,
	UINT32_T = 3,
	INT8_T = 4,
	INT16_T = 5,
	INT32_T = 6,
	FLOAT = 7,
	BYTE_ARRAY = 8,
	BOOL = 9
} conversion_t;

// Hardware types define - add others
typedef enum hw_type {
	rhinoedge_nrf52840 = 1,
	rangeredge_nrf52840 = 5,
	rhinopuck_nrf52840 = 6,
	rhinopuck35_nrf52840 = 7,
	collaredge_nrf52840 = 8,
	freeedge_nrf52840 = 9,

	/*
	Unsupported (outdated) hardware types:
	These enumerations are not in use but must be kept for reference. Any NEW hardware types
	must NOT use these numbers.
	*/
	elephantedge_nrf52840 = 2,
	wisentedge_nrf52840 = 3,
	cattracker_nrf52840 = 4,
} hw_type;

// Firmware types define - add others
typedef enum fw_type {
	/* HW default */
	default_tracker = 0,
	rhinoedge_tracker = 1,
	rangeredge_tracker = 5,
	rhinopuck_tracker = 6,
	collaredge_tracker = 8,
	freeedge_tracker = 9,
	/* FW selectable */
	elephantedge_tracker = 2,
	wisentedge_tracker = 3,
	cattracker_tracker = 4,
	scanneredge_tracker = 7,
	fenceedge_tracker = 10,
	horseedge_tracker = 11,
	collaredgepico_tracker = 12,
	collaredgenano_tracker = 13,
	baboonedge_tracker = 14,
	pangolinedge_tracker = 15
} fw_type;

/* END ENUMS */

/* SETTINGS STRUCTS - settings containing governing settings for message communication and sending*/

// Firmware version
typedef struct {
	fw_type type;
	uint8_t major;
	uint8_t minor;
} fw_version;

/* END SETTINGS STRUCTS */

/* MESSAGES */
// General message structure
typedef struct {
	uint8_t port;
	uint8_t id;
	uint8_t length;
	conversion_t conversion;
} cmd_message;

/* END MESSAGES STRUCTS */

/* SETTINGS, VALUES STRUCTS */
// Structures for containing settings of values of certain type
typedef struct {
	uint8_t id;
	uint8_t def_val;
	uint8_t min;
	uint8_t max;
	uint8_t len;
	conversion_t conversion;

} value_uint8;

typedef struct {
	uint8_t id;
	uint16_t def_val;
	uint16_t min;
	uint16_t max;
	uint8_t len;
	conversion_t conversion;

} value_uint16;

typedef struct {
	uint8_t id;
	uint32_t def_val;
	uint32_t min;
	uint32_t max;
	uint8_t len;
	conversion_t conversion;

} value_uint32;

typedef struct {
	uint8_t id;
	int8_t def_val;
	int8_t min;
	int8_t max;
	uint8_t len;
	conversion_t conversion;

} value_int8;

typedef struct {
	uint8_t id;
	int16_t def_val;
	int16_t min;
	int16_t max;
	uint8_t len;
	conversion_t conversion;

} value_int16;

typedef struct {
	uint8_t id;
	int32_t def_val;
	int32_t min;
	int32_t max;
	uint8_t len;
	conversion_t conversion;

} value_int32;

typedef struct {
	uint8_t id;
	int16_t def_val[2];
	int16_t min[2];
	int16_t max[2];
	uint8_t len;
	conversion_t conversion;

} value_float;

typedef struct {
	uint8_t id;
	uint8_t *def_val;
	uint8_t *min;
	uint8_t *max;
	uint8_t len;
	conversion_t conversion;

} value_byte_array;

typedef struct {
	uint8_t id;
	bool def_val;
	bool min;
	bool max;
	uint8_t len;
	conversion_t conversion;

} value_bool;

#endif
