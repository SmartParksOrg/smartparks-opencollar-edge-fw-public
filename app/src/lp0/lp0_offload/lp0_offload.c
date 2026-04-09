/** @file lp0_offload.c
 *
 * @brief LP0 ping and offloading functions used by LP0. This file serves to declutter LP0 of
 * offloading features and improve readability. It is not meant to be a standalone offloading
 * module and is intended to be tightly coupled with LP0.
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2026 Irnas. All rights reserved.
 */

#include <global_time.h>
#include <led.h>
#include <lorawan.h>
#include <lp0.h>
#include <lp0_communication.h>
#include <messages_def.h>
#ifdef CONFIG_RF_FRONT_END_MODULE
#include <rf_front_end_module.h>
#endif /* CONFIG_RF_FRONT_END_MODULE */
#ifdef CONFIG_SDFS_UART_MODULE
#include "sdfs_uart_handler.h"
#endif /* CONFIG_SDFS_UART_MODULE */
#include <time.h>
#include <settings_def.h>
#include <status.h>
#include <thread_com.h>
#include <thread_watchdog.h>
#include <uart_pm.h>
#include <values_def.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/base64.h>

#include <lp0_offload.h>

LOG_MODULE_REGISTER(lp0_offload);

/* RX windows will start sooner based on this amount (where applicable) and get
 * elongated by this amount (in all cases). If the rx window was started sooner,
 * another elongation of this amount will be applied
 */
#define LP0_LORAWAN_SEM_TIMEOUT_MS                                                                 \
	((int)(THREAD_LR_GPS_MAX_RESPONSE / 2 * 1000)) /* Max semaphore timeout is half of the     \
							Lorawan thread watchdog timeout time */

extern struct k_sem lp0_tx_rx_done_sem;
extern struct k_sem lora_chip_suspended_sem;

/* Flash semaphore */
static uint8_t prv_flash_tx_buf[250];
K_SEM_DEFINE(lp0_flash_save_sem, 1, 1);

static const struct device *prv_lora_context = DEVICE_DT_GET(DT_NODELABEL(lr11xx));

char prv_output_file_name[64] = "output_default";

/**
 * @brief Send discovery ping message as a device that wants to offload data to an offload station
 */
static void prv_lp0_discovery_ping_send(void)
{
	uint8_t payload[Main_messages.msg_lp0_ping->length +
			2]; /* 2 bytes for message ID and payload length */

	/* payload byte 0 -> msg_id 		*/
	payload[0] = 0x03;
	/* payload byte 1 -> msg_len = 14 	*/
	payload[1] = Main_messages.msg_lp0_ping->length;
	/* payload bytes 2-5 -> ublox_time 	*/
	memcpy(&payload[2], &Main_values.last_position_time->def_val,
	       sizeof(Main_values.last_position_time->def_val));
	/* payload bytes 6-9 -> latitude */
	memcpy(&payload[6], &Main_values.gps_lat->def_val, sizeof(Main_values.gps_lat->def_val));
	/* payload bytes 10-13 -> longitude */
	memcpy(&payload[10], &Main_values.gps_lon->def_val, sizeof(Main_values.gps_lon->def_val));

	// LUKATODO: TODO-FUTURE: add count of pending messages
	// memcpy(&payload[14], &pending_message_count, sizeof(pending_message_count));
	// LUKATODO: TODO-FUTURE: Add last confirm msg ID and reverse bitmask of received messages

	lp0_prepare_and_send_message(prv_lora_context, payload, sizeof(payload), 33, false,
				     Main_settings.lp0_tx_frequency_hz->def_val, false,
				     Main_settings.lp0_node_params->def_val[3],
				     LR11XX_RADIO_LORA_IQ_STANDARD);
}

/**
 * @brief Generate output file name for LP0 offload station mode
 *
 * Output file name format: \{device_eui}{unix_time}.jsonl
 *
 * @param output_string - Pointer to output string buffer
 * @param max_output_len - Maximum length of output string buffer
 */
