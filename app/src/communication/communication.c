/** @file communication.c
 *
 * @brief Interface to Lora and gps communication
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2020 Irnas. All rights reserved.
 */
#include <stdio.h>
#include <string.h>
#include <zephyr/device.h>
#include <zephyr/drivers/counter.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include "commands_def.h"
#include "communication.h"

#include "almanac.h"
#include "gnss.h"
#include "lorawan.h"
#include "lp0.h"
#include "lr11xx_almanac.h"

#ifdef CONFIG_OUTDOOR_DETECTION
#include "outdoor_detection.h"
#endif /* CONFIG_OUTDOOR_DETECTION */

#include "zephyr/sys/reboot.h"

#ifdef CONFIG_LR_WIFI_SCAN
#include "wifi_scan.h"
#include "wifi_scan_data.h"
#endif /* CONFIG_LR_WIFI_SCAN */

#ifdef CONFIG_LR_S_BAND
#include "lr_s_band.h"
#endif /* CONFIG_LR_S_BAND */

#ifdef CONFIG_BT_CMDQ
#include "bt_cmdq.h"
#include "bt_cmdq_messaging.h"
#endif /* CONFIG_BT_CMDQ */

#include "common_functions.h"
#include "generated_settings.h"
#include "global_time.h"
#include "gps.h"
#include "led.h"
#include "lr_messaging.h"
#include "messages_def.h"
#include "nvs_storage.h"
#include "status.h"
#include "thread_com.h"
#include "thread_operation.h"
#include "thread_watchdog.h"

#ifdef CONFIG_SATELLITE
#include "satellite.h"
#endif // CONFIG_SATELLITE

#include "vhf.h"

#ifdef CONFIG_FENCE_PORT
#include "external_switch.h"
#include "fence.h"
#endif // CONFIG_FENCE_PORT

#ifdef CONFIG_AIR_QUALITY
#include <air_quality.h>
#endif /* CONFIG_AIR_QUALITY */

#define LR_ALMANAC_UPDATE_TIMEOUT_MS            30000
#define EXTERNAL_SWITCH_DETECTION_ON_BOOT_DELAY 30
#define AIR_QUALITY_DETECTION_ON_BOOT_DELAY_S   30
#define CMDQ_TIME_BEFORE_FIRST_SCAN_S           60

/* Payload local variables */
static uint8_t payload[MAX_BUF_SIZE];
static uint8_t payload_size = 0;
static uint8_t port = 0;

/* Last action timestamps */
static uint64_t last_rejoin_attempt = 0;
static uint64_t last_lr_gps_send = 0;
static uint64_t last_status_send = 0;
#ifdef CONFIG_LR_WIFI_SCAN
static uint64_t last_wifi_scan = 0;
static uint64_t last_wifi_scan_send = 0;
#endif /* CONFIG_LR_WIFI_SCAN */
static uint64_t last_bt_scan = 0;
static uint64_t last_bt_scan_send = 0;
static uint64_t last_lr_messages_send = 0;
#ifdef CONFIG_LR_S_BAND
static uint64_t last_s_band_send = 0;
#endif /* CONFIG_LR_S_BAND */
static uint64_t last_satellite_send = 0;

#ifdef CONFIG_AIR_QUALITY
static uint64_t last_air_quality_send = 0;
#endif /* CONFIG_AIR_QUALITY */

#ifdef CONFIG_FENCE_PORT
static uint64_t last_fence_measurement = 0;
static uint64_t last_external_switch_interval = 0;
static uint32_t prv_external_switch_number_of_active_detections = 0;
#endif /* CONFIG_FENCE_PORT */

static uint64_t last_gps_resend = 0;

#ifdef CONFIG_BT_CMDQ
static uint64_t last_cmdq_message_sent = 0;
#endif /* CONFIG_BT_CMDQ */

static uint64_t last_vhf_burst = 0;

/* LR callback event variables */
/* LR GNSS */
static bool lr_gnss_done = false;
static int lr_gnss_err = 0;

/* LR Almanac */
static bool lr_almanac_update_done = false;
static int lr_almanac_update_err = 0;

#ifdef CONFIG_LR_WIFI_SCAN
static bool lr_wifi_scan_done = false;
static int lr_wifi_scan_err = 0;
#endif /* CONFIG_LR_WIFI_SCAN */

#ifdef CONFIG_LR_S_BAND
static bool lr_s_band_tx_done = false;
#endif /* CONFIG_LR_S_BAND */

// Satellite
#ifdef CONFIG_SATELLITE
static bool satellite_immediate_send = false;
#endif // CONFIG_SATELLITE

LOG_MODULE_REGISTER(communication, 4); // init logging

/*
===============================================================================
Private functions
===============================================================================
*/

/**
 * @brief Disable communication thread.
 *
 */
static void prv_disable_communication_thread(void)
{
	LOG_INF("Disabling communication thread...");

	/* Disable GPS */
	LOG_INF("Disable GPS");
	gps_stop();
	sys_err.ublox = 0;
	LOG_INF("Disable GPS done");

	/* Disable lora */
	LOG_INF("Disable LR");
	lorawan_suspend();
	sys_err.lr = 0;
	LOG_INF("Disable LR done");

	/* Disable thread operation */
	LOG_INF("Disable LR and GPS thread loop");
	set_thread_operation(LORA_GPS_THREAD, THREAD_DISABLED);
}

/**
 * @brief Enable communication thread.
 *
 */
static void prv_enable_communication_thread(void)
{
	/* Restart GPS module */
	if (!gps_get_enabled()) {
		LOG_INF("Enable GPS");
		sys_err.ublox = init_gps_module();
		LOG_INF("Enable GPS done: %d", sys_err.ublox);
	} else {
		LOG_INF("Ublox GPS enabled!");
	}

	/* Enable lorawan */
	if (!lorawan_is_enabled()) {
		LOG_INF("Enable LR");
		lorawan_resume();

	} else {
		LOG_INF("LR enabled!");
	}

	/* Enable thread operation */
	set_thread_operation(LORA_GPS_THREAD, THREAD_NORMAL);
}

/**
 * @brief Disable communication thread.
 *
 */
static void prv_low_power_communication_thread(void)
{
	LOG_INF("Set communication thread to low power...");

	/* Disable GPS */
	LOG_INF("Disable GPS");
	gps_stop();
	sys_err.ublox = -EIO;
	LOG_INF("Disable GPS done");

	/* Set thread operation to low power */
	LOG_INF("Set LR and GPS thread loop to LP");
	set_thread_operation(LORA_GPS_THREAD, THREAD_LOW_POWER);
}

/*!
 * @brief Compose max payload size based on message destination.
 *
 * @param msg_dest- LR, flash or BT
 * @return max msg size
 */
static uint8_t prv_get_max_payload(mb_msg_dest msg_dest)
{
	/* Set max size */
	uint8_t size = MESSAGE_LR_MAX_LEN;
	if (msg_dest == MB_MSG_LORA) {
		/* Check allowed message size with LoRaWAN */
		int tmp = lorawan_get_max_payload();
		if (tmp > 0 && tmp < size) {
			size = tmp;
		}
	} else if (msg_dest == MB_MSG_FLASH) {
		/* Since flash messages need to be appropriate size to be sent via LoRaWAN, check
		 * limitation */
		int tmp = lorawan_get_max_payload();
		if (tmp > 0 && tmp < size) {
			size = tmp;
		}
		/* Subtract port and timestamp length */
		size -= 5;
	} else if (msg_dest == MB_MSG_BT) {
		/* Subtract port byte */
		size = MESSAGE_BT_MAX_LEN - 1;
	}

	LOG_DBG("Max length: %d for dest: %d", size, msg_dest);

	return size;
}

/*
===============================================================================
Private functions - timers
===============================================================================
*/

/**
 * @brief Check if it is time to attempt rejoin
 *
 * @return true - attempt rejoin
 * @return false - do not attempt rejoin
 */
static bool prv_check_lr_join_interval(void)
{
	if (!lorawan_is_enabled()) {
		return false;
	}
	if (lorawan_is_joined()) {
		return false;
	}

	/* If set to 0, set it to max interval value */
	if (Main_settings.rejoin_interval->def_val == 0) {
		Main_settings.rejoin_interval->def_val = Main_settings.rejoin_interval->max;
		nvs_storage_write(Main_settings.rejoin_interval->id,
				  &Main_settings.rejoin_interval->def_val,
				  Main_settings.rejoin_interval->len);
		LOG_WRN("User tried to turn off re-join. Set it to max value of: %d s",
			Main_settings.rejoin_interval->def_val);
	}

	if ((uint32_t)((k_uptime_get() - last_rejoin_attempt) / 1000) >
	    Main_settings.rejoin_interval->def_val) {
		LOG_INF("Attempt to rejoin.");
		return true;
	}

	return false;
}

/**
 * @brief Check if it is time to acquire LR position and send it via LoRa
 *
 * @return true - acquire LR position
 * @return false - do not acquire LR position
 */
static bool prv_check_lr_gps_interval(void)
{
	if (Main_settings.lr_gps_interval->def_val > 0) {
		if ((uint32_t)((k_uptime_get() - last_lr_gps_send) / 1000) >
			    Main_settings.lr_gps_interval->def_val ||
		    last_lr_gps_send == 0) {
			LOG_INF("Get LR fix and send via LoRa.");
			return true;
		}
	}

	return false;
}

/**
 * @brief Check if it is time to send status it via LoRa/store to flash
 *
 * @return true - send/store status
 * @return false - do not send/store status
 */
static bool prv_check_send_status_interval(void)
{
	// If set to 0, set it to max interval value
	if (Main_settings.status_send_interval->def_val == 0) {
		Main_settings.status_send_interval->def_val =
			Main_settings.status_send_interval->max;
		nvs_storage_write(Main_settings.status_send_interval->id,
				  &Main_settings.status_send_interval->def_val,
				  Main_settings.status_send_interval->len);
		LOG_WRN("User tried to turn off send status. Set it to max value of: %d s",
			Main_settings.status_send_interval->def_val);
	}
	if ((uint32_t)((k_uptime_get() - last_status_send) / 1000) >
		    Main_settings.status_send_interval->def_val ||
	    last_status_send == 0) {
		LOG_INF("Generate new status.");
		return true;
	}
	return false;
}

