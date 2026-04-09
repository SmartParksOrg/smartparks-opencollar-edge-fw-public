#include <zephyr/device.h>
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/kernel.h>
#include <zephyr/types.h>
#include <soc.h>

#include <zephyr/settings/settings.h>

#include <stdio.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/sys/reboot.h>

#include <version_info.h>

#include "definitions.h"

#ifdef CONFIG_OUTDOOR_DETECTION
#include "outdoor_detection.h"
#endif /* CONFIG_OUTDOOR_DETECTION */

#include "settings_interface.h"

#include "common_functions.h"

#include "battery_interface.h"
#include "charging_interface.h"

#include "bt_adv.h"
#include "bt_interface.h"
#include "bt_nus.h"

#include "thread_watchdog.h"

#include "global_time.h"
#include "led.h"
#include "status.h"

#include "sensors.h"
#include "temperature_sensor.h"
#ifdef CONFIG_ENABLE_MIC
#include "mic_interface.h"
#endif

#include "communication.h"
#include "gps.h"

#include "thread_com.h"
#include "thread_operation.h"

#include "flash_ext_partitions.h"
#include "flash_interface.h"

#include "reed.h"

#ifdef CONFIG_PROVISIONING_MODE
#include "operation_test.h"
#endif // CONFIG_PROVISIONING_MODE

#ifdef CONFIG_SATELLITE
#include "satellite.h"
#endif // CONFIG_SATELLITE

#ifdef CONFIG_FENCE_PORT
#include <external_switch.h>
#include <fence.h>
#include <fence_port_common.h>
#endif // CONFIG_FENCE_PORT

#ifdef CONFIG_SDFS_UART_MODULE
#include <sdfs_uart_handler.h>
#endif /* CONFIG_SDFS_UART_MODULE */

/*ADD LP*/
#include "uart_pm.h"
#include <zephyr/pm/pm.h>
/*END LP*/

#include <zephyr/logging/log.h>

#ifdef CONFIG_SATELLITE
#define MAX_MAIN_THREAD_SEM 4
#else
#define MAX_MAIN_THREAD_SEM 3
#endif // CONFIG_SATELLITE
#define INIT_TIMEOUT_SEC 60

LOG_MODULE_REGISTER(MAIN);

/* THREADS */
static void main_com_thread(void);
static void flash_thread(void);
static void ble_write_thread(void);
static void ble_read_thread(void);
static void sensors_thread(void);
static void lora_gps_thread(void);
#ifdef CONFIG_SATELLITE
static void satellite_thread(void);
#endif // CONFIG_SATELLITE
/* END THREADS */

static K_SEM_DEFINE(main_thread_sem, 0, MAX_MAIN_THREAD_SEM); /* Main th. semaphore */
static K_SEM_DEFINE(ble_write_thread_sem, 0, 1);              /* ble write th. semaphore */
static K_SEM_DEFINE(ble_read_thread_sem, 0, 1);               /* ble write th. semaphore */
static K_SEM_DEFINE(flash_thread_sem, 0, 1);                  /* Flash init semaphore */
static K_SEM_DEFINE(sensor_thread_sem, 0, 1);                 /* Sensor init semaphore */
static K_SEM_DEFINE(lr_thread_sem, 0, 1);                     /* LR init semaphore */
#ifdef CONFIG_SATELLITE
static K_SEM_DEFINE(satellite_thread_sem, 0, 1);
#endif // CONFIG_SATELLITE

K_SEM_DEFINE(lora_chip_suspended_sem, 0, 1); /* Semaphore to control lora chip access */

/* Terminal error */
void error(void)
{
	LOG_ERR("Terminal error, reboot the device!");
	k_sleep(K_MSEC(1000));
	sys_reboot(0);
}