static void prv_lp0_output_file_name(char *output_string, size_t max_output_len)
{
	snprintf(output_string, max_output_len, "\\%02X%02X%02X%02X%02X%02X%02X%02X_%d.jsonl",
		 Main_settings.device_eui->def_val[0], Main_settings.device_eui->def_val[1],
		 Main_settings.device_eui->def_val[2], Main_settings.device_eui->def_val[3],
		 Main_settings.device_eui->def_val[4], Main_settings.device_eui->def_val[5],
		 Main_settings.device_eui->def_val[6], Main_settings.device_eui->def_val[7],
		 get_global_unix_time());
}

/**
 * @brief Start receiving window for device nodes.
 *
 * Stays in RX until ping message is received or timeout occurs (if not set to continuous).
 *
 * @param timeout_ms - timeout for reception in milliseconds
 * @param continuous - if true, reception will be continuous until explicitly stopped, otherwise it
 * will stop after timeout
 *
 * @return int 0 if ok, negative error code if error
 */
static int prv_lp0_device_node_start_receive(uint32_t timeout_ms, bool continuous)
{
	int err = lp0_start_receive(prv_lora_context, Main_settings.lp0_rx_frequency_hz->def_val,
				    LR11XX_RADIO_LORA_IQ_INVERTED, timeout_ms, continuous);
	return err;
}

/**
 * @brief Start receiving window for offload stations (Only difference from normal receive is that
 * it uses Standard IQ).
 *
 * Stays in RX until ping message is received or timeout occurs (if not set to continuous).
 *
 * @param timeout_ms - timeout for reception in milliseconds
 * @param continuous - if true, reception will be continuous until explicitly stopped, otherwise it
 * will stop after timeout
 *
 * @return int 0 if ok, negative error code if error
 */
static int prv_lp0_offload_station_start_receive(uint32_t timeout_ms, bool continuous)
{
	int err = lp0_start_receive(prv_lora_context, Main_settings.lp0_rx_frequency_hz->def_val,
				    LR11XX_RADIO_LORA_IQ_STANDARD, timeout_ms, continuous);
	return err;
}

/**
 * @brief Send discovery ping acknowledge response message from offload station
 *
 * @param received_dev_addr - Device address of the device from which we've received the ping. Array
 * needs to be at least 4 bytes long. Only 4 bytes are used.
 */
static void prv_lp0_offload_station_discovery_ping_response_send(uint8_t *received_dev_addr)
{
	uint8_t payload[10];
	/* 1 for ACK + 1 for length + 4 for id of ping sending device + 4 for last saved msg */

	/* payload byte 0 -> msg_id */
	payload[0] = LP0_PING_ACK;
	/* payload byte 1 -> msg_len = 8 */
	payload[1] = 8;
	/* payload bytes 2-5 -> address of the device from which we've received the ping */

	memcpy(&payload[2], received_dev_addr, Main_settings.lp0_dev_addr->len);
	/* payload bytes 6-9 -> latitude */
	memset(&payload[6], 0x00, 4);
	/* LUKATODO: TODO-FUTURE: add last saved msg ID we're keeping in NVS or flash. */

	/* Send with custom header, so the receiving device can decode */
	lp0_prepare_and_send_message(prv_lora_context, payload, sizeof(payload), 33, false,
				     Main_settings.lp0_tx_frequency_hz->def_val, false, false,
				     LR11XX_RADIO_LORA_IQ_INVERTED);
}

/**
 * @brief Perform full RX procedure as device that will offload data to an offload station
 *
 * Performs RX windows + lorawan RX1 and RX2
 */
static void prv_lp0_device_perform_full_rx(void)
{
	/* Start LP0 RX window(s) + concatenated Lorawan RX1 window */
	prv_lp0_device_node_start_receive(0, true);

	uint32_t rx_timeout = Main_settings.lp0_communication_params->def_val[3] * 1000 +
			      lp0_get_rx_timeout_ms() + 1000;

	LOG_INF("Sleeping (ms) for RX windows: %d", rx_timeout + LP0_RX_WINDOW_SYNC_TIME_MS);
	k_sleep(K_MSEC(rx_timeout + LP0_RX_WINDOW_SYNC_TIME_MS));

	/* Stop LP0 RX + LoRaWAN RX1 windows */
	lp0_stop_continuous_message_receive(prv_lora_context);

	/* Reset semaphore count in case of unexpected TX/RX error */
	k_sem_reset(&lp0_tx_rx_done_sem);

	/* Start Lorawan RX2 window */
	prv_lp0_device_node_start_receive(lp0_get_rx_timeout_ms(), false);

	/* Wait for RX2 window to close */
	k_sem_take(&lp0_tx_rx_done_sem, K_SECONDS(15));

	/* Resume lorawan module */
	k_sem_give(&lora_chip_suspended_sem);
	lorawan_resume();
}

