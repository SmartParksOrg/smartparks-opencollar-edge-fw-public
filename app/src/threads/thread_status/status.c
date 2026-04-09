#include <stdio.h>
#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>
#include <version_info.h>

#include "status.h"

#include "definitions.h"
#include "generated_settings.h"

LOG_MODULE_REGISTER(status); // init logging

status_system_errors sys_err;
status_system_operation sys_operation;
status_supported_features sys_features;

statusPacket_t status_general;

uint8_t system_functions_errors = 0;

/** PRIVATE FUNCTIONS **/

/**
 * @brief Return float from array float representation
 *
 * @param x1
 * @param x2
 * @return float
 */
static float status_float_from_array(int16_t x1, int16_t x2)
{
	float f = 1;
	if (x1 < 0 || x2 < 0) {
		f = -1;
	}

	f *= (float)abs(x1) + ((float)abs(x2)) / 10000;

	return f;
}

/**
 * @brief Convert float in array representation to scaled uint8_t value
 *
 * @param value
 * @return uint8_t
 */
static uint8_t status_float_to_uint8(value_float *value)
{
	// convert limits and values to floats
	float _min = status_float_from_array(value->min[0], value->min[1]);
	float _max = status_float_from_array(value->max[0], value->max[1]);
	float _val = status_float_from_array(value->def_val[0], value->def_val[1]);

	_val = ((_val - _min) * 255) / (_max - _min);

	return (uint8_t)_val;
}

/**
 * @brief Update tracker HW and FW type from board definition and setting.
 *
 */
static void status_update_type()
{
	// Add FW and HW type
	hw_type hw = 0;
	fw_type fw = 0;
#ifdef CONFIG_BOARD_RHINOEDGE_NRF52840
	hw = rhinoedge_nrf52840;
	if (Main_settings.tracker_type->def_val == default_tracker) {
		fw = rhinoedge_tracker;
	} else {
		fw = Main_settings.tracker_type->def_val;
	}
#elif CONFIG_BOARD_RANGEREDGE_NRF52840 || CONFIG_BOARD_RANGEREDGE_AIRQ_NRF52840
	hw = rangeredge_nrf52840;
	if (Main_settings.tracker_type->def_val == default_tracker) {
		fw = rangeredge_tracker;
	} else {
		fw = Main_settings.tracker_type->def_val;
	}
#elif CONFIG_BOARD_RHINOPUCK_NRF52840
	hw = rhinopuck_nrf52840;
	if (Main_settings.tracker_type->def_val == default_tracker) {
		fw = rhinopuck_tracker;
	} else {
		fw = Main_settings.tracker_type->def_val;
	}
#elif CONFIG_BOARD_RHINOPUCK35_NRF52840
	hw = rhinopuck35_nrf52840;
	if (Main_settings.tracker_type->def_val == default_tracker) {
		fw = rhinopuck_tracker;
	} else {
		fw = Main_settings.tracker_type->def_val;
	}
#elif CONFIG_BOARD_COLLAREDGE_NRF52840
	hw = collaredge_nrf52840;
	if (Main_settings.tracker_type->def_val == default_tracker) {
		fw = collaredge_tracker;
	} else {
		fw = Main_settings.tracker_type->def_val;
	}
#elif CONFIG_BOARD_FREEEDGE_NRF52840
	hw = freeedge_nrf52840;
	if (Main_settings.tracker_type->def_val == default_tracker) {
		fw = freeedge_tracker;
	} else {
		fw = Main_settings.tracker_type->def_val;
	}
#else
	LOG_ERR("Invalid HW type!");
#endif

	// Add FW type
	status_general.data.type = (fw << 4) | hw;
	LOG_DBG("HW type: %d FW type: %d status type: %d", hw, fw, status_general.data.type);
}

/**
 * @brief Init FW and HW versions
 *
 */
