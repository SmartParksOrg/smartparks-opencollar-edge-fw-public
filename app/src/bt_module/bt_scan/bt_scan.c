/** @file bt_scan.c
 *
 * @brief Interface to process bt scan results
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2020 Irnas. All rights reserved.
 */
#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/types.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/hci_vs.h>

/* Local settings */
#include "bt_adv.h"
#include "bt_con.h"
#include "definitions.h"
#include "generated_settings.h"
#include "global_time.h"
#include "lp0.h"
#include "thread_com.h"

#include "bt_scan.h"

#define BT_SCAN_BUFF_LEN          BT_SCAN_MAX_RES
#define BT_SCAN_SEND_MAX_RES      5 // Send max 5 res in aggregated message
#define SINGLE_BT_SCAN_RESULT_LEN 9 // Length of single result in bytes
#define SINGLE_BT_SCAN_SHORT_LEN  4 // Length of short scan result in bytes
#define BT_SCAN_MIN_PERIOD        5 // Min period in s to count repeated result

#define BT_SCAN_RES_HEAD_LEN MESSAGE_HEAD_LEN + TIMESTAMP_SIZE

LOG_MODULE_REGISTER(BT_SCAN_APP, 3); // init logging

struct bt_scan_result {
	int8_t rssi;
	int8_t best_rssi;
	bt_addr_t mac_address;
	uint32_t timestamp;
	uint32_t best_timestamp;
	uint8_t counter;
};

enum filter_type {
	BT_SCAN_FILTER_NONE = 0,
	BT_SCAN_FILTER_SP = 1,
	BT_SCAN_FILTER_MAC = 2,
	BT_SCAN_FILTER_PHONE = 3,
};

enum bt_result_sort {
	BT_SCAN_SORT_BY_BEST_RSSI = 0,
	BT_SCAN_SORT_BY_RSSI = 1,
	BT_SCAN_SORT_BY_COUNT = 2,
};

enum mac_match {
	BT_NO_MAC_MATCH = -1,
	BT_MAC_MATCH_TIME_TOO_CLOSE = -2
};

uint8_t nr_bt_scan_results = 0;
struct bt_scan_result bt_result_buf[BT_SCAN_BUFF_LEN];

enum bt_result_sort sort_type = BT_SCAN_SORT_BY_RSSI;

uint8_t bt_scan_buf[MAX_BUF_SIZE];
uint8_t bt_scan_buf_max_len = MESSAGE_LR_MAX_LEN;

const struct bt_le_scan_param scan_param = {
	.type = BT_HCI_LE_SCAN_PASSIVE,
	.options = BT_LE_SCAN_OPT_FILTER_DUPLICATE,
	.interval = 32, //(N * 0.625 ms)
	.window = 32,   //(N * 0.625 ms)
};

static int add_bt_scan_result(const bt_addr_le_t *addr, int8_t rssi, uint32_t timestamp);
static int find_bt_mac_match(const bt_addr_le_t *addr, uint32_t timestamp,
			     struct bt_scan_result *buf, uint8_t buf_len);
static int increase_bt_scan_counter(uint8_t pos_idx, struct bt_scan_result *buf, int8_t rssi,
				    uint32_t timestamp);
static void add_new_bt_scan_entry(struct bt_scan_result *buf, uint8_t *buf_len,
				  const bt_addr_le_t *addr, int8_t rssi, uint32_t timestamp);
static void sort_bt_scan_results_by_count(void);
static void sort_bt_scan_results_by_rssi(void);
static void sort_bt_scan_results_by_best_rssi(void);
static bool filter_scan_data(const bt_addr_le_t *addr, struct net_buf_simple *buf,
			     enum filter_type filter, uint16_t man_data);

/*******************************************************************************************************/
/* Private functions */
/*******************************************************************************************************/

/** Add new scan result to data structure.
 *
 * @param[in] bt_addr_le_t *addr - MAC address
 * @param[in] rssi
 * @param[in] timestamp
 *
 * @return zero on success, or a negative error code if the same result was already added to the
 * structure.
 */