/**
 * @brief Convert received message into JSON string for offload station mode
 *
 * @param port - Port on which message was received
 * @param raw_payload - Pointer to raw payload bytes
 * @param raw_len - Pointer to length of raw payload
 * @param pkt_type - Packet type
 * @param pkt_status - Pointer to packet status structure
 * @param output_string - Pointer to output string buffer
 * @param max_output_len - Maximum length of output string buffer
 *
 * @retval length of output string on success
 * @retval -ENOMEM if output buffer is not large enough
 */
static int prv_lp0_offload_station_stringify_received_message(uint8_t *port, uint8_t *raw_payload,
							      size_t *raw_len,
							      lr11xx_radio_pkt_type_t pkt_type,
							      void *pkt_status, char *output_string,
							      size_t max_output_len)
{
	size_t current_len = 0;
	int ret = 0;

	/* ==========================
	 * Compute SF, BW, and CR strings first
	 * ==========================
	 */

	const char *sf_str = NULL;
	switch (Main_settings.lp0_communication_params->def_val[0]) {
	case 0x05:
		sf_str = "SF5";
		break;
	case 0x06:
		sf_str = "SF6";
		break;
	case 0x07:
		sf_str = "SF7";
		break;
	case 0x08:
		sf_str = "SF8";
		break;
	case 0x09:
		sf_str = "SF9";
		break;
	case 0x0A:
		sf_str = "SF10";
		break;
	case 0x0B:
		sf_str = "SF11";
		break;
	case 0x0C:
		sf_str = "SF12";
		break;
	default:
		sf_str = "SF?";
		break;
	}

	const char *bw_str = NULL;
	switch (Main_settings.lp0_communication_params->def_val[1]) {
	case 0x01:
		bw_str = "BW15";
		break;
	case 0x02:
		bw_str = "BW31";
		break;
	case 0x03:
		bw_str = "BW62";
		break;
	case 0x04:
		bw_str = "BW125";
		break;
	case 0x05:
		bw_str = "BW250";
		break;
	case 0x06:
		bw_str = "BW500";
		break;
	case 0x08:
		bw_str = "BW10";
		break;
	case 0x09:
		bw_str = "BW20";
		break;
	case 0x0A:
		bw_str = "BW41";
		break;
	case 0x0D:
		bw_str = "BW203";
		break;
	case 0x0E:
		bw_str = "BW406";
		break;
	case 0x0F:
		bw_str = "BW812";
		break;
	default:
		bw_str = "BW?";
		break;
	}

	char datr_string[16] = {0};
	snprintf(datr_string, sizeof(datr_string), "%s%s", sf_str, bw_str);

	const char *cr_str = NULL;
	switch (Main_settings.lp0_communication_params->def_val[2]) {
	case 0x01:
		cr_str = "4/5";
		break;
	case 0x02:
		cr_str = "4/6";
		break;
	case 0x03:
		cr_str = "4/7";
		break;
	case 0x04:
		cr_str = "4/8";
		break;
	case 0x05:
		cr_str = "4/5_LI";
		break;
	case 0x06:
		cr_str = "4/6_LI";
		break;
	case 0x07:
		cr_str = "4/8_LI";
		break;
	default:
		cr_str = "4/?";
		break;
	}

	/* ==========================
	 * Generate the JSON string
	 * ==========================
	 */

	/* ----- Start squiggly bracket ----- */
	ret = snprintf(output_string + current_len, max_output_len - current_len,
		       "\n{\"gatewayEui\": \""); /* Start print with new line in case  */
	if (ret < 0) {
		return -ENOMEM;
	}
	current_len += ret;

	/* ----- Gateway EUI from Main Settings ----- */
	ret = snprintf(output_string + current_len, max_output_len - current_len,
		       "%02X%02X%02X%02X%02X%02X%02X%02X", Main_settings.device_eui->def_val[0],
		       Main_settings.device_eui->def_val[1], Main_settings.device_eui->def_val[2],
		       Main_settings.device_eui->def_val[3], Main_settings.device_eui->def_val[4],
		       Main_settings.device_eui->def_val[5], Main_settings.device_eui->def_val[6],
		       Main_settings.device_eui->def_val[7]);
	if (ret < 0) {
		return -ENOMEM;
	}
	current_len += ret;

	/* ----- RXPK ----- */
	ret = snprintf(output_string + current_len, max_output_len - current_len,
		       "\", \"rxpk\": {");
	if (ret < 0) {
		return -ENOMEM;
	}
	current_len += ret;

	/* ----- Time ----- */
	char time_string[21] = {0};
	uint32_t epoch_time = get_global_unix_time();
	struct tm tm_utc;
	time_t t = (time_t)epoch_time;
	gmtime_r(&t, &tm_utc);
	strftime(time_string, sizeof(time_string), "%Y-%m-%dT%H:%M:%SZ", &tm_utc);

	ret = snprintf(output_string + current_len, max_output_len - current_len,
		       "\"time\": \"%s\", ", time_string);
	if (ret < 0) {
		return -ENOMEM;
	}

	current_len += ret;

	/* ----- Epoch timestamp ----- */
	ret = snprintf(output_string + current_len, max_output_len - current_len, "\"tmst\": %u, ",
		       epoch_time);
	if (ret < 0) {
		return -ENOMEM;
	}
	current_len += ret;

	/* ----- Frequency in MHz ----- */
	int freq_mhz = (Main_settings.lp0_rx_frequency_hz->def_val) / 1000000;
	int freq_trail = Main_settings.lp0_rx_frequency_hz->def_val % 1000000;
	ret = snprintf(output_string + current_len, max_output_len - current_len,
		       "\"freq\": %d.%d, ", freq_mhz, freq_trail);
	if (ret < 0) {
		return -ENOMEM;
	}
	current_len += ret;

	/* ----- Channel, RFCH, STAT ----- */
	ret = snprintf(output_string + current_len, max_output_len - current_len,
		       "\"chan\": 0, \"rfch\": 0, \"stat\": 1, ");
	if (ret < 0) {
		return -ENOMEM;
	}
	current_len += ret;

	/* ----- Modulation ----- */
	ret = snprintf(output_string + current_len, max_output_len - current_len,
		       "\"modu\": \"LORA\", ");
	if (ret < 0) {
		return -ENOMEM;
	}
	current_len += ret;

	/* ----- Datarate and coding rate ----- */
	ret = snprintf(output_string + current_len, max_output_len - current_len,
		       "\"datr\": \"%s\", \"codr\": \"%s\", ", datr_string, cr_str);
	if (ret < 0) {
		return -ENOMEM;
	}
	current_len += ret;

	/* ----- RSSI, SNR, size ----- */
	lr11xx_radio_pkt_status_lora_t *lora_status = (lr11xx_radio_pkt_status_lora_t *)pkt_status;
	ret = snprintf(output_string + current_len, max_output_len - current_len,
		       "\"rssi\": %d, \"lsnr\": %d, \"size\": %u, ", lora_status->rssi_pkt_in_dbm,
		       lora_status->snr_pkt_in_db, (unsigned int)*raw_len);
	if (ret < 0) {
		return -ENOMEM;
	}
	current_len += ret;

	/* ----- Base64 encoded payload ----- */
	/* Calculate presumed max base64 encoded len */
	size_t base64_max_len = ((*raw_len + 2) / 3) * 4;
	/* Clamp on 250 bytes (max payload) -> 340 bytes base64 encoded */
	if (base64_max_len > 340) {
		base64_max_len = 340;
	}

	char base64_payload[base64_max_len + 1];
	size_t olen;
	base64_encode(base64_payload, sizeof(base64_payload), &olen, raw_payload, *raw_len);

	ret = snprintf(output_string + current_len, max_output_len - current_len,
		       "\"data\": \"%s\"}}", base64_payload);
	if (ret < 0) {
		return -ENOMEM;
	}
	current_len += ret;

	return current_len;
}