/* Handle thread settings messages */
void handle_setting_messages(uint8_t *msg_data)
{
	// Check if new message
	mb_msg_dest msg_origin = 0;
	mb_msg_dest msg_dest = 0;
	mb_msg_action msg_action = 0;
	uint8_t msg_port = 0;
	uint8_t msg_max_rsp_len = 0;
	int msg_size = thread_get_main(&msg_origin, &msg_dest, &msg_action, &msg_port, msg_data,
				       &msg_max_rsp_len);
	if (msg_size > 0) {
		LOG_INF("Main got message from: %d on port: %d of length: %d", msg_origin, msg_port,
			msg_size);
		// Parse message based on the destination
		if (msg_dest == MB_MSG_BT) {
			parse_ble_message(msg_data, msg_size, msg_origin, msg_action, msg_port,
					  msg_max_rsp_len);
		} else if (msg_dest == MB_MSG_DEV) {
			parse_settings_message(msg_data, msg_size, msg_origin, msg_port,
					       msg_max_rsp_len);
		} else {
			LOG_ERR("Invalid message destination.");
		}
	}
}

/*Creation of startup threads*/
K_THREAD_DEFINE(main_com_thread_id, CONFIG_THREAD_MAIN_STACKSIZE, main_com_thread, NULL, NULL, NULL,
		CONFIG_THREAD_PRIORITY, K_ESSENTIAL, 0);
K_THREAD_DEFINE(sensors_thread_id, CONFIG_THREAD_SENSORS_STACKSIZE, sensors_thread, NULL, NULL,
		NULL, CONFIG_THREAD_PRIORITY, 0, 0);
K_THREAD_DEFINE(flash_thread_id, CONFIG_THREAD_FLASH_STACKSIZE, flash_thread, NULL, NULL, NULL,
		CONFIG_THREAD_PRIORITY, 0, 0);
K_THREAD_DEFINE(lora_gps_thread_id, CONFIG_THREAD_LORA_STACKSIZE, lora_gps_thread, NULL, NULL, NULL,
		CONFIG_THREAD_PRIORITY, 0, 0);
K_THREAD_DEFINE(ble_write_thread_id, CONFIG_THREAD_BLE_WRITE_STACKSIZE, ble_write_thread, NULL,
		NULL, NULL, CONFIG_THREAD_PRIORITY, 0, 0);
K_THREAD_DEFINE(ble_read_thread_id, CONFIG_THREAD_BLE_READ_STACKSIZE, ble_read_thread, NULL, NULL,
		NULL, CONFIG_THREAD_PRIORITY, 0, 0);

#ifdef CONFIG_SATELLITE
K_THREAD_DEFINE(satellite_thread_id, CONFIG_THREAD_SATELLITE_STACKSIZE, satellite_thread, NULL,
		NULL, NULL, CONFIG_THREAD_PRIORITY, 0, 0);
#endif // CONFIG_SATELLITE

/*!
 * @brief Main communication thread that handles all the messages from other threads and BLE
 * communication. This thread is Essential which means termination or aborting of the thread as a
 * fatal system error.
 *
 * @return When returned this thread is terminated.
 */