/**
 * @brief Check if it is time to perform LR WiFi scan
 *
 * @return true - perform scan
 * @return false - do not perform scan
 */
static bool prv_check_scan_wifi_interval(void)
{
#ifdef CONFIG_LR_WIFI_SCAN
	if (Main_settings.wifi_scan_interval->def_val > 0) {
		if (((uint32_t)((k_uptime_get() - last_wifi_scan) / 1000) >
		     Main_settings.wifi_scan_interval->def_val) ||
		    last_wifi_scan == 0) {
			LOG_INF("Perform WiFi scan.");
			return true;
		}
	}
#endif /* CONFIG_LR_WIFI_SCAN */
	return false;
}

/**
 * @brief Check if it is time to send aggregated WiFi scan data
 *
 * @return true - send
 * @return false - do not send
 */
static bool prv_check_wifi_scan_aggregated_interval(void)
{
#ifdef CONFIG_LR_WIFI_SCAN
	if (!lorawan_is_enabled()) {
		return false;
	}
	if (Main_settings.wifi_scan_aggregated_interval->def_val > 0) {
		if ((uint32_t)((k_uptime_get() - last_wifi_scan_send) / 1000) >
			    Main_settings.wifi_scan_aggregated_interval->def_val ||
		    last_wifi_scan_send == 0) {
			LOG_INF("Send WiFi scan aggregated data.");
			return true;
		}
	}
#endif /* CONFIG_LR_WIFI_SCAN */
	return false;
}

/**
 * @brief Check if BT scan needs to be performed.
 *
 * Check if scan interval has passed and scanning is
 * turned on. Update last scan time.
 *
 * @return true - perform scan
 * @return false - do not perform scan
 */
static bool prv_check_bt_scan_period(void)
{
	if ((uint32_t)((k_uptime_get() - last_bt_scan) / 1000) >
		    Main_settings.ble_scan_interval->def_val &&
	    Main_settings.ble_scan_interval->def_val) {
		return true;
	}
	return false;
}

/**
 * @brief Check if BT aggregated scan data needs to be send. Notify BT module.
 *
 * @return true - send
 * @return false - do not send
 */
static bool prv_check_bt_scan_aggregated_period(void)
{
	if ((uint32_t)((k_uptime_get() - last_bt_scan_send) / 1000) >
		    Main_settings.ble_scan_aggregated_interval->def_val &&
	    Main_settings.ble_scan_aggregated_interval->def_val) {
		return true;
	}
	return false;
}

/**
 * @brief Check if its time to send outgoing LR messages in messaging buffer
 *
 * @return true - send
 * @return false - do not send
 */
static bool prv_check_send_lr_messages_interval(void)
{
	if (!lorawan_is_enabled()) {
		return false;
	}

	if (Main_settings.lr_messaging_retry_interval->def_val > 0) {
		if ((uint32_t)((k_uptime_get() - last_lr_messages_send) / 1000) >
			    Main_settings.lr_messaging_retry_interval->def_val ||
		    last_lr_messages_send == 0) {
			LOG_INF("Send LR outgoing messages.");
			return true;
		}
	}

	return false;
}

/**
 * @brief Check if its time to send LR S-band messages
 *
 * @return true
 * @return false
 */
static bool prv_check_send_s_band_interval(void)
{
#ifdef CONFIG_LR_S_BAND
	if (Main_settings.s_band_send_interval->def_val > 0) {
		if ((uint32_t)((k_uptime_get() - last_s_band_send) / 1000) >
			    Main_settings.s_band_send_interval->def_val ||
		    last_s_band_send == 0) {
			LOG_INF("Send LR S-band messages.");
			return true;
		}
	}

#endif /* CONFIG_LR_S_BAND */
	return false;
}

/**
 * @brief Check if we need to send new message via satellite module
 *
 * @return true - send
 * @return false - do not send
 */
static bool prv_check_send_satellite_interval(void)
{
#ifdef CONFIG_SATELLITE
	if (Main_settings.satellite_enabled->def_val) {

		uint8_t interval = satellite_get_current_time_interval();

		if (interval == 1) {
			/* Check if scan disabled */
			if (Main_settings.satellite_send_interval->def_val == 0) {
				return false;
			}
			/* Check if it's time to perform a send */
			if ((uint32_t)(k_uptime_get() - last_satellite_send) / 1000 >
				    Main_settings.satellite_send_interval->def_val ||
			    satellite_immediate_send) {
				LOG_INF("Send satellite buffer.");
				satellite_immediate_send = false;
				return true;
			}
		} else if (interval == 2) {
			/* Check if scan disabled */
			if (Main_settings.satellite_send_interval2->def_val == 0) {
				return false;
			}
			/* Check if it's time to perform a send */
			if ((uint32_t)(k_uptime_get() - last_satellite_send) / 1000 >
				    Main_settings.satellite_send_interval2->def_val ||
			    satellite_immediate_send) {
				LOG_INF("Send satellite buffer.");
				satellite_immediate_send = false;
				return true;
			}
		}
	}
#endif /* CONFIG_SATELLITE */

	return false;
}

/**
 * @brief Check if we need to perform VHF burst.
 *
 * NOTE: If rf scan interval 1 or 2 are set to a non-zero value, they must be at least 1s long.
 * Setting interval1 or interval2 bellow 1s will disable the scan for that interval.
 *
 * @return true - perform scan
 * @return false - do not scan
 */
static bool prv_check_vhf_interval(void)
{
	/* Get minimum allowed interval duration */
	uint32_t min_sec = vhf_get_interval_min_seconds();
	/* Check which interval are we currently in */
	uint8_t interval = vhf_get_current_time_interval();

	if (interval == 1) {
		/* Check if scan disabled */
		if (Main_settings.vhf_interval1->def_val < min_sec) {
			return false;
		}
		/* Check if it's time to perform a scan */
		if ((uint32_t)(k_uptime_get() - last_vhf_burst) / 1000 >=
			    Main_settings.vhf_interval1->def_val ||
		    last_vhf_burst == 0) {
			LOG_INF("Perform VHF burst.");
			return true;
		}
	}
	if (interval == 2) {
		/* Check if scan disabled */
		if (Main_settings.vhf_interval2->def_val < min_sec) {
			return false;
		}
		/* Check if it's time to perform a scan */
		if ((uint32_t)(k_uptime_get() - last_vhf_burst) / 1000 >=
			    Main_settings.vhf_interval2->def_val ||
		    last_vhf_burst == 0) {
			LOG_INF("Perform VHF burst.");
			return true;
		}
	}
	return false;
}

#ifdef CONFIG_FENCE_PORT
/**
 * @brief Check if we need to perform fence measurement
 *
 * @return true
 * @return false
 */
static bool prv_check_fence_interval(void)
{
	if (sys_features.fence) {
		if (Main_settings.fence_interval->def_val > 0) {
			if (((uint32_t)(k_uptime_get() - last_fence_measurement) / 1000 >
			     Main_settings.fence_interval->def_val)) {
				LOG_INF("Perform fence measurement.");
				return true;
			}
		}
	}

	return false;
}

/**
 * @brief Check if we need to poll external switch state.
 *
 * @return true
 * @return false
 */
static bool prv_check_external_switch_interval(void)
{
	if (Main_settings.external_switch_detection_reporting_interval->def_val > 0) {
		if ((uint32_t)((k_uptime_get() - last_external_switch_interval) / 1000) >
		    Main_settings.external_switch_detection_reporting_interval->def_val) {
			last_external_switch_interval = k_uptime_get();
			return true;
		}
	}

	return false;
}
#endif /* CONFIG_FENCE_PORT */

/**
 * @brief Check if latest GPS data needs to be resent.
 *
 * @return true
 * @return false
 */
static bool prv_check_gps_resend_interval(void)
{
	if (Main_settings.gps_resend_interval->def_val > 0) {
		if ((uint32_t)((k_uptime_get() - last_gps_resend) / 1000) >
		    Main_settings.gps_resend_interval->def_val) {
			LOG_INF("Resend GPS data.");
			return true;
		}
	}
	return false;
}

#ifdef CONFIG_BT_CMDQ
/**
 * @brief Check if latest GPS data needs to be resent.
 *
 * @retval true If it is time to send the message,
 * @retval false otherwise
 */
static bool prv_check_bt_cmdq_send_interval(void)
{
	if (Main_settings.cmdq_reporting_interval->def_val > 0) {
		if ((uint32_t)((k_uptime_get() - last_cmdq_message_sent) / 1000) >
		    Main_settings.cmdq_reporting_interval->def_val) {
			return true;
		}
	}
	return false;
}
#endif /* CONFIG_BT_CMDQ */

#ifdef CONFIG_AIR_QUALITY
/**
 * @brief Check if it is time to acquire air quality data and send it via LoRa
 *
 * @return true - acquire and send air quality data
 * @return false - do not acquire and send air quality data
 */
static bool prv_check_air_quality_interval(void)
{
	if (Main_settings.air_quality_interval->def_val > 0 &&
	    Main_settings.air_quality_enabled->def_val &&
	    k_uptime_get() < AIR_QUALITY_DETECTION_ON_BOOT_DELAY_S * 1000) {
		return false;
	}
	if (((k_uptime_get() - last_air_quality_send) / 1000) >
		    Main_settings.air_quality_interval->def_val ||
	    last_air_quality_send == 0) {
		LOG_INF("Get air quality data and send via LoRa.");
		last_air_quality_send = k_uptime_get();
		return true;
	}
	return false;
}
#endif /* CONFIG_AIR_QUALITY */

/*
===============================================================================
Private functions - LR
===============================================================================
*/

/**
 * @brief Send loraWAN message.
 *
 * @param[in] uint8_t message_length  length of send message.
 * @param[in] uint8_t *message  pointer to the message.
 * @param[in] uint8_t port  port to send Lora message on.
 * @param[in] bool tx_confirm - request to send message as confirmed
 * @param[in] bool join - try to join if not yet joined
 * @param[in] bool rejoin - if the device should first disconnect the network and then join
 *
 * @retval 0 if message was added to send que
 * @retval -EMSGSIZE if message is too long
 */
static int prv_lr_send(uint8_t message_length, uint8_t *message, uint8_t port, bool tx_confirm,
		       bool join_attempt, bool rejoin)
{
	if (rejoin) {
		LOG_DBG("Leaving network before joining...");
		/* Leave network before re-joining */
		lorawan_leave_network();
	}

	return lorawan_send_message(message, message_length, port, tx_confirm, join_attempt);
}