static void status_init_version_type()
{
	// Get HW version from build type
	int ver_major = 0, ver_minor = 0, ver_revision = 0;
	sscanf(CONFIG_BOARD_REVISION, "%d.%d.%d", &ver_major, &ver_minor, &ver_revision);
	status_general.data.hw_ver = ((uint8_t)ver_major << 4) | (uint8_t)ver_minor;
	LOG_INF("HW maj: %d HW min: %d, status: %d", ver_major, ver_minor,
		status_general.data.hw_ver);

	/* fetch fw version info */
	const struct version_info *vi = version_info_get();

	status_general.data.fw_ver = (vi->major << 4) | vi->minor;
	LOG_INF("FW maj: %d FW min: %d, status: %d", vi->major, vi->minor,
		status_general.data.fw_ver);

	/* Update HW and FW version */
	status_update_type();
}

/**
 * @brief Set all error values to 0
 *
 */
static void status_init_system_errors()
{
	sys_err.lr = 0;
	sys_err.ble = 0;
	sys_err.ublox = 0;
	sys_err.acc = 0;
	sys_err.ublox_busy = 0;
	sys_err.ublox_fix = 0;
	sys_err.bat = 0;
	sys_err.flash = 0;
}

/**
 * @brief Set all operation values to 0
 *
 */
static void status_init_system_operation()
{
	sys_operation.msg = 0;
	sys_operation.security = 0;
	sys_operation.lr_join = 0;
	sys_operation.lr_sat = 0;
}

/**
 * @brief Initialize supported features to 0;
 *
 */
static void status_init_system_features()
{
	sys_features.satellite_com = false;
	sys_features.fence = false;
	sys_features.satellite_retries = 0;
}

/**
 * @brief For all system errors, convert representing bits to 1
 *
 * @return uint8_t - error byte
 */
static uint8_t status_update_system_errors()
{
	// Reset status
	uint8_t err = 0x00;
	// Add errors
	if (sys_err.lr < 0) {
		err = err | STATUS_LR_ERROR;
	}
	if (sys_err.ble < 0) {
		err = err | STATUS_BLE_ERROR;
	}
	if (sys_err.ublox < 0) {
		err = err | STATUS_UBLOX_ERROR;
	}
	if (sys_err.acc < 0) {
		err = err | STATUS_ACC_ERROR;
	}
	if (sys_err.ublox_fix < 0) {
		err = err | STATUS_UBLOX_FIX;
	}
	if (sys_err.bat < 0) {
		err = err | STATUS_BAT_ERROR;
	}
	if (sys_err.flash < 0) {
		err = err | STATUS_FLASH_ERROR;
	}
	if (sys_err.ublox_busy != 0) {
		err = err | STATUS_UBLOX_BUSY;
	}

	// err = 0x1F; Test all errors
	// LOG_INF("ERRORS lr: %d ble: %d ublox: %d acc: %d time: %d bat: %d");
	return err;
}

/**
 * @brief For all system operations, convert representing bits to 1. Add nr. of LR satellites.
 *
 * @return uint8_t system operations byte.
 */
static uint8_t status_update_system_operation()
{
	// Reset status
	uint8_t operation = 0x00;
	// Unread msg
	if (Main_values.n_mes->def_val > 0) {
		operation = operation | OPERATION_UNREAD_MSG;
	}

	// Security
	sys_operation.security = false;
	for (uint8_t i = 0; i < Main_settings.device_pin->len; i++) {
		if (Main_settings.device_pin->def_val[i] != 0) {
			sys_operation.security = true;
			break;
		}
	}
	if (sys_operation.security) {
		operation = operation | OPERATION_SECURITY;
	}

	// LR join
	if (sys_err.lr < 0) {
		sys_operation.lr_join = -EIO;
	}
	if (sys_operation.lr_join < 0) {
		operation = operation | OPERATION_LR_JOIN;
	}

	// LR satellites
	LOG_INF("N sat status: %d", Main_values.lr_satellites->def_val);
	uint8_t n_sat = Main_values.lr_satellites->def_val;
	if (n_sat > 15) {
		n_sat = 15;
	}
	operation = operation | (n_sat << 4);
	LOG_INF("Send bit: %x", operation);

	return operation;
}