void lp0_offload_handle_lp0_recv(uint8_t *payload, size_t *len, enum lp0_mode *prv_mode)
{
	switch (payload[0]) {
	case LP0_PING:
		LOG_INF("Received discovery ping from LP0 device!");
		break;
	case LP0_PING_ACK:
	case LP0_START_OFFLOAD:
		if (payload[0] == LP0_PING_ACK) {
			LOG_INF("Received ping acknowledge from LP0 device!");
		} else {
			LOG_INF("Received start offload command from LP0 device!");
		}

		/* LUKATODO: TODO-FUTURE: add framecounter of LP0 to NVS so it will be reboot
		 * persistent. */

		*prv_mode = LP0_MODE_DEVICE_DATA_TRANSFER;

		prv_flash_tx_buf[0] = 0xBB; // CMD_FLASH_GET_ALL
		prv_flash_tx_buf[1] = 0x01;
		prv_flash_tx_buf[2] = 0x01;
		thread_put_message(MB_MSG_LP0, MB_MSG_FLASH, MB_MSG_EXECUTE, 0, prv_flash_tx_buf, 3,
				   250);
		LOG_INF("Sent offload start command to flash thread");

		break;
	case LP0_FLASH_TEST_FILL_100_MESSAGES:
		LOG_INF("Received flash fill 100 messages command from LP0 "
			"device!");
		prv_flash_tx_buf[0] = 0xF4;
		prv_flash_tx_buf[1] = 0x0e;
		for (uint8_t i = 0; i < 100; i++) {
			uint8_t status_size =
				status_get_message(prv_flash_tx_buf + 2, sizeof(prv_flash_tx_buf));
			thread_put_message(MB_MSG_DEV, MB_MSG_FLASH, MB_MSG_STORE,
					   Main_messages.msg_status->port, prv_flash_tx_buf,
					   status_size + 2, 0);

			/* Save message to flash and wait for it to save */
			k_sem_take(&lp0_flash_save_sem, K_FOREVER);
		}
		break;
	default:
		/* Check for custom header and specifically ping acknowledge */
		if (*len >= 2 && payload[0] == 0x42 && payload[1] == 0x69 &&
		    payload[8] == LP0_PING_ACK) {
			LOG_INF("Received custom header from LP0 device!");
			led_turn_on(LED_Y);
			k_sleep(K_SECONDS(1));
			led_turn_off(LED_Y);

			*prv_mode = LP0_MODE_DEVICE_DATA_TRANSFER;

			prv_flash_tx_buf[0] = 0xBB; // CMD_FLASH_GET_ALL
			prv_flash_tx_buf[1] = 0x01;
			prv_flash_tx_buf[2] = 0x01;
			thread_put_message(MB_MSG_LP0, MB_MSG_FLASH, MB_MSG_EXECUTE, 0,
					   prv_flash_tx_buf, 3, 250);
			LOG_INF("Received start offload command from LP0 device!");
			break;
		}
		if (*prv_mode == LP0_MODE_OFFLOAD_STATION) {
			LOG_INF("Offload station mode - Send ping ack");
			led_turn_on(LED_Y);
			k_sleep(K_SECONDS(1));
			led_turn_off(LED_Y);

			/* Send ping acknowledge back to device */
			uint8_t ack_payload[10];
			ack_payload[0] = LP0_PING_ACK;

			lp0_prepare_and_send_message(prv_lora_context, ack_payload,
						     sizeof(ack_payload), 33, false,
						     Main_settings.lp0_tx_frequency_hz->def_val,
						     false, false, LR11XX_RADIO_LORA_IQ_INVERTED);
		} else {
			LOG_WRN("Received unknown command from LP0 device: %d", payload[0]);
		}
		break;
	}
}