/**
 * @brief Attempt LoRaWAN join and sending status message.
 *
 * @param[in] rejoin if we should leave the network before joining or not
 *
 * @retval 0 - join attempted
 * @retval -EMSGSIZE - status message is too long to send
 */
static int prv_lr_join(bool rejoin)
{
	LOG_INF("Attempt LoRaWAN join");

	/* get status */
	int err = 0;
	port = Main_messages.msg_status->port;
	// Add header
	payload[0] = Main_messages.msg_status->id;
	payload_size = status_get_message(payload + MESSAGE_HEAD_LEN,
					  prv_get_max_payload(MB_MSG_LORA) - MESSAGE_HEAD_LEN);
	payload[1] = payload_size;
	payload_size += MESSAGE_HEAD_LEN;

	err = prv_lr_send(payload_size, payload, port, false, true, rejoin);

	return err;
}

/**
 * @brief Start almanac update.
 *
 * @return int error code
 */
static int prv_lr_update_almanac(void)
{
	LOG_INF("Update almanac.");

	lorawan_update_almanac();

	int64_t uptime = k_uptime_get();
	/* We need to block other functions until we do not get almanac cb */
	while (!lr_almanac_update_done) {
		if ((k_uptime_get() - uptime) > LR_ALMANAC_UPDATE_TIMEOUT_MS) {
			LOG_ERR("Almanac update timeout.");
			lr_almanac_update_err = -ETIMEDOUT;
			break;
		}
		k_sleep(K_MSEC(100));
	}

	LOG_INF("Update almanac done, err: %d.", lr_almanac_update_err);

	return lr_almanac_update_err;
}

/**
 * @brief Send and or store payload based on port flag settings.
 *
 * @param[in] port
 */
static void prv_send_and_store_payload(uint8_t port)
{
	if (check_flash_store_flag(port)) {
		thread_put_message(MB_MSG_LORA, MB_MSG_FLASH, MB_MSG_STORE, port, payload,
				   payload_size, 0);
	}
	if (check_sat_send_flag(port)) {
		thread_put_message(MB_MSG_LORA, MB_MSG_SAT, MB_MSG_SEND, port, payload,
				   payload_size, 0);
	}
	if (check_lr_send_flag(port)) {
		prv_lr_send(payload_size, payload, port, check_lr_confirm_flag(port),
			    check_lr_join_flag(port), false);
	}
	if (check_lp0_send_flag(port)) {
		/* Add event to LP0 que */
		lp0_add_message_to_send_queue(payload, payload_size, port, false, false);
	}
}

/**
 * @brief Obtain device status and based on set flags store to flash and/or send via lorawan and/or
 * send via satellite.
 *
 */
void prv_send_status(void)
{
	/* status port */
	port = Main_messages.msg_status->port;
	/* msg header */
	payload[0] = Main_messages.msg_status->id;

	if (check_flash_store_flag(port)) {
		LOG_INF("Store device status to flash.");
		/* get status */
		payload_size =
			status_get_message(payload + MESSAGE_HEAD_LEN,
					   prv_get_max_payload(MB_MSG_FLASH) - MESSAGE_HEAD_LEN);
		payload[1] = payload_size;
		payload_size += MESSAGE_HEAD_LEN;
		thread_put_message(MB_MSG_LORA, MB_MSG_FLASH, MB_MSG_STORE, port, payload,
				   payload_size, 0);
	}
	if (check_lr_send_flag(port) || check_sat_send_flag(port) || check_lp0_send_flag(port)) {
		LOG_INF("Send device status via LoRa.");
		/* get status */
		payload_size =
			status_get_message(payload + MESSAGE_HEAD_LEN,
					   prv_get_max_payload(MB_MSG_LORA) - MESSAGE_HEAD_LEN);
		payload[1] = payload_size;
		payload_size += MESSAGE_HEAD_LEN;
		if (check_sat_send_flag(port)) {
			thread_put_message(MB_MSG_LORA, MB_MSG_SAT, MB_MSG_SEND, port, payload,
					   payload_size, 0);
		}
		if (check_lr_send_flag(port)) {
			prv_lr_send(payload_size, payload, port, check_lr_confirm_flag(port),
				    check_lr_join_flag(port), false);
		}
		if (check_lp0_send_flag(port)) {
			/* Add event to LP0 que */
			lp0_add_message_to_send_queue(payload, payload_size, port, false, false);
		}
	}
}

/*
===============================================================================
Private functions - LR GNSS scan
===============================================================================
*/

/*!
 * @brief Get latest gnss scan results and compose gnss message.
 *
 */
static void prv_compose_message_lr_gnss(void)
{
	uint8_t data_len = gnss_get_last_nav_data(payload + MESSAGE_HEAD_LEN);

	payload[0] = Main_messages.msg_gnss->id;
	payload[1] = data_len;

	payload_size = data_len + MESSAGE_HEAD_LEN;
	port = Main_messages.msg_gnss->port;
}

/**
 * @brief Compose satellite data message, given gnss scan results.
 *
 */
static void prv_compose_message_lr_satellites(void)
{
	uint8_t n_sat;
	uint8_t data_len = gnss_get_last_sat_data(payload + 1 + MESSAGE_HEAD_LEN, &n_sat);

	payload[0] = Main_messages.msg_lr_satellites->id;
	payload[1] = data_len + 1;
	payload[2] = n_sat;

	payload_size = data_len + 1 + MESSAGE_HEAD_LEN;
	port = Main_messages.msg_lr_satellites->port;
}

/**
 * @brief Send command to lora to start gnss scan. Wait for gnss result handler to return before
 * exiting. NOTE: user is responsible for registering handler prior to calling gnss scan function,
 * otherwise handler will never return!
 *
 * @retval 0 - gnss scan done
 * @retval -EIO - lorawan module disabled
 * @retval -EBUSY - lorawan module is busy with joining process
 */
static int prv_lr_gnss_scan(void)
{
	if (!lorawan_is_enabled()) {
		LOG_ERR("LR module disabled!");
		return -EIO;
	}

	if (lorawan_joining_status()) {
		LOG_WRN("Device is in the middle of joining process. Postpone GNSS scan.");
		return -EBUSY;
	}

	lr_gnss_done = false;

	LOG_INF("Call LR GNSS scan.");
	if (Main_settings.gnss_assisted_scan->def_val) {
		lorawan_gnss_scan_assisted(Main_values.gps_lat->def_val,
					   Main_values.gps_lon->def_val, get_global_gps_time());
	} else {
		lorawan_gnss_scan_autonomous();
	}

	/* We need to block other functions until we do not get gnss cb */
	while (!lr_gnss_done) {
		k_sleep(K_MSEC(100));
	}

	return lr_gnss_err;
}

/*
===============================================================================
Private functions - LR WiFi scan
===============================================================================
*/

#ifdef CONFIG_LR_WIFI_SCAN

/**
 * @brief Given max payload length, compose message for aggregated scan.
 *
 * @param[in] max_len max payload length
 * @param[in] max_res max number of results to include
 * @param[in] clear_flag clear results after composing
 */
static void prv_compose_message_lr_wifi_scan_aggregated(uint8_t max_len, uint8_t max_res,
							bool clear_flag)
{
	payload[0] = Main_messages.msg_wifi_scan_aggregated->id;
	port = Main_messages.msg_wifi_scan_aggregated->port;

	payload_size = compose_message_wifi_scan_results(
		payload + MESSAGE_HEAD_LEN, max_len - MESSAGE_HEAD_LEN, max_res, clear_flag);

	payload[1] = payload_size;
	payload_size += MESSAGE_HEAD_LEN;
}

/**
 * @brief Given max payload length, compose message for latest scan.
 *
 * @param[in] max_len max payload length
 */
static void prv_compose_message_lr_wifi_single_scan(uint8_t max_len)
{
	payload[0] = Main_messages.msg_wifi_scan->id;
	port = Main_messages.msg_wifi_scan->port;

	/* Get results in payload format */
	payload_size = compose_message_wifi_scan_results_single_scan(payload + MESSAGE_HEAD_LEN,
								     max_len - MESSAGE_HEAD_LEN);
	payload[1] = payload_size;
	payload_size += MESSAGE_HEAD_LEN; // Add header
}

/**
 * @brief Get wifi scan data and send it via LR.
 *
 *
 * @return negative integer error code, 0 - OK
 */
static int prv_lr_wifi_scan(void)
{
	if (!lorawan_is_enabled()) {
		LOG_ERR("LR module disabled!");
		return -EIO;
	}

	if (lorawan_joining_status()) {
		LOG_WRN("Device is in the middle of joining process. Postpone WiFi scan.");
		return -EBUSY;
	}

	lr_wifi_scan_done = false;

	/* Request scan */
	lorawan_wifi_scan();

	while (!lr_wifi_scan_done) {
		k_sleep(K_MSEC(100));
	}

	return lr_wifi_scan_err;
}
#endif /* CONFIG_LR_WIFI_SCAN */

/*
===============================================================================
Private functions - LR messaging module
===============================================================================
*/

/**
 * @brief Send outgoing lr messages in lr messaging buffer
 *
 */
static void prv_lr_send_messages(void)
{
	int rc = 0;
	/* Get number of messages in outgoing buffer */
	uint8_t n = lr_messaging_get_nr_outgoing_msg();
	LOG_INF("Number of outgoing messages to send: %d", n);
	if (n == 0) {
		return;
	}

	/* Send all messages */
	for (uint8_t i = 0; i < n; i++) {
		/* Compose new message */
		rc = lr_messaging_send_outgoing_msg(payload);
		if (rc <= 0) {
			/* No more messages to send */
			return;
		}
		/* Send new message */
		LOG_INF("Sending message %d out of %d", i + 1, n);
		prv_lr_send((uint8_t)rc, payload, PORT_LR_MESSAGING, 0, 0, false);
	}
}

/*
===============================================================================
Private functions - LR on event external handlers
===============================================================================
*/