static int add_bt_scan_result(const bt_addr_le_t *addr, int8_t rssi, uint32_t timestamp)
{
	// Check in we already have this MAC address and timestamp
	int match_idx = find_bt_mac_match(addr, timestamp, bt_result_buf, nr_bt_scan_results);
	// Match found
	if (match_idx >= 0) {
		// If already in buffer increase counter and reorder structure
		increase_bt_scan_counter(match_idx, bt_result_buf, rssi, timestamp);
	}
	// New entry
	else if (match_idx == BT_NO_MAC_MATCH) {
		// Add new entry
		add_new_bt_scan_entry(bt_result_buf, &nr_bt_scan_results, addr, rssi, timestamp);
	}
	// We already have match with the timestamp
	else {
		return -EIO;
	}

	return 0;
}

/** Check if scanned mac address is already added to the structure.
 *
 * @param[in] bt_addr_le_t *addr - MAC address
 * @param[in] rssi
 * @param[in] timestamp
 * @param[in] bt_scan_result buf - buffer containing aggregated scan results
 * @param[in] buf_len current buffer length
 *
 * @return match index on success, or a negative code if match is not already present or time is too
 * close.
 */
static int find_bt_mac_match(const bt_addr_le_t *addr, uint32_t timestamp,
			     struct bt_scan_result *buf, uint8_t buf_len)
{
	// Loop over storage buffer
	bool match = false;
	for (uint8_t i = 0; i < buf_len; i++) {
		match = true;
		for (uint8_t j = 0; j < 6; j++) {
			if (addr->a.val[j] != buf[i].mac_address.val[j]) {
				match = false;
				break; // Exit loop
			}
		}
		if (match) {
			// Match found, check timestamp to be at least 5s apart
			if (timestamp - buf[i].timestamp < 5) {
				return BT_MAC_MATCH_TIME_TOO_CLOSE; // Skip this entry
			}
			return i; // Return match position
		}
	}
	return BT_NO_MAC_MATCH;
}

/** Update single entry data and increase counter.
 *
 * @param[in] pos_idx - index of entry in structure
 * @param[in] buf - buffer containing aggregated scan results
 * @param[in] rssi - new rssi
 * @param[in] timestamp - new timestamp
 *
 * @return 0 on success, or a negative code on invalid data.
 */
static int increase_bt_scan_counter(uint8_t pos_idx, struct bt_scan_result *buf, int8_t rssi,
				    uint32_t timestamp)
{
	if (rssi < 0) {
		// Update new rssi
		buf[pos_idx].rssi = rssi;
		// Update timestamp
		buf[pos_idx].timestamp = timestamp;

		// Update data only if better signal is encountered
		if (buf[pos_idx].best_rssi < rssi) {
			buf[pos_idx].best_rssi = rssi;
			// Update timestamp
			buf[pos_idx].best_timestamp = timestamp;
		}
	} else {
		return -EIO;
	}

	// Increase counter
	buf[pos_idx].counter++;

	// Sort buffer
	struct bt_scan_result tmp = buf[pos_idx];
	for (uint8_t i = pos_idx; i > 0; i--) {
		buf[i] = buf[i - 1];
	}
	buf[0] = tmp;

	return 0;
}

/** Add single entry at the end of the buffer.
 *
 * @param[in] buf - buffer containing aggregated scan results
 * @param[in] buf_len current buffer length
 * @param[in] bt_addr_le_t *addr - MAC address
 * @param[in] rssi - new rssi
 * @param[in] timestamp - new timestamp
 *
 * @return void.
 */
static void add_new_bt_scan_entry(struct bt_scan_result *buf, uint8_t *buf_len,
				  const bt_addr_le_t *addr, int8_t rssi, uint32_t timestamp)
{
	// Adjust buffer size
	*buf_len = MIN(BT_SCAN_BUFF_LEN, *buf_len + 1);

	// Move buffer
	for (uint8_t i = *buf_len - 1; i > 0; i--) {
		buf[i] = buf[i - 1];
	}

	// Add new entry
	buf[0].counter = 1;
	buf[0].rssi = rssi;
	buf[0].best_rssi = rssi;
	buf[0].timestamp = timestamp;
	buf[0].best_timestamp = timestamp;
	buf[0].mac_address = addr->a;
}

/** Sort result structure by entry rssi.
 *
 */