static void main_com_thread(void)
{
	/* If in production mode, start WTD immediately */
#ifndef CONFIG_DEBUG_MODE
#ifndef CONFIG_PROVISIONING_MODE
	LOG_INF("*    Init watchdog     *");
	init_watchdog();
#endif
#endif

	int err = 0;

	/* Init settings module */
	LOG_INF("*    Init settings    *");

	err = init_settings();
	if (err) {
		LOG_ERR("*   Init settings failed!    *");
	}
	LOG_INF("*    Init settings done     *\n");

	/* Init LED */
	led_init();

	LOG_INF("********************************************");
	LOG_INF("**********************************");
	LOG_INF("*   Starting smartparks opencollar edge     *");
	LOG_INF("*      board: %s revision: %s    *", CONFIG_BOARD, CONFIG_BOARD_REVISION);
	LOG_INF("*      build time: " __DATE__ " " __TIME__ "       *");
	LOG_INF("********************************************\n");

	/* Print FW version */
	version_info_print();

	LOG_INF("*    Starting main thread    *");
	LOG_INF("******************************\n");

	/* Init mutexes */
	thread_init_mutex();

#ifdef CONFIG_MCUMGR
	/* Validate image */
	int imgConfirmVal = boot_write_img_confirmed(); // so we dont accidentally revert to
							// previous version (before FOTA)
	LOG_INF("boot_write_img_confirmed() return value: %d", imgConfirmVal);
#endif /* CONFIG_MCUMGR */

	/* Read factory name */
	read_factory_name(Main_values.factory_device_name->def_val);
	LOG_INF("Factory name: %c%c%c%c%c%c%c%c", Main_values.factory_device_name->def_val[0],
		Main_values.factory_device_name->def_val[1],
		Main_values.factory_device_name->def_val[2],
		Main_values.factory_device_name->def_val[3],
		Main_values.factory_device_name->def_val[4],
		Main_values.factory_device_name->def_val[5],
		Main_values.factory_device_name->def_val[6],
		Main_values.factory_device_name->def_val[7]);

	/* Init BLE module */
	LOG_INF("*    Init BLE     *");
	err = init_bt_module();
	if (err) {
		LOG_ERR("*   Init BLE failed!    *");
		error();
	}
	LOG_INF("*    Init BLE done    *\n");

	/* Init global time */
	LOG_INF("*    Init reference time     *");
	init_ref_time();
	LOG_INF("*    Init reference time done    *\n");

	/* Init battery and charging */
	LOG_INF("*    Init battery     *");
	err = battery_interface_init();
	if (err) {
		LOG_ERR("*   Init battery failed!    *");
	}
	LOG_INF("*    Init battery done    *\n");

	LOG_INF("*    Init charging     *");
	err = charging_interface_init();
	if (err) {
		LOG_ERR("*   Init charging failed!    *");
	}
	LOG_INF("*    Init charging done    *\n");

	// Give two semaphores for read and write BLE thread
	k_sem_give(&ble_write_thread_sem);
	k_sem_give(&ble_read_thread_sem);

	if (IS_ENABLED(CONFIG_SETTINGS)) {
		settings_load();
	}

	/* Init status message */
	LOG_INF("*    Init status message     *");
	status_init();
	LOG_INF("*    Init status message done    *\n");

	k_sleep(K_MSEC(500));
	// Give two semaphores for flash and sensors init
	k_sem_give(&sensor_thread_sem);
	k_sem_give(&lr_thread_sem);

	/* Wait for other threads to finish init */
	uint32_t init_start = k_uptime_get_32();

	while (k_sem_count_get(&main_thread_sem) != MAX_MAIN_THREAD_SEM) {
		// Check for timeout
		if ((k_uptime_get_32() - init_start) / 1000 > INIT_TIMEOUT_SEC) {
			LOG_ERR("INIT TIMEOUT, taking more than: %d s. REBOOT!",
				(k_uptime_get_32() - init_start) / 1000);
			sys_reboot(0);
		}
		k_sleep(K_MSEC(100));
	}
	LOG_INF("Got %d semaphores in main, should be: %d", k_sem_count_get(&main_thread_sem),
		MAX_MAIN_THREAD_SEM);
	LOG_INF("**********************************");
	LOG_INF("All threads initialized. Start BT advertising.");
	LOG_INF("**********************************\n");

	// Message variables
	uint8_t message_setting_data[MAX_BUF_SIZE];
	// BLE adv data
	uint8_t adv_message[MAX_ADVERTISEMENT_LEN - 2];

	/* Init adv data with init results */
	battery_interface_handler(); // Get bat mes
	status_get_message(adv_message,
			   MAX_ADVERTISEMENT_LEN -
				   2); // Max length is reduced by first two reserved bytes
	ble_adv_data_set(adv_message, MAX_ADVERTISEMENT_LEN - 2);

	/* Start ble service */
	LOG_INF("*    Start BLE     *");
	err = start_bt_service();
	if (err) {
		LOG_ERR("*   Start BLE failed!    *");
		error();
	}
	LOG_INF("*    Start BLE done     *\n");

	//  If in provisioning mode, wait if test procedure is initiated
#ifdef CONFIG_PROVISIONING_MODE
	if (NRF_UICR->CUSTOMER[0] == 0xFFFFFFFF) {
		printk("Enter test procedure.\n");
		k_sleep(K_MSEC(1000));
		operation_test_setup();
	} else {
		printk("Enter low power test.\n");
		k_sleep(K_MSEC(1000));
		test_low_power();
	}
#endif

#if defined(CONFIG_DEBUG_MODE) || defined(CONFIG_PROVISIONING_MODE)
	/* If in debug mode, start WTD after test */
	LOG_INF("*    Init watchdog     *");
	init_watchdog();
#endif

	// Report to WTD
	main_thread_report();

	// Init REED
	reed_init();

#if !defined(CONFIG_DEBUG_MODE) && !defined(CONFIG_PROVISIONING_MODE)
	/* If in production mode disable uart if needed */
#if DT_NODE_HAS_STATUS(DT_ALIAS(serial_uart), okay)
	const struct device *serial_uart_dev = DEVICE_DT_GET(DT_ALIAS(serial_uart));
	uart_pm_disable(serial_uart_dev->name);
#endif // DT_NODE_HAS_STATUS(DT_ALIAS(serial_uart), okay)
#endif

#ifdef CONFIG_SDFS_UART_MODULE
	/* SD card uart module initialization */
	LOG_INF("*    Init SDFS UART module     *");
	err = sdfs_uart_init(DEVICE_DT_GET(DT_NODELABEL(sdfs)));
	if (err) {
		LOG_ERR("*   Init SDFS UART module failed!    *");
	}
#endif /* CONFIG_SDFS_UART_MODULE */

	/* Give semaphore to other threads to start */
	LOG_INF("**********************************\n");
	LOG_INF("INIT DONE\n");
	LOG_INF("**********************************\n");
	k_sem_give(&sensor_thread_sem);
	k_sleep(K_MSEC(100));
	k_sem_give(&flash_thread_sem);
	k_sleep(K_MSEC(100));
	k_sem_give(&lr_thread_sem);
	k_sleep(K_MSEC(100));

	led_blink(2, LED_R);

	while (true) {
		/* Handle critical errors */
		handle_errors();

		/* Check new setting messages */
		handle_setting_messages(message_setting_data);

		// If general messaging is supported, run trough timers
		if (get_thread_operation(MAIN_THREAD) != THREAD_DISABLED) {
			// Terminate BLE connection if timeout
			bt_con_check_disconnect_period();

			// Update advertisement status if not connected and update period has
			// elapsed

			if (check_adv_data_update_period()) {
				status_get_message(adv_message,
						   MAX_ADVERTISEMENT_LEN -
							   2); // Max length is reduced by first two
							       // reserved bytes

				sys_err.ble =
					ble_adv_data_update(adv_message, MAX_ADVERTISEMENT_LEN - 2);
				if (sys_err.ble) {
					LOG_ERR("Advertising failed to update (err %d)",
						sys_err.ble);
				}
			}
		}

		/* Read battery voltage */
		battery_interface_handler();

		/* Read charging value */
		charging_interface_handler();

		/* Blink led */
		led_handler();

		/* Safety check reed */
		reed_check();

		/* Report to watchdog */
		main_thread_report();

		/* Sleep */
		k_sleep(K_MSEC(thread_sleep));
	}
}

