/** @file thread_operation.h
 *
 * @brief Sets thread operation status.
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2020 Irnas. All rights reserved.
 */

#include "thread_operation.h"

LOG_MODULE_REGISTER(thread_operation); // init logging

uint32_t thread_sleep = THREAD_LP_SLEEP;

thread_enable thread_status = {
	THREAD_NORMAL, THREAD_NORMAL, THREAD_NORMAL, THREAD_NORMAL,
	THREAD_NORMAL, THREAD_NORMAL, THREAD_NORMAL, 0}; // By defoult enable all threads

/*!
 * @brief Set functionality of communication in all threads (lr, gps and bt)
 *
 * @param state - enable/disable
 *
 * @retval
 */
void set_com_thread_operation(thread_operation_status state)
{
	thread_status.com_thread_operation = state;
	if (state == THREAD_DISABLED) {
		thread_status.com_thread_inactive_start = k_uptime_get();
	}
}

/*!
 * @brief Enable or disable thread operation
 *
 * @param th_name - thread identifier
 * @param state - enable/disable
 *
 * @retval
 */
void set_thread_operation(uint8_t th_name, thread_operation_status state)
{
	switch (th_name) {
	case MAIN_THREAD: {
		thread_status.main_thread_enabled = state;
	}
	case FLASH_THREAD: {
		thread_status.flash_thread_enabled = state;
	}
	case BLE_WRITE_THREAD: {
		thread_status.ble_write_thread_enabled = state;
	}
	case BLE_READ_THREAD: {
		thread_status.ble_read_thread_enabled = state;
	}
	case SENSORS_THREAD: {
		thread_status.sensors_thread_enabled = state;
	}
	case LORA_GPS_THREAD: {
		thread_status.lora_gps_thread_enabled = state;
	}
	}
}

/*!
 * @brief Set functionality of com, thread
 * @retval
 */
thread_operation_status get_thread_operation(uint8_t th_name)
{
	// Check if timeout of temporal disable of communication services by lr thread
	if (thread_status.com_thread_operation == THREAD_DISABLED &&
	    (uint32_t)((k_uptime_get() - thread_status.com_thread_inactive_start) / 1000) >
		    COM_THREAD_INACTIVE_TIMEOUT) {
		thread_status.com_thread_operation = THREAD_NORMAL;
		thread_sleep = THREAD_LP_SLEEP;
	}

	switch (th_name) {
	case MAIN_THREAD: {
		return MIN(thread_status.com_thread_operation, thread_status.main_thread_enabled);
	}
	case FLASH_THREAD: {
		return thread_status.flash_thread_enabled;
	}
	case BLE_WRITE_THREAD: {
		return thread_status.ble_write_thread_enabled;
	}
	case BLE_READ_THREAD: {
		return thread_status.ble_read_thread_enabled;
	}
	case SENSORS_THREAD: {
		return thread_status.sensors_thread_enabled;
	}
	case LORA_GPS_THREAD: {
		return MIN(thread_status.com_thread_operation,
			   thread_status.lora_gps_thread_enabled);
	}
	}
	return THREAD_DISABLED;
}