void handle_lora_recv(const uint8_t *message, uint8_t size, uint8_t port)
{
	/* Special parse of commands that are LP0 related */
	if (port == Main_messages.msg_lp0_commands->port) {
		lp0_command_parser(message, size);
		return;
	}

	LOG_INF("lora_recv_handler triggered!");
	/* Handle LR messaging */
	if (port == PORT_LR_MESSAGING) {
		thread_put_message(MB_MSG_LORA, MB_MSG_LORA, MB_MSG_STORE, port, (uint8_t *)message,
				   size, 0);
	}
	/* Send message to main thread */
	else {
		thread_put_message(MB_MSG_LORA, MB_MSG_DEV, MB_MSG_EXECUTE, port,
				   (uint8_t *)message, size, prv_get_max_payload(MB_MSG_LORA));
	}

#ifdef CONFIG_DEBUG_MODE
	printk("RCV Port: %d, Size: %d, Msg: ", port, size);
	for (int i = 0; i < size; i++) {
		printk("%x ", message[i]);
	}
	printk("\n");
#endif /* CONFIG_DEBUG_MODE */
}

/**
 * @brief External handler for GNSS scan done event.
 *
 * @param[in] err - gnss scan error code
 */
static void prv_handle_gnss_done(int err)
{
	lr_gnss_err = err;
	if (err) {
		LOG_ERR("GNSS scan failed!");
	}

	lr_gnss_done = true;
}

/**
 * @brief External handler for almanac update done event.
 *
 * @param[in] err - almanac update error code
 * @param[in] age - current almanac age
 */
static void prv_handle_almanac_update(int err, uint16_t age)
{
	lr_almanac_update_err = err;

	if (err) {
		LOG_ERR("Almanac update failed!");
	} else {
		LOG_INF("Almanac update successful!");
	}

	Main_values.almanac_age->def_val = age;

	lr_almanac_update_done = true;
}

#ifdef CONFIG_LR_WIFI_SCAN
/**
 * @brief External handler for WiFi scan done event.
 *
 * @param[in] err - WiFi scan error code
 */
static void prv_handle_wifi_scan_done(int err)
{
	lr_wifi_scan_err = err;
	if (err) {
		LOG_ERR("WiFi scan failed!");
	} else {
		/* Invoke storing of WiFi scan results */
		wifi_scan_store_results(get_global_unix_time());
	}

	lr_wifi_scan_done = true;
}
#endif /* CONFIG_LR_WIFI_SCAN */

#ifdef CONFIG_LR_S_BAND
/**
 * @brief External handler for LR S Band TX done event.
 *
 * @param[in] err - LR S Band TX error code
 */
static void prv_handle_lr_s_band_tx_done(int err)
{
	if (err) {
		LOG_ERR("LR S Band send failed!");
	} else {
		LOG_INF("LR S Band send successful!");
	}

	lr_s_band_tx_done = true;
}
#endif /* CONFIG_LR_S_BAND */

/*
===============================================================================
Private functions - Ublox GPS
===============================================================================
*/

/**
 * @brief Obtain new position data from Ublox module.
 *
 * @return int
 */
static int prv_obtain_ublox_fix(void)
{
	/* Suspend smtc engine */
	lorawan_suspend();

	while (lorawan_is_enabled()) {
		k_sleep(K_MSEC(100));
	}

	int err = 0;
	err = gps_get_fix();
	if (err) {
		LOG_ERR("Ublox module not functional, cannot obtain fix!");
	} else {
		LOG_INF("Did obtain GPS fix.");
	}

	/* Resume smtc engine */
	lorawan_resume();

	return err;
}

/*!
 * @brief Compose standard Ublox GPS message
 * @brief Get ublox GPS position data and store it in the local buffer.
 *
 * @retval 0 - OK, GPS fix attempted
 * @retval -EIO - GPS module disabled
 *
 */
static int prv_get_message_ublox_position(void)
{
	int err = 0;

	/* Header */
	payload[0] = Main_messages.msg_ublox_location->id;
	payload[1] = Main_messages.msg_ublox_location->length;

	/* Fix status */
	bool fix = false;
	uint8_t hot_retry = 0;
	uint8_t cold_retry = 0;
	uint16_t ttf = 0;
	gps_get_last_fix_status(&fix, &hot_retry, &cold_retry, &ttf);

#ifdef CONFIG_OUTDOOR_DETECTION
	payload[2] = fix | (outdoor_detection_get_status() << 1);
#else
	payload[2] = fix;
#endif /* CONFIG_OUTDOOR_DETECTION */

	payload[3] = hot_retry;
	payload[4] = cold_retry;
	memcpy(&payload[5], &ttf, sizeof(ttf));

	/* Fix data */
	struct gps_ublox_position_data position;
	gps_get_last_fix_data(&position);

	/* Latitude */
	memcpy(&payload[7], &position.latitude, sizeof(position.latitude));
	/* Longitude */
	memcpy(&payload[11], &position.longitude, sizeof(position.longitude));
	/* Altitude */
	memcpy(&payload[15], &position.altitude, sizeof(position.altitude));
	/* Fix type */
	payload[19] = position.fix_type;
	/* SIV */
	payload[20] = position.SIV;
	/* Scaled accuracy */
	memcpy(&payload[21], &position.scaled_accuracy, sizeof(position.scaled_accuracy));
	/* PDOP */
	payload[23] = position.PDOP;

	/* Fix time */
	uint32_t time;
	gps_get_last_fix_time(&time);
	memcpy(&payload[24], &time, sizeof(time));

	/* Active tracking */
	payload[28] = gps_get_active_tracking();
	if (gps_get_active_tracking()) {
		memcpy(&payload[29], &position.scaled_cog, sizeof(position.scaled_cog));
		payload[31] = position.scaled_sog;
	}

	port = Main_messages.msg_ublox_location->port;
	payload_size = Main_messages.msg_ublox_location->length + MESSAGE_HEAD_LEN;

	return err;
}

#ifdef CONFIG_OUTDOOR_DETECTION
/**
 * @brief Get empty ublox GPS message and store it in local buffer.
 */
static void prv_get_empty_message_ublox(void)
{
	/* Header */
	payload[0] = Main_messages.msg_ublox_location->id;
	payload[1] = Main_messages.msg_ublox_location->length;

	payload[2] = 0 | (outdoor_detection_get_status() << 1);

	memset(&payload[3], 0, sizeof(uint8_t) * 20);

	/* Fix time */
	uint32_t time = get_global_unix_time();
	memcpy(&payload[24], &time, sizeof(time));

	memset(&payload[28], 0, sizeof(uint8_t) * 4);

	port = Main_messages.msg_ublox_location->port;
	payload_size = Main_messages.msg_ublox_location->length + MESSAGE_HEAD_LEN;
}
#endif /* CONFIG_OUTDOOR_DETECTION */

/**
 * @brief Get ublox GPS satellites data and store it in local buffer.
 *
 * @param[in] max_payload - max message length
 *
 * @return negative integer error code, 0 - OK
 */
static int prv_get_message_ublox_satellites(uint8_t max_payload)
{
	int len = 0;
	len = gps_get_sat_data(payload + 2, max_payload - MESSAGE_HEAD_LEN_BT);
	if (len < 0) {
		return len;
	}

	payload_size = (uint8_t)len;
	payload[0] = Main_messages.msg_ublox_satellites->id;
	payload[1] = payload_size;
	payload_size += MESSAGE_HEAD_LEN;
	port = Main_messages.msg_ublox_satellites->port;

	return 0;
}

/**
 * @brief Compose the ublox short message object into payload
 *
 */
static void prv_get_message_ublox_short(void)
{
	/* payload byte 0 -> msg_id 		*/
	payload[0] = Main_messages.msg_ublox_location_short->id;
	/* payload byte 1 -> msg_len = 14 	*/
	payload[1] = Main_messages.msg_ublox_location_short->length;
	/* payload bytes 2-5 -> ublox_time 	*/
	memcpy(&payload[2], &Main_values.last_position_time->def_val,
	       sizeof(Main_values.last_position_time->def_val));
	/* payload bytes 6-9 -> latitude */
	memcpy(&payload[6], &Main_values.gps_lat->def_val, sizeof(Main_values.gps_lat->def_val));
	/* payload bytes 10-13 -> longitude */
	memcpy(&payload[10], &Main_values.gps_lon->def_val, sizeof(Main_values.gps_lon->def_val));
	/* payload bytes 14-15 -> horizontal accuracy */
	memcpy(&payload[14], &Main_values.gps_h_acc_est->def_val,
	       sizeof(Main_values.gps_h_acc_est->def_val));

	payload_size = Main_messages.msg_ublox_location_short->length + MESSAGE_HEAD_LEN;
}

/**
 * @brief Compose a ublox short message and save into payload
 *
 */
static void prv_get_message_ublox_resend_short(void)
{
	/* payload byte 0 -> msg_id 		*/
	payload[0] = Main_messages.msg_ublox_resend_location->id;
	/* payload byte 1 -> msg_len = 14 	*/
	payload[1] = Main_messages.msg_ublox_resend_location->length;
	/* payload bytes 2-5 -> ublox_time 	*/
	memcpy(&payload[2], &Main_values.last_position_time->def_val,
	       sizeof(Main_values.last_position_time->def_val));
	/* payload bytes 6-9 -> latitude */
	memcpy(&payload[6], &Main_values.gps_lat->def_val, sizeof(Main_values.gps_lat->def_val));
	/* payload bytes 10-13 -> longitude */
	memcpy(&payload[10], &Main_values.gps_lon->def_val, sizeof(Main_values.gps_lon->def_val));
	/* payload bytes 14-15 -> horizontal accuracy */
	memcpy(&payload[14], &Main_values.gps_h_acc_est->def_val,
	       sizeof(Main_values.gps_h_acc_est->def_val));

	payload_size = Main_messages.msg_ublox_resend_location->length + MESSAGE_HEAD_LEN;
}

/**
 * @brief Get ublox GPS data and send it via LR.
 *
 *
 * @return negative integer error code, 0 - OK
 */