/* Flash thread */
static void flash_thread(void)
{
	// Don't go any further until settings are initialized
	k_sem_take(&flash_thread_sem, K_FOREVER);
	LOG_INF("*    Starting flash thread        *");
	LOG_INF("***********************************\n");
	// Init flash
	LOG_INF("*    Init flash     *");
	int err = flash_ext_partitions_init();
	if (err) {
		LOG_ERR("*   Init external flash partitions failed!    *");
		sys_err.flash = err;
	}
	err = init_flash();
	if (err) {
		LOG_ERR("*   Init flash failed!    *");
		sys_err.flash = err;
	}

	LOG_INF("*    Init flash done    *\n");

#ifdef CONFIG_SATELLITE
	k_sem_give(&satellite_thread_sem);
#endif // CONFIG_SATELLITE

	k_sem_give(&main_thread_sem);

	// Don't go any further until all threads are initialized
	k_sem_take(&flash_thread_sem, K_FOREVER);
	LOG_INF("* Starting flash thread operation *");
	LOG_INF("***********************************\n");
	k_sleep(K_MSEC(100));

	while (true) {
		// Check flash module message box
		handle_flash_thread_messages();

		// Check if flash status report needs to be send
		handle_flash_status();

		// Report to watchdog
		flash_thread_report();
		// Sleep
		k_sleep(K_MSEC(thread_sleep));
	}
}