static void sort_bt_scan_results_by_rssi(void)
{
	struct bt_scan_result tmp;
	int8_t max_rssi = 0;
	uint8_t max_idx = 0;
	// Debug
	LOG_DBG("RSSI before sort:");
	for (uint8_t i = 0; i < nr_bt_scan_results; i++) {
		LOG_DBG("%d", bt_result_buf[i].rssi);
	}

	// Simple sort of results
	for (uint8_t i = 0; i < nr_bt_scan_results; i++) {
		// Find new max
		max_rssi = bt_result_buf[i].rssi;
		max_idx = i;
		for (uint8_t j = i + 1; j < nr_bt_scan_results; j++) {
			if (bt_result_buf[j].rssi > max_rssi) {
				max_rssi = bt_result_buf[j].rssi;
				max_idx = j;
			}
		}
		// Move up
		if (max_idx > i) {
			tmp = bt_result_buf[max_idx];
			for (uint8_t j = max_idx; j > i; j--) {
				bt_result_buf[j] = bt_result_buf[j - 1];
			}
			bt_result_buf[i] = tmp;
		}
	}

	LOG_DBG("RSSI after sort:");
	for (uint8_t i = 0; i < nr_bt_scan_results; i++) {
		LOG_DBG("%d", bt_result_buf[i].rssi);
	}
}

/** Sort result structure by best entry rssi.
 *
 */
static void sort_bt_scan_results_by_best_rssi(void)
{
	struct bt_scan_result tmp;
	int8_t max_rssi = 0;
	uint8_t max_idx = 0;
	// Debug
	LOG_DBG("RSSI before sort:");
	for (uint8_t i = 0; i < nr_bt_scan_results; i++) {
		LOG_DBG("%d", bt_result_buf[i].best_rssi);
	}

	// Simple sort of results
	for (uint8_t i = 0; i < nr_bt_scan_results; i++) {
		// Find new max
		max_rssi = bt_result_buf[i].best_rssi;
		max_idx = i;
		for (uint8_t j = i + 1; j < nr_bt_scan_results; j++) {
			if (bt_result_buf[j].best_rssi > max_rssi) {
				max_rssi = bt_result_buf[j].best_rssi;
				max_idx = j;
			}
		}
		// Move up
		if (max_idx > i) {
			tmp = bt_result_buf[max_idx];
			for (uint8_t j = max_idx; j > i; j--) {
				bt_result_buf[j] = bt_result_buf[j - 1];
			}
			bt_result_buf[i] = tmp;
		}
	}

	LOG_DBG("RSSI after sort:");
	for (uint8_t i = 0; i < nr_bt_scan_results; i++) {
		LOG_DBG("%d", bt_result_buf[i].best_rssi);
	}
}

/** Sort result structure by entry count.
 *
 */
static void sort_bt_scan_results_by_count(void)
{
	struct bt_scan_result tmp;
	uint8_t max_count = 0;
	uint8_t max_idx = 0;
	// Debug
	LOG_DBG("Count before sort:");
	for (uint8_t i = 0; i < nr_bt_scan_results; i++) {
		LOG_DBG("%d", bt_result_buf[i].counter);
	}

	// Simple sort of results
	for (uint8_t i = 0; i < nr_bt_scan_results; i++) {
		// Find new max
		max_count = bt_result_buf[i].counter;
		max_idx = i;
		for (uint8_t j = i + 1; j < nr_bt_scan_results; j++) {
			if (bt_result_buf[j].counter > max_count) {
				max_count = bt_result_buf[j].counter;
				max_idx = j;
			}
		}
		// Move up
		if (max_idx > i) {
			tmp = bt_result_buf[max_idx];
			for (uint8_t j = max_idx; j > i; j--) {
				bt_result_buf[j] = bt_result_buf[j - 1];
			}
			bt_result_buf[i] = tmp;
		}
	}

	LOG_DBG("Count after sort:");
	for (uint8_t i = 0; i < nr_bt_scan_results; i++) {
		LOG_DBG("%d", bt_result_buf[i].counter);
	}
}

/**
 * @brief Add single entry at the end of the buffer.
 *
 * @param[in] addr
 * @param[in] buf
 * @param[in] filter
 * @param[in] man_data
 *
 * @return true if filter pass, false if not
 */