/**
 * @brief Initialize supported features to 0;
 *
 */
static uint8_t status_update_supported_features(void)
{
	// Reset status
	uint8_t features = 0x00;

	// Satellite support
	if (sys_features.satellite_com) {
		features = features | FEATURES_SATELLITE;
	}

	// Fence support
	if (sys_features.fence) {
		features = features | FEATURES_FENCE;
	}

	sys_features.satellite_retries = Main_values.satellite_resend_try->def_val;
	if (sys_features.satellite_retries > 15) {
		sys_features.satellite_retries = 15;
	}
	features = features | (sys_features.satellite_retries << 4);

	return features;
}

/**
 * @brief Read reset reason.
 *
 * @return uint8_t
 */
static uint8_t status_read_reset_reason(void)
{
	Main_values.reset_reason->def_val = NRF_POWER->RESETREAS;
	NRF_POWER->RESETREAS = 0xFFFFFFFF; // clear reg
	LOG_INF("RESET REASON: 0x%08X", Main_values.reset_reason->def_val);

	// Return only first 4 bits
	uint8_t reset_reason = Main_values.reset_reason->def_val & 0x0000000F;

	return reset_reason;
}

/** END PRIVATE FUNCTIONS **/

/** PUBLIC FUNCTIONS **/

void status_init()
{
	status_init_system_errors();
	status_init_version_type();
	status_init_system_operation();
	status_init_system_features();

	status_general.data.reset_cause = status_read_reset_reason();
}

int status_update(void)
{
	// General status
	status_general.data.system_functions_errors = status_update_system_errors();

	// Update set FW type
	status_update_type();

	// Check battery
	if (Main_values.batt_mV->def_val < 2500) {
		status_general.data.battery = 0;
	} else {
		status_general.data.battery = (uint8_t)((Main_values.batt_mV->def_val - 2500) / 10);
	}
	// Check charging
	if (Main_values.charge_mV->def_val < CHARGING_THRESHOLD_V) {
		status_general.data.charging = 0;
	} else {
		status_general.data.charging =
			(uint8_t)((Main_values.charge_mV->def_val - CHARGING_THRESHOLD_V) / 100);
	}
	status_general.data.operation = status_update_system_operation();
	status_general.data.features = status_update_supported_features();
	status_general.data.temperature = status_float_to_uint8(Main_values.mcu_temp);
	status_general.data.uptime = (uint8_t)(k_uptime_get() / (1000 * 3600 * 24));

	status_general.data.acc_x = status_float_to_uint8(Main_values.lis2_acc_x);
	status_general.data.acc_y = status_float_to_uint8(Main_values.lis2_acc_y);
	status_general.data.acc_z = status_float_to_uint8(Main_values.lis2_acc_z);

	/*
	LOG_INF("Reset: %d Err: %d Bat: %d Oper: %d Temp: %d Uptime: %d X: %d Y: %d Z: %d",
	status_general.data.reset_cause, status_general.data.system_functions_errors,
	status_general.data.battery, status_general.data.operation, status_general.data.temperature,
	status_general.data.uptime, status_general.data.acc_x, status_general.data.acc_y,
	status_general.data.acc_z); LOG_INF("LR status: %d LR join: %d\n\n", sys_err.lr,
	sys_operation.lr_join);
	*/

	return 0;
}

int status_get_message(uint8_t *message, uint8_t max_len)
{
	// Update status
	status_update();

	// Main status
	int len = sizeof(status_general.bytes);
	if (len > max_len) {
		len = max_len;
	}
	memcpy(message, status_general.bytes, sizeof(status_general.bytes));

	return len;
}

bool status_check_critical_errors(void)
{
	if (sys_err.lr < 0) {
		LOG_ERR("Critical LR error %d", sys_err.lr);
		return true;
	}
	if (sys_err.ble < 0) {
		LOG_ERR("Critical BLE error %d", sys_err.ble);
		return true;
	}

	return false;
}

/** END PUBLIC FUNCTIONS **/