static int prv_ublox_gps(void)
{
	int err = 0;

	/* Get new fix - wait until finished */
	err = prv_obtain_ublox_fix();
	if (err) {
		return err;
	}

	/* Send messages based on sending flags */

	/* Check if standard Ublox message is needed */
	port = Main_messages.msg_ublox_location->port;
	if (check_flash_store_flag(port) || check_sat_send_flag(port) || check_lr_send_flag(port) ||
	    check_lp0_send_flag(port)) {

		/* Compose standard Ublox message */
		prv_get_message_ublox_position();

		/* Send and store message */
		prv_send_and_store_payload(port);
	}

	/* Satellite data message */
	port = Main_messages.msg_ublox_satellites->port;
	if (check_flash_store_flag(port) || check_sat_send_flag(port) || check_lr_send_flag(port) ||
	    check_lp0_send_flag(port)) {

		/* Determine max payload size based on flags */
		uint8_t max_len = MESSAGE_LR_MAX_LEN - 1; // Max length, space for port nr.
		if (check_lr_send_flag(port)) {
			max_len = prv_get_max_payload(MB_MSG_LORA);
		}
		if (check_flash_store_flag(port)) {
			uint8_t tmp_max_len = prv_get_max_payload(MB_MSG_FLASH);
			if (tmp_max_len < max_len) {
				max_len = tmp_max_len;
			}
		}

		/* Construct satellite message */
		err = prv_get_message_ublox_satellites(max_len);
		if (err < 0) {
			return err;
		}

		/* Send and store message */
		LOG_INF("Send debug UBLOX satellite data.");
		prv_send_and_store_payload(port);
	}

	/* Short satellite data message */
	port = Main_messages.msg_ublox_location_short->port;
	if (gps_get_last_fix_success() &&
	    (check_flash_store_flag(port) || check_sat_send_flag(port) ||
	     check_lr_send_flag(port) || check_lp0_send_flag(port))) {
		/* compose short message into payload */
		prv_get_message_ublox_short();
		LOG_INF("Send short UBLOX position.");
		/* Send and store message */
		prv_send_and_store_payload(port);
	}

	/* If successful fix was obtained and send, reset resend timer */
	if (gps_get_last_fix_success()) {
		last_gps_resend = k_uptime_get();
	}

	return err;
}

/**
 * @brief Resend latest GPS position over LoRaWAN / flash / satellite.
 *
 */
static void prv_ublox_resend_gps(void)
{
	/* Short satellite location resend data message */
	port = Main_messages.msg_ublox_resend_location->port;

	/* compose short resend message into payload */
	prv_get_message_ublox_resend_short();

	if (check_lr_send_flag(port)) {
		prv_lr_send(payload_size, payload, port, check_lr_confirm_flag(port),
			    check_lr_join_flag(port), false);
	}
	if (check_lp0_send_flag(port)) {
		/* Add event to LP0 que */
		lp0_add_message_to_send_queue(payload, payload_size, port, false, false);
	}
}

/*
===============================================================================
Private functions - LR S-Band send
===============================================================================
*/

#ifdef CONFIG_LR_S_BAND
static int prv_s_band_send(void)
{
	if (!lorawan_is_enabled()) {
		LOG_ERR("LR module disabled!");
		return -EIO;
	}

	if (lorawan_joining_status()) {
		LOG_WRN("Device is in the middle of joining process. Postpone S-band send.");
		return -EBUSY;
	}

	/* Disconnect BT if connected */
	uint8_t tmp_msg[2];
	tmp_msg[0] = CMD_BT_DISCONNECT;
	tmp_msg[1] = 0;
	thread_put_message(MB_MSG_LORA, MB_MSG_BT, MB_MSG_EXECUTE, PORT_COMMANDS, tmp_msg,
			   sizeof(tmp_msg), 0);

	/* Wait for disconnect */
	k_sleep(K_MSEC(thread_sleep));

	/* Send status message */
	/* status port */
	port = Main_messages.msg_status->port;
	/* msg header */
	payload[0] = Main_messages.msg_status->id;
	/* get status */
	payload_size = status_get_message(payload + MESSAGE_HEAD_LEN,
					  prv_get_max_payload(MB_MSG_LORA) - MESSAGE_HEAD_LEN);

	lr_s_band_tx_done = false;

	/* Request sending */
	LOG_INF("Send LR S Band status message.");
	lorawan_s_band_send(payload, payload_size, port, Main_settings.lp0_network_key->def_val,
			    Main_settings.lp0_app_key->def_val,
			    Main_settings.lp0_dev_addr->def_val);

	/* Wait for send to be done */
	LOG_INF("Wait for s-band send to be completed.");
	while (!lr_s_band_tx_done) {
		k_sleep(K_MSEC(100));
	}

	/* Send position message */

	/* short location port */
	port = Main_messages.msg_ublox_location_short->port;
	/* compose short message into payload */
	prv_get_message_ublox_short();

	lr_s_band_tx_done = false;

	/* Request sending */
	lorawan_s_band_send(payload, payload_size, port, Main_settings.lp0_network_key->def_val,
			    Main_settings.lp0_app_key->def_val,
			    Main_settings.lp0_dev_addr->def_val);

	/* Wait for send to be done */
	while (!lr_s_band_tx_done) {
		k_sleep(K_MSEC(100));
	}

	return 0;
}
#endif /* CONFIG_LR_S_BAND */

/*
===============================================================================
Private functions - RF scan, external module
===============================================================================
*/

/**
 * @brief Perform VHF burst.
 */
static void prv_lr_vhf_burst(void)
{
	lorawan_vhf_burst();
}

/*
===============================================================================
Private functions - fence, external module
===============================================================================
*/

#ifdef CONFIG_FENCE_PORT
/**
 * @brief Check if fence functionality is supported and and enabled for the device. Take fence
 * measurement and form message with results data. Based on flags: store to flash and/or send to
 * satellite and/or send via lora.
 *
 * @retval 0 if ok
 * @retval -ENOTSUP not supported
 * @retval -EIO - fence power on/off failed
 * @retval int adc_read() error.
 */
static int prv_fence_measure_and_send(void)
{
	int err = 0;
	payload[0] = Main_messages.msg_fence->id;
	port = Main_messages.msg_fence->port;

	/* Check if module is enabled */
	if (Main_settings.fence_enabled->def_val) {

		err = fence_measure(Main_settings.fence_sampling_length->def_val,
				    Main_settings.fence_mv_scaling_factor->def_val,
				    payload + MESSAGE_HEAD_LEN, &payload[1]);

		/* Calculate total message length */
		payload_size = MESSAGE_HEAD_LEN + payload[1];

		/* Send and store message */
		prv_send_and_store_payload(port);
	} else {
		LOG_WRN("Fence module not enabled, message will not be sent!");
	}

	return err;
}

/**
 * @brief Send external switch detection message.
 *
 * @param active True if switch is active, false otherwise.
 * @param duration_ms Duration in milliseconds.
 * @param force_send If true, send report immediately (except if minimal duration is set and not
 * met).
 *
 * @retval 0 if ok
 * @retval -ENOTSUP not supported
 */
static int prv_external_switch_send_activity(enum external_switch_state active,
					     uint32_t duration_ms, bool force_send)
{
	int err = 0;
	if (Main_settings.external_switch_minimal_report_duration_ms->def_val > 0 &&
	    duration_ms < Main_settings.external_switch_minimal_report_duration_ms->def_val) {
		/* Do not send report if minimal duration is not met, even if force_send is true */
		return err;
	}

	if (force_send) {
		/* Immediate state change message */
		port = Main_messages.msg_external_switch_detection->port;
		payload_size =
			Main_messages.msg_external_switch_detection->length + MESSAGE_HEAD_LEN;

		payload[0] = Main_messages.msg_external_switch_detection_status->id;
		payload[1] = Main_messages.msg_external_switch_detection_status->length;

		if (active == EXTERNAL_SWITCH_ACTIVE) {
			/* Switch state went to ACTIVE */
			payload[2] = 1;
			prv_external_switch_number_of_active_detections++;
		} else {
			/* Switch state went to INACTIVE */
			payload[2] = 0;
		}
		memcpy(&payload[3], &duration_ms, sizeof(duration_ms));

		/* Send and store message - We're not using the normal prv_send_and_store_payload()
		 * here because we want to send the message immediately (satellite specific
		 * limitation) */
		if (check_flash_store_flag(port)) {
			thread_put_message(MB_MSG_LORA, MB_MSG_FLASH, MB_MSG_STORE, port, payload,
					   payload_size, 0);
		}
		if (check_lr_send_flag(port)) {
			prv_lr_send(payload_size, payload, port, check_lr_confirm_flag(port),
				    check_lr_join_flag(port), false);
			LOG_INF("External switch state change message added to send queue! active: "
				"%d, "
				"Duration: %d ms",
				active, duration_ms);
		}
		if (check_sat_send_flag(port)) {
			/* Add message to buffer */
			thread_put_message(MB_MSG_LORA, MB_MSG_SAT, MB_MSG_SEND, port, payload,
					   payload_size, 0);
			/* Immediately send message */
			payload[0] = CMD_SEND_SAT_BUFFER;
			payload[1] = 0;
			thread_put_message(MB_MSG_DEV, MB_MSG_SAT, MB_MSG_EXECUTE, PORT_COMMANDS,
					   payload, MESSAGE_HEAD_LEN, 0);
			last_satellite_send = k_uptime_get();
			LOG_INF("External switch state change is being sent over satellite! "
				"active: %d, Duration: %d ms",
				active, duration_ms);
		}
		if (check_lp0_send_flag(port)) {
			/* Add event to LP0 que */
			lp0_add_message_to_send_queue(payload, payload_size, port, false, false);
		}
	} else {
		/* Status message */
		port = Main_messages.msg_external_switch_detection_status->port;
		payload_size = Main_messages.msg_external_switch_detection_status->length +
			       MESSAGE_HEAD_LEN;

		payload[0] = Main_messages.msg_external_switch_detection_status->id;
		payload[1] = Main_messages.msg_external_switch_detection_status->length;

		if (active == EXTERNAL_SWITCH_ACTIVE) {
			payload[2] = 1;
		} else {
			payload[2] = 0;
		}
		memcpy(&payload[3], &prv_external_switch_number_of_active_detections,
		       sizeof(prv_external_switch_number_of_active_detections));
		prv_external_switch_number_of_active_detections = 0;
		/* Only send if switch state is active or inactivity report is enabled */
		if (active == EXTERNAL_SWITCH_ACTIVE ||
		    Main_settings.external_switch_send_inactivity_report->def_val) {
			prv_send_and_store_payload(port);
			LOG_INF("External switch status report added to send queue!");
		}
	}

	/* Update last reported state */
	external_switch_last_report_set(active);

	return err;
}