static bool filter_scan_data(const bt_addr_le_t *addr, struct net_buf_simple *buf,
			     enum filter_type filter, uint16_t man_data)
{
	switch (filter) {
	case BT_SCAN_FILTER_NONE: {
		// No filter, use all data
		return true;
	}
	case BT_SCAN_FILTER_SP: {
		// Filter SmartParks devices
		// Read length and message type
		// uint8_t len = buf->data[0];
		uint8_t type = buf->data[1];
		if (type == BT_DATA_MANUFACTURER_DATA) {
			uint16_t new_man_data = buf->data[3] << 8 | buf->data[2];
			if (man_data == new_man_data) {
				LOG_INF("Correct manufacturer data detected!");

				return true;
			}
		}
		return false;
	}
	case BT_SCAN_FILTER_PHONE: {
		if ((addr->a.val[0] & 0b10) == 0) {
			return false;
		} else {
			return true;
		}
	}
	default: {
		return false;
	}
	}
}

/**
 * @brief BT scan callback.
 *
 * Filter new BT scan result with provided filtering function.
 * If pass, add new result to result buffer.
 *
 * @param[in] addr - bt address
 * @param[in] rssi - result rssi
 * @param[in] adv_type
 * @param[in] buf - data buffer
 */
static void scan_cb(const bt_addr_le_t *addr, int8_t rssi, uint8_t adv_type,
		    struct net_buf_simple *buf)
{
	// Check if new data passes filter
	if (!filter_scan_data(addr, buf, Main_settings.ble_scan_filter->def_val,
			      Main_settings.ble_scan_manufacturer_id->def_val)) {
		return;
	}
	// Add result to buffer
	if (!add_bt_scan_result(addr, rssi, get_global_unix_time())) {
		LOG_INF("BT scan MAC: %x:%x:%x:%x:%x:%x rssi: %d", addr->a.val[0], addr->a.val[1],
			addr->a.val[2], addr->a.val[3], addr->a.val[4], addr->a.val[5], rssi);
	}
}

/**
 * @brief compose and send bluetooth scan result message
 *
 * @param[in] message - buffer
 * @param[in] max_len - max length of single message
 * @param[in] scan_time - timestamp of scan we want to obtain results for.
 * @param[in] res_idx - index of result in results buffer
 *
 * @return int 0 on success, negative on error otherwise
 */
static int compose_and_send_single_scan_result_message(uint8_t *message, uint8_t max_len,
						       uint32_t scan_time, uint8_t res_idx)
{
	int err = 0;
	// Add results to buffer - as many as possible
	int len = compose_message_bt_scan_result_single_scan(message + BT_SCAN_RES_HEAD_LEN,
							     max_len, scan_time, res_idx);
	if (len < 0) {
		len = 0;
	}
	if (len > 0) {
		// Add message id
		message[0] = Main_messages.msg_ble_scan->id;
		// Add message length
		message[1] = len + TIMESTAMP_SIZE;
		len += BT_SCAN_RES_HEAD_LEN; // Add head
		// Add scan timestamp
		message[5] = scan_time >> 24;
		message[4] = scan_time >> 16;
		message[3] = scan_time >> 8;
		message[2] = scan_time;

		LOG_INF("Compose and send BT scan structure via LR and or to flash.");
		if (check_flash_store_flag(Main_messages.msg_ble_scan->port)) {
			err = thread_put_message(MB_MSG_DEV, MB_MSG_FLASH, MB_MSG_STORE,
						 Main_messages.msg_ble_scan->port, message, len, 0);
		}
		// Send only first LR message
		if (check_lr_send_flag(Main_messages.msg_ble_scan->port) && res_idx == 0) {
			err = thread_put_message(MB_MSG_DEV, MB_MSG_LORA, MB_MSG_SEND,
						 Main_messages.msg_ble_scan->port, message, len, 0);
		}
		// Send only first SAT message
		if (check_sat_send_flag(Main_messages.msg_ble_scan->port) && res_idx == 0) {
			err = thread_put_message(MB_MSG_DEV, MB_MSG_SAT, MB_MSG_SEND,
						 Main_messages.msg_ble_scan->port, message, len, 0);
		}
		// Send only first LP0 message
		if (check_lp0_send_flag(Main_messages.msg_ble_scan->port) && res_idx == 0) {
			/* Add event to LP0 que */
			err = lp0_add_message_to_send_queue(
				message, len, Main_messages.msg_ble_scan->port, false, false);
		}
	}

	return err;
}