static void ble_write_thread(void)
{
	// Don't go any further until BLE is initialized
	k_sem_take(&ble_write_thread_sem, K_FOREVER);
	LOG_INF("*    Starting ble write thread    *");
	LOG_INF("***********************************\n");

	for (;;) {
		sys_err.ble = bt_nus_send_data();
	}
}

static void ble_read_thread(void)
{
	// Don't go any further until BLE is initialized
	k_sem_take(&ble_read_thread_sem, K_FOREVER);
	LOG_INF("*    Starting ble read thread    *");
	LOG_INF("**********************************\n");

	for (;;) {
		// Wait indefinitely for received data over bluetooth
		nus_data_t *buf = get_fifo_rx_data();

		if (buf) {
			// LOG_INF("Got BLE message: ");
#ifdef CONFIG_DEBUG_MODE
			printk("Got BLE message: ");
			for (int i = 0; i < buf->len; i++) {
				printk("%x ", buf->data[i]);
			}
			printk("\n");
#endif
			// Send BLE message to main thread
			thread_put_message(MB_MSG_BT, MB_MSG_DEV, MB_MSG_EXECUTE, buf->data[0],
					   buf->data + 1, buf->len - 1, MESSAGE_BT_MAX_LEN);
		}

		k_free(buf);
	}
}

static void sensors_thread(void)
{
	// Don't go any further until settings are initialized
	k_sem_take(&sensor_thread_sem, K_FOREVER);

	LOG_INF("*    Starting sensor thread    *");
	LOG_INF("********************************\n");

	int err = 0;
	// Sensors acc init
	err = sensors_init();
	if (err) {
		LOG_ERR("Error sensor init!");
	}

	// Sensors acc init
	err = temperature_sensor_init();
	if (err) {
		LOG_ERR("Error temp sensor init!");
	}

#ifdef CONFIG_OUTDOOR_DETECTION
	err = outdoor_detection_init();
	if (err) {
		LOG_ERR("Error outdoor detection init!");
	}
#endif /* CONFIG_OUTDOOR_DETECTION */

	// mic
#ifdef CONFIG_ENABLE_MIC
	err = mic_init();
	if (err) {
		LOG_ERR("Error mic init!");
	}
#endif

	// report successful init to main thread
	k_sem_give(&main_thread_sem);
	// Don't go any further until all threads are initialized
	k_sem_take(&sensor_thread_sem, K_FOREVER);

	LOG_INF("* Starting sensor thread operation *");
	LOG_INF("********************************\n");

	while (true) {
		// Handle msg commands
		sensors_handle_commands();
		// Call all available sensors
		sensors_handle();
		// Update temperature
		temperature_sensor_handle();

		// Handle outdoor detection
#ifdef CONFIG_OUTDOOR_DETECTION
		outdoor_detection_handle();
#endif /* CONFIG_OUTDOOR_DETECTION */

		// Report to watchdog
		sensor_thread_report();

		k_sleep(K_SECONDS(2));
	}
}

