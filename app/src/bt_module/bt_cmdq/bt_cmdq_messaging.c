/** @file bt_cmdq_messaging.c
 *
 * @brief Messaging functions for bt_cmdq module.
 *
 * Handles communication between the bt_cmdq module and satellite/LoRa/flash modules.
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2024 Irnas. All rights reserved.
 */

#include <bt_cmdq.h>
#include <bt_cmdq_messaging.h>
#include <definitions.h>
#include <generated_settings.h>
#include <lp0.h>
#include <messages_def.h>
#include <thread_com.h>

#include <zephyr/logging/log.h>

/* Max nr of scan results to be stored at a time */
#define BT_CMDQ_SCAN_BUFF_LEN 50
#define BT_CMDQ_RES_HEAD_LEN  MESSAGE_HEAD_LEN

LOG_MODULE_REGISTER(BT_CMDQ_MESSAGING);

/* Size of important data that is provided by CMDQ and will be sent. */
#define CMDQ_IMPORTANT_DATA_LEN    11
/* Offset of important data in CMDQ buffer. */
#define CMDQ_IMPORTANT_DATA_OFFSET 17
/* Minimum required CMDQ device response message data length */
#define CMDQ_MIN_TOTAL_DATA_LENGTH (CMDQ_IMPORTANT_DATA_OFFSET + CMDQ_IMPORTANT_DATA_LEN)

struct bt_cmdq_scan_result {
	uint8_t buffer[CMDQ_IMPORTANT_DATA_LEN];
	uint32_t timestamp;
};

struct bt_cmdq_scan_result bt_cmdq_result_buf[BT_CMDQ_SCAN_BUFF_LEN];
static uint8_t bt_cmdq_result_counter = 0;
static struct bt_cmdq_scan_result latest_bt_cmdq_result;

void bt_cmdq_messaging_print_scan_data(uint8_t *data, size_t len, uint32_t timestamp)
{
	if (len == 0) {
		LOG_INF("Scan data is NULL. Unsuccessful scan results received.");
		return;
	}
	if (len < CMDQ_MIN_TOTAL_DATA_LENGTH) {
		LOG_ERR("Scan data length is too short. Are you sure you're scanning the right "
			"device?");
		return;
	}
	LOG_INF("5 Minute RR Median: %d", data[17]);
	LOG_INF("5 Minute RR Median Modesum: %d", data[18]);
	LOG_INF("5 Minute Activity Average: %d", data[19]);
	LOG_INF("5 Minute Activity Max: %d", data[20]);
	LOG_INF("Active Minutes in the last hour: %d", data[21]);
	LOG_INF("Temperature: %d", (uint16_t)data[22] << 8 | data[23]);
	LOG_INF("Hourly Impedance: %d", (uint16_t)data[24] << 8 | data[25]);
	LOG_INF("5 minute HRV: %d", (uint16_t)data[26] << 8 | data[27]);
	LOG_INF("Timestamp: %d", timestamp);
}

void bt_cmdq_messaging_save_to_buffer(uint8_t *data, size_t len, uint32_t timestamp)
{
	/* Check if data is NULL, signaling an unsuccessful scan */
	if (!data) {
		return;
	}

	uint8_t important_data[CMDQ_IMPORTANT_DATA_LEN];
	memcpy(important_data, data + CMDQ_IMPORTANT_DATA_OFFSET, CMDQ_IMPORTANT_DATA_LEN);

	/* Save scan result to buffer */
	if (bt_cmdq_result_counter < BT_CMDQ_SCAN_BUFF_LEN) {
		bt_cmdq_messaging_print_scan_data(data, len, timestamp);
		memcpy(bt_cmdq_result_buf[bt_cmdq_result_counter].buffer, important_data,
		       sizeof(important_data));
		bt_cmdq_result_buf[bt_cmdq_result_counter].timestamp = timestamp;
		/* Save latest result separately */
		memcpy(latest_bt_cmdq_result.buffer, important_data, sizeof(important_data));
		latest_bt_cmdq_result.timestamp = timestamp;
		bt_cmdq_result_counter++;
	} else {
		LOG_WRN("BT scan result buffer is full. Discarding scan result.");
	}
}

/**
 * @brief reorder and clear old messages from buffer
 *
 * Clears all sent messages from buffer and reorders the buffer.
 * The leftover messages are moved to the beginning of the buffer.
 *
 * Updates the bt_cmdq_result_counter to the new number of messages in buffer.
 *
 * @param[in] number_of_leftover_results number of events still in buffer, but not sent
 *
 */
void prv_reorder_and_clean_results_buffer(int number_of_leftover_results)
{
	/* Reorder buffer */
	for (int i = 0; i < number_of_leftover_results; i++) {
		memcpy(&bt_cmdq_result_buf[i],
		       &bt_cmdq_result_buf[i + bt_cmdq_result_counter - number_of_leftover_results],
		       sizeof(struct bt_cmdq_scan_result));
	}
	/* Clear rest of buffer */
	for (int i = number_of_leftover_results; i < bt_cmdq_result_counter; i++) {
		memset(&bt_cmdq_result_buf[i], 0, sizeof(struct bt_cmdq_scan_result));
	}
	bt_cmdq_result_counter = number_of_leftover_results;
}

