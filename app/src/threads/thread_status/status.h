#ifndef STATUS_H_
#define STATUS_H_

#include <zephyr/kernel.h>

// SYSTEM ERRORS
enum system_bit_errors {
	STATUS_LR_ERROR = (1 << 0),
	STATUS_BLE_ERROR = (1 << 1),
	STATUS_UBLOX_ERROR = (1 << 2),
	STATUS_ACC_ERROR = (1 << 3),
	STATUS_BAT_ERROR = (1 << 4),
	STATUS_UBLOX_FIX = (1 << 5),
	STATUS_FLASH_ERROR = (1 << 6),
	STATUS_UBLOX_BUSY = (1 << 7),
};

enum system_operation_flags {
	OPERATION_UNREAD_MSG = (1 << 0),
	OPERATION_SECURITY = (1 << 1),
	OPERATION_LR_JOIN = (1 << 2),
};

enum supported_features_flags {
	FEATURES_SATELLITE = (1 << 0),
	FEATURES_FENCE = (1 << 2)
};

enum reset_reasons_bit {
	RESETREAS_A_RESETPIN = (1 << 0),
	RESETREAS_B_DOG = (1 << 1),
	RESETREAS_C_SREQ = (1 << 2),
	RESETREAS_D_LOCKUP = (1 << 3),
};

typedef struct {
	int lr;
	int ble;
	int ublox;
	int ublox_busy;
	int acc;
	int bat;
	int ublox_fix;
	int flash;
} status_system_errors;

typedef struct {
	bool msg;
	bool security;
	int lr_join;
	int lr_sat;
} status_system_operation;

typedef struct {
	bool satellite_com;
	bool fence;
	uint8_t satellite_retries;
} status_supported_features;

typedef struct {
	uint8_t reset_cause;
	uint8_t system_functions_errors;
	uint8_t battery;
	uint8_t operation;
	uint8_t temperature;
	uint8_t uptime;
	uint8_t acc_x;
	uint8_t acc_y;
	uint8_t acc_z;
	uint8_t hw_ver;
	uint8_t fw_ver;
	uint8_t type;
	uint8_t charging;
	uint8_t features;
} __attribute__((packed)) statusData_t;

typedef union statusPacket_t {
	statusData_t data;
	uint8_t bytes[sizeof(statusData_t)];
} statusPacket_t;

extern status_system_errors sys_err;
extern status_system_operation sys_operation;
extern status_supported_features sys_features;

/**
 * @brief Initialize status message to def values.
 *
 */
void status_init();

/**
 * @brief Update status message based on the incoming values.
 *
 * @return int
 */
int status_update(void);

/**
 * @brief Get the status message if supplied max length allows.
 *
 * @param message message array
 * @param max_len max allowed length
 * @return int
 */
int status_get_message(uint8_t *message, uint8_t max_len);

/**
 * @brief Check if there are any critical operational errors detected.
 *
 * @return true
 * @return false
 */
bool status_check_critical_errors(void);

#endif