static void lora_gps_thread(void)
{
	int err = 0;
	/* Wait for main thread */
	k_sem_take(&lr_thread_sem, K_FOREVER);

	LOG_INF("***************************************");
	LOG_INF("*    Starting communication thread    *");
	LOG_INF("***************************************");

	/* Ublox GPS */
	LOG_INF("Start GPS.");
	err = init_gps_module();
	if (err) {
		LOG_ERR("Failed to init GPS");
	}

	/* LR11XX - start smtc engine */
	lr_start();

#ifdef CONFIG_FENCE_PORT
	/* Initialization of fence port features */
	err = fence_port_common_init();
	if (err) {
		LOG_ERR("Failed to init fence port common: %d", err);
	}
	sys_features.fence = Main_settings.fence_enabled->def_val;

#endif /* CONFIG_FENCE_PORT */

	/* Start flash init */
	k_sem_give(&flash_thread_sem);
	/* Report to main thread */
	k_sem_give(&main_thread_sem);

	/* Don't go any further until all systems are initialized */
	k_sem_take(&lr_thread_sem, K_FOREVER);

	LOG_INF("*******************************************");
	LOG_INF("* Starting communication thread operation *");
	LOG_INF("*******************************************");

	while (true) {
		/* Check if new messages are received in the thread */
		handle_communication_thread_messages();

		if (get_thread_operation(LORA_GPS_THREAD) != THREAD_DISABLED) {

			/* Check if we to attempt lora join */
			handle_lr_join();

			/* Check if we need to send status via lora/satellite/flash */
			handle_send_status();

			if (get_thread_operation(LORA_GPS_THREAD) == THREAD_NORMAL) {

				/* Check if we need to obtain lora position and send it via
				 * lora/satellite/flash */
				handle_lr_gps();

				/* Check if we need to preform lora wifi scan and send it via
				 * lora/satellite/flash */
				handle_wifi_scan();

				/* Check if we need to send aggregated wifi scan data via
				 * lora/satellite/flash */
				handle_wifi_scan_aggregated();

				/* Check if we need to preform BT scan and send it via
				 * lora/satellite/flash */
				handle_bt_scan();

				/* Check if we need to send aggregated BT scan data via
				 * lora/satellite/flash */
				handle_bt_scan_aggregated();

				/* Check if we need to perform CMDQ BT scan and send data via
				 * lora/sattelite/flash */
				handle_bt_cmdq();

				/* Check if we need to obtain UBLOX position and send it via
				 * lora/satellite/flash */
				handle_ublox_gps();

				// Check if we need to resend latest valid position
				handle_resend_ublox_gps();

				/* Check if we need to send user lora messages */
				handle_lr_messaging();

				/* Check if we need to send messages using lr s-band */
				handle_s_band_send();

				handle_vhf_burst();

				/* Check if we need to send satellite messages */
				handle_satellite_send();

#ifdef CONFIG_FENCE_PORT
				/* Check fence port settings */
				fence_port_common_check_settings();

				/* Check if we need to start fence monitoring and send data via
				 * lora/satellite/flash */
				handle_fence();

				/* Check if we need to start external switch monitoring */
				handle_external_switch();

#endif /* CONFIG_FENCE_PORT */
				handle_air_quality();
			}
		}

		/* Report thread to watchdog */
		lr_gps_thread_report();

		/* Thread sleep */
		k_sleep(K_MSEC(thread_sleep));
	}
}

#ifdef CONFIG_SATELLITE
static void satellite_thread(void)
{
	// Wait for init sequence
	k_sem_take(&satellite_thread_sem, K_FOREVER);

	LOG_INF("*    Starting satellite thread    *");
	LOG_INF("**********************************\n");

	// Uart
	LOG_INF("Init satellite module.");
	int err = satellite_init();
	if (err) {
		LOG_ERR("Failed to init satellite module");
	}

	k_sem_give(&main_thread_sem);

	for (;;) {
		// Indefinitely wait for new command
		satellite_handle_commands();

		k_sleep(K_SECONDS(2));
	}
}
#endif // CONFIG_SATELLITE