static int prv_external_switch_send_impulse_count(uint32_t impulse_count)
{
	int err = 0;
	port = Main_messages.msg_external_switch_detection_status->port;
	payload_size =
		Main_messages.msg_external_switch_detection_status->length + MESSAGE_HEAD_LEN;

	payload[0] = Main_messages.msg_external_switch_detection_status->id;
	payload[1] = Main_messages.msg_external_switch_detection_status->length;

	/* We're reusing the same message as normal external switch reports. This signals to ttn
	 * decoder that we're sending impulse count */
	payload[2] = 2;

	memcpy(&payload[3], &impulse_count, sizeof(impulse_count));

	/* Send and store message */
	prv_send_and_store_payload(port);

	return err;
}
#endif /* CONFIG_FENCE_PORT */

/* =======================================================================================================
 */
/* PUBLIC FUNCTIONS */
/* =======================================================================================================
 */

void handle_communication_thread_messages(void)
{
	int err = 0;
	mb_msg_dest msg_origin = 0;
	mb_msg_action msg_action = 0;
	uint8_t msg_max_rsp_len = 0;

	/* Check if new message */
	int msg_size = thread_get_lr(&msg_origin, &msg_action, &port, payload, &msg_max_rsp_len);
	/* Process multiple messages */
	while (msg_size > 0) {
		uint8_t id, len;
		LOG_INF("Got message in lR thread from: %d, length: %d!", msg_origin, msg_size);

		/* Send action message */
		if (msg_action == MB_MSG_SEND) {
			/* Send lora message */
			err = prv_lr_send((uint8_t)msg_size, payload, port,
					  check_lr_confirm_flag(port), check_lr_join_flag(port),
					  false);

			/* Check if message was received from flash module. Return new max response
			 * size */
			if (port == Main_messages.msg_read_flash->port) {
				msg_max_rsp_len = prv_get_max_payload(MB_MSG_LORA);
				payload[0] = Main_messages.msg_read_flash->id;
				msg_size = compose_response_msg(payload, err, &port);
				LOG_INF("Got message from flash. Return rsp with max payload: %d",
					msg_max_rsp_len);
				thread_put_message(MB_MSG_LORA, msg_origin, MB_MSG_EXECUTE, port,
						   payload, msg_size, msg_max_rsp_len);
			}
		}
		/* Execute action */
		else if (msg_action == MB_MSG_EXECUTE) {

			/* Get cmd id */
			id = payload[0];
			len = payload[1];
			msg_size = 0;

			switch (id) {
			case CMD_RESET_GPS: {
				err = gps_reset();
				payload[0] = CMD_RESET_GPS;
				msg_size = compose_response_msg(payload, err, &port);
				break;
			}
			case CMD_RESET_LR: {
				lr_reset();
				payload[0] = CMD_RESET_LR;
				msg_size = compose_response_msg(payload, 0, &port);
				break;
			}
			case CMD_JOIN: {
				err = prv_lr_join(true);
				payload[0] = CMD_JOIN;
				msg_size = compose_response_msg(payload, err, &port);
				break;
			}
			case CMD_SEND_LR_FIX: {
				if (!prv_lr_gnss_scan()) {
					/* Check if we need to send NAV message */
					port = Main_messages.msg_gnss->port;
					if (check_flash_store_flag(port) ||
					    check_sat_send_flag(port) || check_lr_send_flag(port) ||
					    check_lp0_send_flag(port)) {
						/* Compose nav message */
						prv_compose_message_lr_gnss();
						/* Send and store nav message */
						prv_send_and_store_payload(port);
					}

					/* Check if we need to send sat data message */
					port = Main_messages.msg_lr_satellites->port;
					if (check_flash_store_flag(port) ||
					    check_sat_send_flag(port) || check_lr_send_flag(port) ||
					    check_lp0_send_flag(port)) {
						/* Compose sat data message */
						prv_compose_message_lr_satellites();
						/* Send and store sat message */
						prv_send_and_store_payload(port);
					}
				}
				break;
			}
			case CMD_GET_WIFI_SCAN: {
#ifdef CONFIG_LR_WIFI_SCAN
				/* Perform new wifi scan */
				err = prv_lr_wifi_scan();
				if (!err) {
					prv_compose_message_lr_wifi_single_scan(
						prv_get_max_payload(msg_origin));
					msg_size = payload_size;
				}
#else

				err = -EIO;
#endif /* CONFIG_LR_WIFI_SCAN */

				if (err) {
					msg_size = compose_response_msg(payload, err, &port);
				}
				break;
			}
			case CMD_S_BAND_SEND: {
#ifdef CONFIG_LR_S_BAND
				/* Perform s-band send */
				err = prv_s_band_send();
#else
				err = -EIO;
#endif /* CONFIG_LR_S_BAND */
				msg_size = compose_response_msg(payload, err, &port);
				break;
			}
			case CMD_GET_LR_SATELLITE_DATA: {
				if (!prv_lr_gnss_scan()) {
					/* Compose sat data message
					 */
					prv_compose_message_lr_satellites();
				}
				msg_size = payload_size;
				port = Main_messages.msg_lr_satellites->port;
				break;
			}
			case CMD_GET_UBLOX_FIX: {
				/* If GPS is not enabled, reset module */
				if (!gps_get_enabled()) {
					gps_reset();
				}
				// Get new fix - wait until new fix or error on th try is received
				err = prv_obtain_ublox_fix();
				port = Main_messages.msg_ublox_location->port;
				if (err) {
					msg_size = 0;
				} else {
					// Compose standard message
					prv_get_message_ublox_position();
					msg_size = payload_size;
				}
				break;
			}
			case CMD_GET_UBLOX_SATELLITE_DATA: {
				// Check if gps is enabled
				bool gps_ok = gps_get_enabled();
				// If not reset module
				if (!gps_ok) {
					gps_reset();
				}
				// Get new fix - wait until new fix or error on th try is received
				err = prv_obtain_ublox_fix();
				if (err) {
					msg_size = 0;
					port = Main_messages.msg_ublox_satellites->port;
				} else {
					prv_get_message_ublox_satellites(msg_max_rsp_len);
				}
				msg_size = payload_size;
				port = Main_messages.msg_ublox_satellites->port;
				break;
			}
			case CMD_ALMANAC_UPDATE: {
				/* Start at first data chunk, omitting header */
				int idx = MESSAGE_HEAD_LEN;
				LOG_INF("Update Almanac command!");
				/* Loop over almanacs */
				while (idx < len) {
					/* Get sat id and determine
					 * position in almanac */
					uint8_t sat_idx = payload[idx];
					LOG_INF("Got data for "
						"satellite: %d",
						sat_idx);
					if (sat_idx < 128) {
						sat_idx++;
					} else {
						/* Header */
						sat_idx = 0;
					}

					int ret = almanac_replace_single_satellite_data(
						payload + idx, sat_idx);
					if (ret < 0) {
						LOG_ERR("Almanac "
							"update "
							"failed!");
						err = ret;
						break;
					}
					idx += ret;
					/* If header was sent,
					 * update whole almanac */
					if (sat_idx == 0) {
						LOG_INF("Update "
							"Almanac!");
						err = prv_lr_update_almanac();
					}
				}
				msg_size = compose_response_msg(payload, err, &port);
				break;
			}
			case CMD_SET_OPERATION_MODE_COM_TH: {
				LOG_INF("Com thread set operation mode: %d", payload[2]);
				if (payload[2] == THREAD_DISABLED) {
					prv_disable_communication_thread();
				} else if (payload[2] == THREAD_LOW_POWER) {
					prv_low_power_communication_thread();
				} else if (payload[2] == THREAD_NORMAL) {
					prv_enable_communication_thread();
				} else {
					err = -EIO;
				}

				payload[0] = CMD_SET_OPERATION_MODE_COM_TH;
				msg_size = compose_response_msg(payload, err, &port);
				break;
			}
			case CMD_READ_ALL_LR_MESSAGES: {
				LOG_INF("Read all incoming LR messages!");
				/* If not from BT module, do nothing */
				if (msg_origin != MB_MSG_BT) {
					err = -EIO;
					break;
				}

				/* get nr. of incoming messages in the buffer */
				uint8_t n = lr_messaging_get_nr_incoming_msg();
				LOG_INF("Nr. of messages to read: %d", n);
				if (n > 0) {
					for (uint8_t i = 0; i < n; i++) {
						msg_size = lr_messaging_read_incoming_msg(payload);
						if (msg_size > 0) {
							/* Send
							 * message
							 * to BT
							 * thread */
							err = thread_put_message(
								MB_MSG_DEV, MB_MSG_BT, MB_MSG_SEND,
								PORT_LR_MESSAGING, payload,
								msg_size, 0);
							if (err) {
								break;
							}
						}
					}
				} else {
					err = -ENODATA;
				}

				payload[0] = CMD_READ_ALL_LR_MESSAGES;
				msg_size = compose_response_msg(payload, err, &port);
				Main_values.n_mes->def_val = 0;

				break;
			}
#ifdef CONFIG_FENCE_PORT
			case CMD_FENCE_MEASURE: {

				/* Check if module is enabled */
				if (Main_settings.fence_enabled->def_val) {

					payload[0] = Main_messages.msg_fence->id;
					port = Main_messages.msg_fence->port;
					fence_measure(
						Main_settings.fence_sampling_length->def_val,
						Main_settings.fence_mv_scaling_factor->def_val,
						payload + MESSAGE_HEAD_LEN, &payload[1]);

					/* Calculate total message
					 * length */
					msg_size = MESSAGE_HEAD_LEN + payload[1];

				} else {
					payload[0] = CMD_FENCE_MEASURE;
					msg_size = compose_response_msg(payload, -EIO, &port);
				}

				break;
			}
#endif /* CONFIG_FENCE_PORT */
			default: {
				/* All relevant settings actions */
			}
			}

			/* Check if we need to send response */
			if (msg_size > 0) {
				/* Send via BT */
				if (msg_origin == MB_MSG_BT) {
					LOG_INF("Put rsp of size: "
						"%d to BLE thread",
						msg_size);
					thread_put_message(MB_MSG_DEV, MB_MSG_BT, MB_MSG_SEND, port,
							   payload, msg_size, msg_max_rsp_len);
				}
				/* Send via LR */
				else if (msg_origin == MB_MSG_LORA) {
					prv_lr_send((uint8_t)msg_size, payload, port,
						    check_lr_confirm_flag(port),
						    check_lr_join_flag(port), false);
				}
			}
		}
		/* LR messaging */
		else if (msg_action == MB_MSG_STORE) {
			id = payload[0];
			len = payload[1];
			/* Incoming msg from Lora server */
			if (id == MSG_LR_MESSAGING_ID) {
				LOG_INF("Store incoming Lora msg!");
				err = lr_messaging_store_incoming(payload, (uint8_t)msg_size);
				Main_values.n_mes->def_val = lr_messaging_get_nr_incoming_msg();
			}
			/* Outgoing message from BT app */
			else if (id == CMD_SEND_LR_MESSAGE) {
				LOG_INF("Store outgoing LR message!");
				err = lr_messaging_store_outgoing(payload + 2, len);
			} else {
				LOG_ERR("Invalid message type to store in incoming LR buffer!");
			}
		} else {
			LOG_ERR("Invalid message action!");
		}

		/* Check if we have more messages */
		msg_size =
			thread_get_lr(&msg_origin, &msg_action, &port, payload, &msg_max_rsp_len);
	}
}