int bt_cmdq_messaging_compose_latest_results_message(uint8_t *payload, uint8_t max_len)
{
	/* message id */
	payload[0] = Main_messages.msg_ble_cmdq->id;
	/* current position in message */
	int result_idx = MESSAGE_HEAD_LEN;
	if (result_idx + TIMESTAMP_SIZE + CMDQ_IMPORTANT_DATA_LEN < max_len) {
		/* Timestamp */
		payload[result_idx + 3] = latest_bt_cmdq_result.timestamp >> 24;
		payload[result_idx + 2] = latest_bt_cmdq_result.timestamp >> 16;
		payload[result_idx + 1] = latest_bt_cmdq_result.timestamp >> 8;
		payload[result_idx] = latest_bt_cmdq_result.timestamp;
		/* Important data */
		for (int j = 0; j < CMDQ_IMPORTANT_DATA_LEN; j++) {
			payload[result_idx + 4 + j] = latest_bt_cmdq_result.buffer[j];
		}
		result_idx += TIMESTAMP_SIZE + CMDQ_IMPORTANT_DATA_LEN;
	}
	/* Message length. Remove head length. */
	payload[1] = result_idx - MESSAGE_HEAD_LEN;
	return result_idx;
}

int bt_cmdq_messaging_compose_results_message(uint8_t *payload, uint8_t max_len)
{
	/* message id */
	payload[0] = Main_messages.msg_ble_cmdq->id;
	/* current position in message */
	int result_idx = MESSAGE_HEAD_LEN;
	for (int message_iterator = 0;
	     message_iterator < bt_cmdq_result_counter && result_idx < max_len;
	     message_iterator++) {
		/* Check if we have space */
		if (result_idx + TIMESTAMP_SIZE + CMDQ_IMPORTANT_DATA_LEN < max_len) {
			/* Timestamp */
			payload[result_idx + 3] =
				bt_cmdq_result_buf[message_iterator].timestamp >> 24;
			payload[result_idx + 2] =
				bt_cmdq_result_buf[message_iterator].timestamp >> 16;
			payload[result_idx + 1] =
				bt_cmdq_result_buf[message_iterator].timestamp >> 8;
			payload[result_idx] = bt_cmdq_result_buf[message_iterator].timestamp;
			/* Important data */
			for (int j = 0; j < CMDQ_IMPORTANT_DATA_LEN; j++) {
				payload[result_idx + 4 + j] =
					bt_cmdq_result_buf[message_iterator].buffer[j];
			}
			result_idx += TIMESTAMP_SIZE + CMDQ_IMPORTANT_DATA_LEN;
		} else {
			break;
		}
	}
	/* Message length. Remove head length. */
	payload[1] = result_idx - MESSAGE_HEAD_LEN;
	return result_idx;
}

/**
 * @brief send BT CMDQ results message to flash and/or LR, satellite
 *
 * @param message buffer containing the message
 * @param len length of the message

 * @return int 0 on success, negative error code otherwise
 */
static int prv_send_bt_cmdq_result_message(uint8_t *message, uint8_t len)
{
	int err = 0;
	LOG_INF("Compose and send BT CMDQ structure via LR and or to flash, satellite.");
	if (check_flash_store_flag(Main_messages.msg_ble_cmdq->port)) {
		err = thread_put_message(MB_MSG_DEV, MB_MSG_FLASH, MB_MSG_STORE,
					 Main_messages.msg_ble_cmdq->port, message, len, 0);
	}
	if (check_lr_send_flag(Main_messages.msg_ble_cmdq->port)) {
		err = thread_put_message(MB_MSG_DEV, MB_MSG_LORA, MB_MSG_SEND,
					 Main_messages.msg_ble_cmdq->port, message, len, 0);
	}
	if (check_sat_send_flag(Main_messages.msg_ble_cmdq->port)) {
		err = thread_put_message(MB_MSG_DEV, MB_MSG_SAT, MB_MSG_SEND,
					 Main_messages.msg_ble_cmdq->port, message, len, 0);
	}
	if (check_lp0_send_flag(Main_messages.msg_ble_cmdq->port)) {
		/* Add event to LP0 que */
		lp0_add_message_to_send_queue(message, len, Main_messages.msg_ble_cmdq->port, false,
					      false);
	}
	return err;
}

int bt_cmdq_messaging_results_send(uint8_t *message, size_t max_len)
{
	/* Check max length */
	if (check_flash_store_flag(Main_messages.msg_ble_cmdq->port)) {
		/* Compose message and send it to flash thread, calculate space for flash
		 * timestamp and port - 5 bytes */
		max_len -= (BT_CMDQ_RES_HEAD_LEN + 5);
	}

	/* Check if we report empty message */
	if (bt_cmdq_result_counter == 0 &&
	    !Main_settings.cmdq_report_zero_messages_to_be_sent->def_val) {
		LOG_INF("No new BT CMDQ. No message will be sent.");
		return 0;
	}

	int len = 0;
	len = bt_cmdq_messaging_compose_results_message(message, max_len);

	int payload_size = message[1];

	/* Remove sent messages from the buffer */
	prv_reorder_and_clean_results_buffer(
		bt_cmdq_result_counter - payload_size / (CMDQ_IMPORTANT_DATA_LEN + TIMESTAMP_SIZE));

	int err = prv_send_bt_cmdq_result_message(message, len);
	if (err) {
		LOG_ERR("Error sending BT CMDQ results message.");
	}
	return len;
}
