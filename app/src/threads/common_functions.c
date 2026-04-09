/** @file common_functions.c
 *
 * @brief File containing globally used functions
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2020 Irnas. All rights reserved.
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/reboot.h>

#include "common_functions.h"

#include "definitions.h"
#include "global_time.h"
#include "status.h"
#include "thread_com.h"
#include "thread_operation.h"

LOG_MODULE_REGISTER(common_functions);

/* Global variables */
struct k_mutex spi_mutex;

static uint64_t prv_errors_last_check = 0; // Time of last LED state change

int thread_init_mutex(void)
{
	int err = k_mutex_init(&spi_mutex);

	return err;
}

void hibernation_mode(void)
{
	uint8_t disable_data[3];
	disable_data[0] = CMD_SET_OPERATION_MODE_COM_TH;
	disable_data[1] = 1;
	disable_data[2] = THREAD_DISABLED;
	thread_put_message(MB_MSG_DEV, MB_MSG_LORA, MB_MSG_EXECUTE, PORT_COMMANDS, disable_data, 3,
			   MESSAGE_DATA_MAX_LEN);

	disable_data[0] = CMD_DISABLE_FLASH_TH;
	disable_data[1] = 0;
	thread_put_message(MB_MSG_DEV, MB_MSG_FLASH, MB_MSG_EXECUTE, PORT_COMMANDS, disable_data, 2,
			   MESSAGE_DATA_MAX_LEN);

	disable_data[0] = CMD_DISABLE_BT_TH;
	disable_data[1] = 0;
	thread_put_message(MB_MSG_DEV, MB_MSG_BT, MB_MSG_EXECUTE, PORT_COMMANDS, disable_data, 2,
			   MESSAGE_DATA_MAX_LEN);

	disable_data[0] = CMD_SET_OPERATION_MODE_MAIN_TH;
	disable_data[1] = 1;
	disable_data[2] = THREAD_DISABLED;
	thread_put_message(MB_MSG_DEV, MB_MSG_DEV, MB_MSG_EXECUTE, PORT_COMMANDS, disable_data, 3,
			   MESSAGE_DATA_MAX_LEN);
}

void low_power_mode(void)
{
	uint8_t disable_data[3];

	// Set communication thread to low power
	disable_data[0] = CMD_SET_OPERATION_MODE_COM_TH;
	disable_data[1] = 1;
	disable_data[2] = THREAD_LOW_POWER;
	thread_put_message(MB_MSG_DEV, MB_MSG_LORA, MB_MSG_EXECUTE, PORT_COMMANDS, disable_data, 3,
			   MESSAGE_DATA_MAX_LEN);

	// Set main thread to low power
	disable_data[0] = CMD_SET_OPERATION_MODE_MAIN_TH;
	disable_data[1] = 1;
	disable_data[2] = THREAD_LOW_POWER;
	thread_put_message(MB_MSG_DEV, MB_MSG_DEV, MB_MSG_EXECUTE, PORT_COMMANDS, disable_data, 3,
			   MESSAGE_DATA_MAX_LEN);
}

void normal_mode(void)
{
	LOG_WRN("System will reboot!");
	k_sleep(K_SECONDS(1));
	sys_reboot(0);

	/* LUKATODO: enable this code after GPS i2C error is resolved */
	// uint8_t enable_data[3];

	// // Set communication thread to normal power
	// enable_data[0] = CMD_SET_OPERATION_MODE_COM_TH;
	// enable_data[1] = 1;
	// enable_data[2] = THREAD_NORMAL;
	// thread_put_message(MB_MSG_DEV, MB_MSG_LORA, MB_MSG_EXECUTE, PORT_COMMANDS, enable_data,
	// 3, 		   MESSAGE_DATA_MAX_LEN);

	// // Set main thread to normal power
	// enable_data[0] = CMD_SET_OPERATION_MODE_MAIN_TH;
	// enable_data[1] = 1;
	// enable_data[2] = THREAD_NORMAL;
	// thread_put_message(MB_MSG_DEV, MB_MSG_DEV, MB_MSG_EXECUTE, PORT_COMMANDS, enable_data, 3,
	// 		   MESSAGE_DATA_MAX_LEN);
}

void read_factory_name(uint8_t *name)
{
	uint32_t name_1 = NRF_UICR->CUSTOMER[1];
	uint32_t name_2 = NRF_UICR->CUSTOMER[2];

	uint8_t tmp[4];

	uint32_t_to_bytes(tmp, name_1);
	name[0] = tmp[3];
	name[1] = tmp[2];
	name[2] = tmp[1];
	name[3] = tmp[0];

	uint32_t_to_bytes(tmp, name_2);
	name[4] = tmp[3];
	name[5] = tmp[2];
	name[6] = tmp[1];
	name[7] = tmp[0];

	return;
}

void handle_errors(void)
{
	// Check if check is should be done
	if (Main_settings.check_error_interval->def_val > 0) {
		if ((uint32_t)((k_uptime_get() - prv_errors_last_check) / 1000) >
		    Main_settings.check_error_interval->def_val) {
			LOG_INF("Check for system errors.");
			prv_errors_last_check = k_uptime_get();
			if (status_check_critical_errors()) {
				LOG_ERR("Rebooting system in 5s due to critical operational "
					"errors!");
				k_sleep(K_SECONDS(5));
				sys_reboot(0);
			}
		}
	}
}