void handle_lr_join(void)
{
	if (prv_check_lr_join_interval()) {
		prv_lr_join(false);
		last_rejoin_attempt = k_uptime_get();
	}
}

void handle_lr_gps(void)
{
	if (prv_check_lr_gps_interval()) {
		int err = prv_lr_gnss_scan();
		if (err != -EBUSY) {
			last_lr_gps_send = k_uptime_get();
		}

		if (!err) {
			/* Check if we need to send NAV message */
			port = Main_messages.msg_gnss->port;
			if (check_flash_store_flag(port) || check_sat_send_flag(port) ||
			    check_lr_send_flag(port) || check_lp0_send_flag(port)) {
				/* Compose nav message */
				prv_compose_message_lr_gnss();
				/* Send and store nav message */
				prv_send_and_store_payload(port);
			}

			/* Check if we need to send sat data message */
			port = Main_messages.msg_lr_satellites->port;
			if (check_flash_store_flag(port) || check_sat_send_flag(port) ||
			    check_lr_send_flag(port) || check_lp0_send_flag(port)) {
				/* Compose sat data message */
				prv_compose_message_lr_satellites();
				/* Send and store sat message */
				prv_send_and_store_payload(port);
			}
		}
	}
}

void handle_send_status(void)
{
	if (prv_check_send_status_interval()) {
		prv_send_status();
		last_status_send = k_uptime_get();
	}
}

void handle_wifi_scan(void)
{
	if (prv_check_scan_wifi_interval()) {
#ifdef CONFIG_LR_WIFI_SCAN
		int err = prv_lr_wifi_scan();
		if (err != -EBUSY) {
			last_wifi_scan = k_uptime_get();
		}
		if (!err) {
			if (check_flash_store_flag(port)) {
				prv_compose_message_lr_wifi_single_scan(
					prv_get_max_payload(MB_MSG_FLASH));
				thread_put_message(MB_MSG_LORA, MB_MSG_FLASH, MB_MSG_STORE, port,
						   payload, payload_size, 0);
			}
			if (check_sat_send_flag(port)) {
				prv_compose_message_lr_wifi_single_scan(
					prv_get_max_payload(MB_MSG_SAT));
				thread_put_message(MB_MSG_LORA, MB_MSG_SAT, MB_MSG_SEND, port,
						   payload, payload_size, 0);
			}
			if (check_lr_send_flag(port)) {
				prv_compose_message_lr_wifi_single_scan(
					prv_get_max_payload(MB_MSG_LORA));
				prv_lr_send((uint8_t)payload_size, payload, port,
					    check_lr_confirm_flag(port), check_lr_join_flag(port),
					    false);
			}
			if (check_lp0_send_flag(port)) {
				/* Add event to LP0 que */
				prv_compose_message_lr_wifi_single_scan(
					prv_get_max_payload(MB_MSG_LORA));
				lp0_add_message_to_send_queue(payload, payload_size, port, false,
							      false);
			}
		}
#endif /* CONFIG_LR_WIFI_SCAN */
	}
}

void handle_wifi_scan_aggregated(void)
{
	if (prv_check_wifi_scan_aggregated_interval()) {
#ifdef CONFIG_LR_WIFI_SCAN
		int err = 0;
		port = Main_messages.msg_wifi_scan_aggregated->port;
		if (check_flash_store_flag(port)) {
			/* Check if we need to clear aggregated data */
			bool clear_flag = true;
			if (check_lr_send_flag(port) || check_sat_send_flag(port) ||
			    check_lp0_send_flag(port)) {
				clear_flag = false;
			}
			/* Get payload for flash storage */
			prv_compose_message_lr_wifi_scan_aggregated(
				prv_get_max_payload(MB_MSG_FLASH), WIFI_SCAN_STORE_MAX_RES,
				clear_flag);
			thread_put_message(MB_MSG_LORA, MB_MSG_FLASH, MB_MSG_STORE, port, payload,
					   payload_size, 0);
		}
		if (check_lr_send_flag(port) || check_sat_send_flag(port) ||
		    check_lp0_send_flag(port)) {
			/* Get payload for sending */
			prv_compose_message_lr_wifi_scan_aggregated(
				prv_get_max_payload(MB_MSG_LORA), WIFI_SCAN_SEND_MAX_RES, true);
			if (check_sat_send_flag(port)) {
				thread_put_message(MB_MSG_LORA, MB_MSG_SAT, MB_MSG_SEND, port,
						   payload, payload_size, 0);
			}
			if (check_lr_send_flag(port)) {
				err = prv_lr_send((uint8_t)payload_size, payload, port,
						  check_lr_confirm_flag(port),
						  check_lr_join_flag(port), false);
			}
			if (check_lp0_send_flag(port)) {
				/* Add event to LP0 que */
				lp0_add_message_to_send_queue(payload, payload_size, port, false,
							      false);
			}
		}
		last_wifi_scan_send = k_uptime_get();
#endif /* CONFIG_LR_WIFI_SCAN */
	}
}

void handle_bt_scan()
{
	if (prv_check_bt_scan_period()) {
		/* Send message to BT module */
		payload[0] = CMD_SINGLE_BT_SCAN;
		payload[1] = 0;
		thread_put_message(MB_MSG_DEV, MB_MSG_BT, MB_MSG_EXECUTE, PORT_COMMANDS, payload, 2,
				   prv_get_max_payload(MB_MSG_LORA));

		last_bt_scan = k_uptime_get();
	}
}

void handle_bt_scan_aggregated()
{
	if (prv_check_bt_scan_aggregated_period()) {
		/* Send message to BT module */
		payload[0] = CMD_AGGREGATED_BT_SCAN;
		payload[1] = 0;
		thread_put_message(MB_MSG_DEV, MB_MSG_BT, MB_MSG_EXECUTE, PORT_COMMANDS, payload, 2,
				   prv_get_max_payload(MB_MSG_LORA));

		last_bt_scan_send = k_uptime_get();
	}
}

void handle_bt_cmdq(void)
{
#ifdef CONFIG_BT_CMDQ
	if (k_uptime_get() / 1000 < CMDQ_TIME_BEFORE_FIRST_SCAN_S) {
		return;
	}
	/* Check if we need to send report messages */
	if (prv_check_bt_cmdq_send_interval() && Main_settings.cmdq_enabled->def_val) {
		payload[0] = CMD_SEND_BT_CMDQ_RESULTS;
		payload[1] = 0;
		thread_put_message(MB_MSG_DEV, MB_MSG_BT, MB_MSG_EXECUTE, PORT_COMMANDS, payload, 2,
				   prv_get_max_payload(MB_MSG_LORA));

		last_cmdq_message_sent = k_uptime_get();
	}

	/* Check if the setting has changed and start/stop scanning. */
	if (Main_settings.cmdq_enabled->def_val == bt_cmdq_is_operation_started()) {
		return;
	}
	/* Start operation */
	if (Main_settings.cmdq_enabled->def_val) {
		int err = bt_cmdq_start_operation(bt_cmdq_messaging_save_to_buffer);
		if (err) {
			LOG_ERR("Failed to start BT_CMDQ operation (err: %d)", err);
		}
	} else {
		/* Stop operation */
		bt_cmdq_stop_operation();
	}
#endif /* CONFIG_BT_CMDQ */
}