/*******************************************************************************************************/
/* Public functions*/
/*******************************************************************************************************/

int bt_scan(void)
{
	int err = 0;
	LOG_INF("Start BLE scan.");

	err = bt_le_scan_start(&scan_param, scan_cb);
	if (err) {
		LOG_ERR("Starting scanning failed (err %d)", err);
		return err;
	}
	k_sleep(K_MSEC(Main_settings.ble_scan_duration->def_val));
	err = bt_le_scan_stop();

	return err;
}

int bt_scan_send(uint8_t *message, uint8_t max_len, uint32_t scan_time)
{
	int err = 0;
	// Check max length
	if (check_flash_store_flag(Main_messages.msg_ble_scan->port)) {
		// Compose message and send it to flash thread, calculate space for flash timestam
		// and port - 5 bytes
		max_len -= (BT_SCAN_RES_HEAD_LEN + 5);
	} else if (check_lr_send_flag(Main_messages.msg_ble_scan_aggregated->port)) {
		// Compose message and send it to LR thread
		max_len -= BT_SCAN_RES_HEAD_LEN;
	}

	// Get predicted nr of scan results to store and send
	uint8_t nr_res = get_nr_timestamp_scan_results(scan_time);

	// Sort results according to rssi
	sort_bt_scan_results_by_rssi();

	// Res idx set to 0 - start from first result
	uint8_t res_idx = 0;

	/* Check if reporting zero connections is enabled and no connections were found */
	if (Main_settings.ble_scan_report_zero_connections_found->def_val && nr_res == 0) {
		LOG_INF("No connections found. Sending report.");
		err = compose_and_send_single_scan_result_message(message, max_len, scan_time,
								  res_idx);

	} else {
		// Loop over found connections and report
		while (res_idx < nr_res) {
			err = compose_and_send_single_scan_result_message(message, max_len,
									  scan_time, res_idx);
			res_idx += message[MESSAGE_HEAD_LEN +
					   TIMESTAMP_SIZE]; // Add nr of send contacts
		}
	}
	return err;
}

int bt_scan_send_aggregated(uint8_t *message, uint8_t max_len)
{
	int err = 0;
	// Check max length
	if (check_flash_store_flag(Main_messages.msg_ble_scan_aggregated->port)) {
		max_len -= (MESSAGE_HEAD_LEN + 5);
	} else if (check_lr_send_flag(Main_messages.msg_ble_scan_aggregated->port)) {
		max_len -= MESSAGE_HEAD_LEN;
	}

	// Compose message and send it to LR thread
	int len = compose_message_bt_scan_results(message + 2, max_len, BT_SCAN_SEND_MAX_RES);
	message[0] = Main_messages.msg_ble_scan_aggregated->id;
	message[1] = len;
	len += MESSAGE_HEAD_LEN; // Add head
	if (len > 0) {
		LOG_INF("Compose and send BT aggregated scan structure via LR and or to flash.");
		if (check_flash_store_flag(Main_messages.msg_ble_scan_aggregated->port)) {
			err = thread_put_message(MB_MSG_DEV, MB_MSG_FLASH, MB_MSG_STORE,
						 Main_messages.msg_ble_scan_aggregated->port,
						 message, len, 0);
		}
		if (check_lr_send_flag(Main_messages.msg_ble_scan_aggregated->port)) {
			err = thread_put_message(MB_MSG_DEV, MB_MSG_LORA, MB_MSG_SEND,
						 Main_messages.msg_ble_scan_aggregated->port,
						 message, len, 0);
		}
		if (check_sat_send_flag(Main_messages.msg_ble_scan_aggregated->port)) {
			err = thread_put_message(MB_MSG_DEV, MB_MSG_SAT, MB_MSG_SEND,
						 Main_messages.msg_ble_scan_aggregated->port,
						 message, len, 0);
		}
		if (check_lp0_send_flag(Main_messages.msg_ble_scan_aggregated->port)) {
			/* Add event to LP0 que */
			lp0_add_message_to_send_queue(message, len,
						      Main_messages.msg_ble_scan_aggregated->port,
						      false, false);
		}
	}

	return err;
}