void lp0_offload_device_discovery_mode(void)
{
	/* Update settings */
	lp0_update_communication_settings();

	/* Suspend lorawan module and wait for suspension */
	lp0_suspend_lorawan_and_wait_for_suspension(LP0_LORAWAN_SEM_TIMEOUT_MS);

	/* Reset semaphore count in case of residual TX/RX error states from
	 * previous operations */
	k_sem_reset(&lp0_tx_rx_done_sem);

	/* Re-initialize LR11XX chip for LP0 */
	lp0_lr11xx_system_init(prv_lora_context);

	/* Send ping */
	prv_lp0_discovery_ping_send();

	/* Wait for tx_done */
	k_sem_take(&lp0_tx_rx_done_sem, K_SECONDS(15));

	/* Start receiving window(s) */
	prv_lp0_device_perform_full_rx();
}

void lp0_offload_station_discovery_acknowledge(void)
{
	/* Stop LP0 RX + LoRaWAN RX1 windows */
	lp0_stop_continuous_message_receive(prv_lora_context);

	/* Sleep so we don't send the ping response too quickly */
	k_sleep(K_MSEC(500));

	uint8_t received_dev_addr[4] = {0}; /* LUKATODO:TODO-FUTURE: get dev addr from received ping
					       message instead of hardcoding */
	prv_lp0_offload_station_discovery_ping_response_send(received_dev_addr);

#ifdef CONFIG_SDFS_UART_MODULE
	/* Enable sdfs uart */
	sdfs_uart_pm_enable();

	/* Set SDFS time */
	time_t timestamp = get_global_unix_time();
	sdfs_uart_set_time_from_global_unix_timestamp(&timestamp);

	/* On each new discovery ping, create new file. Set output file name */
	prv_lp0_output_file_name(prv_output_file_name, sizeof(prv_output_file_name));

	/* Disable sdfs uart to conserve power */
	sdfs_uart_pm_disable();
#endif /* CONFIG_SDFS_UART_MODULE */
}