void handle_ublox_gps(void)
{
#ifdef CONFIG_OUTDOOR_DETECTION
	if (Main_settings.outdoor_detection_enabled->def_val &&
	    outdoor_detection_get_fix_interval_elapsed()) {
		if (outdoor_detection_get_status()) {
			/* Perform GPS fix and send data */
			prv_ublox_gps();
			outdoor_detection_clear_status();
		} else {
			/* If outdoors were not detected send empty message */
			port = Main_messages.msg_ublox_location->port;
			if (check_flash_store_flag(port) || check_sat_send_flag(port) ||
			    check_lr_send_flag(port) || check_lp0_send_flag(port)) {

				prv_get_empty_message_ublox();

				/* Send and store message */
				prv_send_and_store_payload(port);
			}
		}
		outdoor_detection_clear_fix_interval_elapsed();
		return;
	}
#endif /* CONFIG_OUTDOOR_DETECTION */

#ifdef CONFIG_OUTDOOR_DETECTION
	if (!Main_settings.outdoor_detection_enabled->def_val && gps_send_interval()) {
#else
	if (gps_send_interval()) {
#endif /* CONFIG_OUTDOOR_DETECTION */
		prv_ublox_gps();
		gps_update_last_fix_time();
	}
}

void handle_resend_ublox_gps()
{
	if (prv_check_gps_resend_interval()) {
		/* Resend */
		prv_ublox_resend_gps();
		/* Uptime */
		last_gps_resend = k_uptime_get();
	}
}

void handle_lr_messaging(void)
{
	if (prv_check_send_lr_messages_interval()) {
		prv_lr_send_messages();
		last_lr_messages_send = k_uptime_get();
	}
}

void handle_s_band_send(void)
{
	if (prv_check_send_s_band_interval()) {
#ifdef CONFIG_LR_S_BAND
		int err = prv_s_band_send();
		if (err != -EBUSY) {
			last_s_band_send = k_uptime_get();
		}
#endif /* CONFIG_LR_S_BAND */
	}
}

void handle_satellite_send(void)
{
	if (prv_check_send_satellite_interval()) {
		// Send command to satellite thread
		payload[0] = CMD_SEND_SAT_BUFFER;
		payload[1] = 0;
		thread_put_message(MB_MSG_DEV, MB_MSG_SAT, MB_MSG_EXECUTE, PORT_COMMANDS, payload,
				   MESSAGE_HEAD_LEN, 0);
		last_satellite_send = k_uptime_get();
	}
}

void handle_vhf_burst(void)
{
	/* Check if feature enabled LoRaWAN modem currently in use */
	if (Main_settings.vhf_enabled->def_val && !lorawan_get_vhf_burst_queued()) {
		if (prv_check_vhf_interval()) {
			last_vhf_burst = k_uptime_get();
			prv_lr_vhf_burst();
		}
	}
}

void handle_fence(void)
{
#ifdef CONFIG_FENCE_PORT
	if (prv_check_fence_interval()) {
		prv_fence_measure_and_send();
		last_fence_measurement = k_uptime_get();
	}
#endif /* CONFIG_FENCE_PORT */
}

void handle_external_switch(void)
{
#ifdef CONFIG_FENCE_PORT
	/* Check if external switch detection is enabled or delay is still active */
	if (k_uptime_get() / 1000 < EXTERNAL_SWITCH_DETECTION_ON_BOOT_DELAY ||
	    !Main_settings.external_switch_detection_enabled->def_val) {
		return;
	}

	/* Check external switch pin configuration and re-initialize if needed */
	if (external_switch_check_pin_configuration() < 0) {
		LOG_ERR("External switch GPIO pin configuration check error!");
		return;
	}

	bool send_report = false, force_send = false;
	int err = external_switch_send_report_check(&send_report, &force_send);

	if (send_report) {
		if (prv_check_external_switch_interval() || force_send) {
			if (Main_settings.external_switch_counter_enabled->def_val) {
				uint32_t impulse_count = external_switch_get_impulse_count();
				/* Check if we send inactivity report */
				if (!Main_settings.external_switch_send_inactivity_report
					     ->def_val &&
				    impulse_count == 0) {
					return;
				}
				err = prv_external_switch_send_impulse_count(impulse_count);
				if (err) {
					LOG_ERR("Failed to send external switch impulse count: %d",
						err);
					return;
				}
			} else {
				/* Check if external switch is active */
				enum external_switch_state active = EXTERNAL_SWITCH_INACTIVE;
				uint32_t duration_ms = 0;
				err = external_switch_active(&active, &duration_ms);
				if (err) {
					LOG_ERR("Failed to check external switch status: %d", err);
					return;
				}
				err = prv_external_switch_send_activity(active, duration_ms,
									force_send);
				if (err) {
					LOG_ERR("Failed to send external switch status: %d", err);
					return;
				}
			}
		}
	}
#endif /* CONFIG_FENCE_PORT */
}

void handle_air_quality(void)
{
#ifdef CONFIG_AIR_QUALITY
	if (prv_check_air_quality_interval()) {
		port = Main_messages.msg_air_quality->port;
		payload[0] = Main_messages.msg_air_quality->id;

		size_t payload_size_counter = MESSAGE_HEAD_LEN;

		/* Get pointers to last locally kept air quality data */
		bmv080_output_t *prv_bmv080_latest_output_ptr = NULL;
		bsec_output_t *prv_bme690_latest_outputs_ptr = NULL;

		air_quality_get_data_pointers(&prv_bmv080_latest_output_ptr,
					      &prv_bme690_latest_outputs_ptr);

		if (prv_bmv080_latest_output_ptr != NULL) {
			/* Compose message */
			size_t bmv080_data_len = sizeof(float) * 6 + sizeof(bool);
			memcpy(&payload[payload_size_counter],
			       &prv_bmv080_latest_output_ptr->pm2_5_mass_concentration,
			       bmv080_data_len);
			payload_size_counter += bmv080_data_len;
		}

		if (prv_bme690_latest_outputs_ptr != NULL) {
			int bme690_data_counter = payload_size_counter;

			for (int i = 0; i < 8; ++i) {
				if (prv_bme690_latest_outputs_ptr[i].sensor_id == BSEC_OUTPUT_IAQ) {
					/* IAQ */
					memcpy(&payload[bme690_data_counter],
					       &prv_bme690_latest_outputs_ptr[i].signal,
					       sizeof(float));
					payload_size_counter += sizeof(float);
				} else if (prv_bme690_latest_outputs_ptr[i].sensor_id ==
					   BSEC_OUTPUT_RAW_TEMPERATURE) {
					/* Raw temperature */
					memcpy(&payload[bme690_data_counter + sizeof(float)],
					       &prv_bme690_latest_outputs_ptr[i].signal,
					       sizeof(float));
					payload_size_counter += sizeof(float);
				} else if (prv_bme690_latest_outputs_ptr[i].sensor_id ==
					   BSEC_OUTPUT_RAW_PRESSURE) {
					/* Raw pressure */
					memcpy(&payload[bme690_data_counter + sizeof(float) * 2],
					       &prv_bme690_latest_outputs_ptr[i].signal,
					       sizeof(float));
					payload_size_counter += sizeof(float);
				} else if (prv_bme690_latest_outputs_ptr[i].sensor_id ==
					   BSEC_OUTPUT_RAW_HUMIDITY) {
					/* Raw humidity */
					memcpy(&payload[bme690_data_counter + sizeof(float) * 3],
					       &prv_bme690_latest_outputs_ptr[i].signal,
					       sizeof(float));
					payload_size_counter += sizeof(float);
				} else if (prv_bme690_latest_outputs_ptr[i].sensor_id ==
					   BSEC_OUTPUT_RAW_GAS) {
					/* Raw gas */
					memcpy(&payload[bme690_data_counter + sizeof(float) * 4],
					       &prv_bme690_latest_outputs_ptr[i].signal,
					       sizeof(float));
					payload_size_counter += sizeof(float);
				}
			}
		}

		payload_size = payload_size_counter;
		payload[1] = payload_size_counter;

		/* Send and store message */
		prv_send_and_store_payload(port);
	}
#endif /* CONFIG_AIR_QUALITY */
}

int init_gps_module(void)
{
	int err = -EIO;
	err = gps_init();
	if (err == -ENXIO) {
		LOG_WRN("GPS module not supported!");
	} else if (err) {
		LOG_ERR("Failed to init GPS");
	}
	gps_power(0); // Power down GPS

	sys_err.ublox = err;

	return err;
}

void lr_start(void)
{
	/* register response handler */
	LOG_INF("Register LoraWAN RX CB");
	lorawan_recv_handler_register(handle_lora_recv);

	LOG_INF("Register GNSS NAV CB");
	gnss_results_handler_register(prv_handle_gnss_done);

	LOG_INF("Register Almanac update CB");
	almanac_update_handler_register(prv_handle_almanac_update);

#ifdef CONFIG_LR_WIFI_SCAN
	LOG_INF("Register WiFi scan CB");
	wifi_scan_results_handler_register(prv_handle_wifi_scan_done);
#endif /* CONFIG_LR_WIFI_SCAN */

#ifdef CONFIG_LR_S_BAND
	LOG_INF("Register S-Band CB");
	lr_s_band_tx_done_handler_register(prv_handle_lr_s_band_tx_done);
#endif /* CONFIG_LR_S_BAND */

	/* Set configuration */
	lorawan_set_configuration(Main_settings.app_eui->def_val, Main_settings.app_key->def_val,
				  Main_settings.lr_region->def_val, Main_settings.lr_adr->def_val,
				  Main_settings.lr_adr_profile->def_val);

	/* start lorawan engine */
	lorawan_start();

	int wait_counter = 0;
	/* Wait for lorawan module to become available */
	while (!lorawan_is_enabled()) {
		k_sleep(K_MSEC(200));
		LOG_WRN("Wait");
		/* if lorawan module is not available after 20s, reboot the device */
		if (wait_counter > 100) {
			sys_reboot(SYS_REBOOT_COLD);
		}
		wait_counter++;
	}

	/* Get device id */
	int err = lorawan_get_dev_eui(Main_settings.device_eui->def_val);
	if (!err) {
		nvs_storage_write(Main_settings.device_eui->id, Main_settings.device_eui->def_val,
				  Main_settings.device_eui->len);
	}

	/* Update Almanac if needed */
	prv_lr_update_almanac();
}

int lr_reset(void)
{
	/* Disable operation */
	lorawan_suspend();

	/* register response handler */
	LOG_INF("Register LoraWAN RX CB");
	lorawan_recv_handler_register(handle_lora_recv);

	LOG_INF("Register GNSS NAV CB");
	gnss_results_handler_register(prv_handle_gnss_done);

	LOG_INF("Register Almanac update CB");
	almanac_update_handler_register(prv_handle_almanac_update);

#ifdef CONFIG_LR_WIFI_SCAN
	LOG_INF("Register WiFi scan CB");
	wifi_scan_results_handler_register(prv_handle_wifi_scan_done);
#endif /* CONFIG_LR_WIFI_SCAN */

	/* Set new configuration */
	lorawan_set_configuration(Main_settings.app_eui->def_val, Main_settings.app_key->def_val,
				  Main_settings.lr_region->def_val, Main_settings.lr_adr->def_val,
				  Main_settings.lr_adr_profile->def_val);

	/* Reset LR */
	lorawan_reset();

	/* Wait for lorawan module to become available */
	int wait_counter = 0;
	while (!lorawan_is_enabled()) {
		k_sleep(K_MSEC(200));
		LOG_WRN("Wait");
		/* if lorawan module is not available after 20s, reboot the device */
		if (wait_counter > 100) {
			sys_reboot(SYS_REBOOT_COLD);
		}
		wait_counter++;
	}

	/* Get device id */
	int err = lorawan_get_dev_eui(Main_settings.device_eui->def_val);
	if (!err) {
		nvs_storage_write(Main_settings.device_eui->id, Main_settings.device_eui->def_val,
				  Main_settings.device_eui->len);
	}

	/* Update Almanac if needed */
	prv_lr_update_almanac();

	return err;
}

/*** end of file ***/