int compose_message_bt_scan_results(uint8_t *payload, uint8_t max_len, uint8_t max_res)
{
	// First byte is nr of results
	payload[0] = nr_bt_scan_results;
	uint8_t idx = 1; // payload index

	switch (sort_type) {
	case BT_SCAN_SORT_BY_BEST_RSSI: {
		sort_bt_scan_results_by_best_rssi();
		break;
	}
	case BT_SCAN_SORT_BY_RSSI: {
		sort_bt_scan_results_by_rssi();
		break;
	}
	case BT_SCAN_SORT_BY_COUNT: {
		sort_bt_scan_results_by_count();
		break;
	}
	default: {
		LOG_WRN("Sort type not supported!");
	}
	}

	// Loop over results
	for (uint8_t i = 0; i < nr_bt_scan_results; i++) {
		// Check if we have space and no more than max nr. of results is send
		if (idx + SINGLE_BT_SCAN_RESULT_LEN < max_len && i < max_res) {
			// MAC only first 3
			for (uint8_t j = 0; j < 3; j++) {
				payload[idx + j] = bt_result_buf[i].mac_address.val[j];
			}
			// signal
			payload[idx + 3] = (uint8_t)(bt_result_buf[i].best_rssi + 128);
			// counter
			payload[idx + 4] = bt_result_buf[i].counter;
			// timestamp
			payload[idx + 8] = bt_result_buf[i].best_timestamp >> 24;
			payload[idx + 7] = bt_result_buf[i].best_timestamp >> 16;
			payload[idx + 6] = bt_result_buf[i].best_timestamp >> 8;
			payload[idx + 5] = bt_result_buf[i].best_timestamp;

			idx += SINGLE_BT_SCAN_RESULT_LEN;
		}
		// Clear structure after it is send
		memset(&bt_result_buf[i], 0, sizeof(bt_result_buf[i]));
	}

	nr_bt_scan_results = 0;

	return idx;
}

int compose_message_bt_scan_result_single_scan(uint8_t *payload, uint8_t max_len,
					       uint32_t scan_time, uint8_t start_idx)
{
	uint8_t nr_latest = 0;   // nr of results in latest scan
	uint8_t idx = 1;         // payload index
	uint8_t skipped_res = 0; // nr. of skipped results when forming a message

	// Loop over results
	LOG_INF("Compose message with scan results timestamp: %d", scan_time);
	for (uint8_t i = 0; i < nr_bt_scan_results; i++) {
		// Check if we have space
		if (idx + SINGLE_BT_SCAN_SHORT_LEN < max_len) {
			// Check if any scan result were obtained in the last 5 s
			if (scan_time - bt_result_buf[i].timestamp < BT_SCAN_MIN_PERIOD) {
				// Skip results that were already send
				if (skipped_res < start_idx) {
					skipped_res++;
				} else {
					LOG_DBG("Use entry with timestamp: %d",
						bt_result_buf[i].timestamp);
					// MAC
					for (uint8_t j = 0; j < 3; j++) {
						payload[idx + j] =
							bt_result_buf[i].mac_address.val[j];
					}
					// signal
					payload[idx + 3] = (uint8_t)(bt_result_buf[i].rssi + 128);

					idx += SINGLE_BT_SCAN_SHORT_LEN;
					nr_latest += 1;
				}
			}
		}
	}

	// First byte is nr of results
	payload[0] = nr_latest;

	return idx;
}

uint8_t get_nr_scan_results(void)
{
	return nr_bt_scan_results;
}

uint8_t get_nr_timestamp_scan_results(uint32_t timestamp)
{
	uint8_t nr_latest = 0; // nr of results in latest scan
	// Loop over results
	for (uint8_t i = 0; i < nr_bt_scan_results; i++) {
		if (timestamp - bt_result_buf[i].timestamp < BT_SCAN_MIN_PERIOD) {
			nr_latest++;
		}
	}
	return nr_latest;
}
