/** @file thread_operation.h
 *
 * @brief Sets thread operation status.
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2020 Irnas. All rights reserved.
 */

#ifndef THREAD_OPERATION_H
#define THREAD_OPERATION_H

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "definitions.h"

#define COM_THREAD_INACTIVE_TIMEOUT 600 // com thread cannot be inactive for more then the timeout

/* Global variables */
extern uint32_t thread_sleep;

typedef enum {
	MAIN_THREAD = 0,
	FLASH_THREAD = 1,
	BLE_WRITE_THREAD = 2,
	BLE_READ_THREAD = 3,
	SENSORS_THREAD = 4,
	LORA_GPS_THREAD = 5,
} thread_name;

typedef enum {
	THREAD_DISABLED = 0,
	THREAD_LOW_POWER = 1,
	THREAD_NORMAL = 2,
} thread_operation_status;

typedef struct {
	thread_operation_status main_thread_enabled;      // Main thread operation
	thread_operation_status flash_thread_enabled;     // Flash thread operation
	thread_operation_status ble_write_thread_enabled; // Ble write thread operation
	thread_operation_status ble_read_thread_enabled;  // Ble read thread operation
	thread_operation_status sensors_thread_enabled;   // Sensors thread operation
	thread_operation_status lora_gps_thread_enabled;  // Lora and gps thread operation

	thread_operation_status
		com_thread_operation;       // All communication operation (LR, GPS and BT)
	uint64_t com_thread_inactive_start; // Start of temporal suspend
} thread_enable;

void set_com_thread_operation(thread_operation_status state);
void set_thread_operation(uint8_t th_name, thread_operation_status state);
thread_operation_status get_thread_operation(uint8_t th_name);

#endif // THREAD_OPERATION_H