int lp0_offload_station_data_receive(uint8_t *raw_payload, size_t *raw_len,
				     lr11xx_radio_pkt_type_t pkt_type, void *pkt_status)
{
	/* LUKATODO:TODO-FUTURE: add a special mode that will save ALL messages heard, default
	 * should be device specific */
	char output_string[600] = {0};

	int ret;
	ret = prv_lp0_offload_station_stringify_received_message(
		output_string, raw_payload, raw_len, pkt_type, pkt_status, output_string,
		sizeof(output_string));
	if (ret < 0) {
		LOG_ERR("Stringify received message failed: %d", ret);
		return ret;
	}

	led_turn_off(LED_ALL);
	led_turn_on(LED_M);
#ifdef CONFIG_SDFS_UART_MODULE
	/* Enable sdfs uart */
	int err = sdfs_uart_pm_enable();
	if (err < 0) {
		LOG_ERR("Failed to enable SDFS UART: %d", err);
		return err;
	}

	time_t timestamp = get_global_unix_time();
	sdfs_uart_set_time_from_global_unix_timestamp(&timestamp);

	/* Write data to SDFS over UART */
	ret = sdfs_uart_file_append(prv_output_file_name, output_string, strlen(output_string));
	if (ret < 0) {
		LOG_ERR("SDFS UART write failed: %d (fprot error code)", ret);
		if (ret == -FPROT_LOCKED) {
			LOG_ERR("File error (Possible file overflow). "
				"Retrying with new file with incremented "
				"file name.");

			/* Set output file name */
			prv_lp0_output_file_name(prv_output_file_name,
						 sizeof(prv_output_file_name));

			ret = sdfs_uart_file_append(prv_output_file_name, output_string,
						    strlen(output_string));
			if (ret < 0) {
				LOG_ERR("SDFS UART write failed again: %d", ret);
			} else {
				LOG_INF("Wrote %d bytes to SDFSUART", ret);
			}
		}
	} else {
		LOG_INF("Wrote %d bytes to SDFSUART", ret);
	}

	/* Disable sdfs uart to conserve power */
	sdfs_uart_pm_disable();
#endif /* CONFIG_SDFS_UART_MODULE */

	led_turn_off(LED_M);

	/* Stop LP0 RX + LoRaWAN RX1 windows */
	lp0_stop_continuous_message_receive(prv_lora_context);
	prv_lp0_offload_station_start_receive(0, true);
	return 0;
}

void lp0_offload_station_discovery(void)
{
	lp0_update_communication_settings();

	/* Send suspend event to LoRaWAN thread */
	lp0_suspend_lorawan_and_wait_for_suspension(LP0_LORAWAN_SEM_TIMEOUT_MS);

	lp0_lr11xx_system_init(prv_lora_context);

	/* Set output file name */
	prv_lp0_output_file_name(prv_output_file_name, sizeof(prv_output_file_name));

	/* 1. Enable RX and wait for ping from device to
	 * offload station */
	prv_lp0_offload_station_start_receive(0, true);

	/* Wait until ping is received and ACK is sent */
	k_sem_take(&lp0_tx_rx_done_sem, K_FOREVER); /* Wait for rx */
}
