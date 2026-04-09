/** @file wifi_scan.c
 *
 * @brief Interface to process wifi scan results
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2023 Irnas. All rights reserved.
 */

#include "wifi_scan_data.h"

#include "lr11xx_board.h"
#include "lr11xx_system.h"
#include "lr11xx_wifi.h"

#include <zephyr/kernel.h>
#include <zephyr/types.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(wifi_scan_data);

#define SINGLE_RESULT_LEN WIFI_SCAN_MAC_ADDRESS_STORE_LENGTH + 6 // Length of single result in bytes

static struct wifi_scan_results prv_results;

/**
 * @brief Check if MAC address is already in buffer
 *
 * @param[in] mac_address mac address to check
 * @retval -1 if not found
 * @retval >=0 index of match if found
 */
static int prv_find_mac_match(lr11xx_wifi_mac_address_t mac_address)
{
	/* Loop over storage buffer */
	bool match = false;
	for (uint8_t i = 0; i < prv_results.nb_results; i++) {
		match = true;
		for (uint8_t j = 0; j < LR11XX_WIFI_MAC_ADDRESS_LENGTH; j++) {
			if (mac_address[j] != prv_results.results[i].mac_address[j]) {
				match = false;
				break; // Exit loop
			}
		}
		if (match) {
			return i; // Return match position
		}
	}

	return -1;
}

/**
 * @brief Update results for existing MAC entry and sort buffer so that latest result is first.
 *
 * @param[in] idx existing result index
 * @param[in] rssi new rssi value
 * @param[in] timestamp new timestamp
 * @retval -EINVAL if rssi is invalid or idx is out of bounds
 * @retval 0 if ok
 */
static int prv_add_existing_result(uint8_t idx, int8_t rssi, uint32_t timestamp)
{
	if (idx >= prv_results.nb_results) {
		return -EINVAL;
	}

	if (rssi < 0) {
		/* Adjust rssi average */
		float new_rssi = (float)prv_results.results[idx].rssi *
					 (((float)prv_results.results[idx].counter - 1) /
					  (float)prv_results.results[idx].counter) +
				 (float)rssi / prv_results.results[idx].counter;
		prv_results.results[idx].rssi = (int8_t)new_rssi;
	} else {
		return -EINVAL;
	}

	/* Update timestamp */
	prv_results.results[idx].timestamp = timestamp;

	/* Increase counter */
	prv_results.results[idx].counter++;

	/* Sort buffer */
	struct wifi_scan_result tmp = prv_results.results[idx];
	for (uint8_t i = idx; i > 0; i--) {
		prv_results.results[i] = prv_results.results[i - 1];
	}
	prv_results.results[0] = tmp;

	return 0;
}

/**
 * @brief Add new result to buffer.
 *
 * @param[in] rssi new rssi value
 * @param[in] info_byte new info byte
 * @param[in] mac new mac address
 * @param[in] timestamp new timestamp
 * @retval -EINVAL if rssi is invalid
 * @retval 0 if ok
 */
static int prv_add_new_result(int8_t rssi, uint8_t type, lr11xx_wifi_mac_address_t mac,
			      uint32_t timestamp)
{
	if (rssi >= 0) {
		return -EINVAL;
	}

	/* Adjust number of stored results */
	prv_results.nb_results++;
	if (prv_results.nb_results > WIFI_SCAN_STORE_MAX_RES) {
		prv_results.nb_results = WIFI_SCAN_STORE_MAX_RES;
	}

	/* Move results */
	for (uint8_t i = prv_results.nb_results - 1; i > 0; i--) {
		prv_results.results[i] = prv_results.results[i - 1];
	}

	/* Add new entry */
	prv_results.results[0].counter = 1;
	prv_results.results[0].rssi = rssi;
	prv_results.results[0].timestamp = timestamp;
	prv_results.results[0].signal_type = type;
	for (uint8_t i = 0; i < LR11XX_WIFI_MAC_ADDRESS_LENGTH; i++) {
		prv_results.results[0].mac_address[i] = mac[i];
	}

	return 0;
}

/**
 * @brief Sort result buffer by count.
 *
 */
static void prv_sort_wifi_scan_results_by_count(void)
{
	struct wifi_scan_result tmp;
	uint8_t max_count = 0;
	uint8_t max_idx = 0;

	/* Sort */
	for (uint8_t i = 0; i < prv_results.nb_results; i++) {
		/* Find new max */
		max_count = prv_results.results[i].counter;
		max_idx = i;
		for (uint8_t j = i + 1; j < prv_results.nb_results; j++) {
			if (prv_results.results[j].counter > max_count) {
				max_count = prv_results.results[j].counter;
				max_idx = j;
			}
		}
		/* Move up */
		if (max_idx > i) {
			tmp = prv_results.results[max_idx];
			for (uint8_t j = max_idx; j > i; j--) {
				prv_results.results[j] = prv_results.results[j - 1];
			}
			prv_results.results[i] = tmp;
		}
	}
}

void wifi_scan_data_add_basic_basic_mac_type_channel_results(
	lr11xx_wifi_basic_mac_type_channel_result_t *results, uint8_t nb_results,
	uint32_t timestamp)
{
	prv_results.last_scan_time = timestamp;
	/* Loop over results */
	for (uint8_t i = 0; i < nb_results; i++) {
		/* Check if we already have this MAC address */
		int match_idx = prv_find_mac_match(results[i].mac_address);
		/* If MAC is already in buffer increase counter and reorder structure */
		if (match_idx >= 0) {
			prv_add_existing_result(match_idx, results[i].rssi, timestamp);
		}
		/* Otherwise add new entry */
		else {
			prv_add_new_result(results[i].rssi,
					   lr11xx_wifi_extract_signal_type_from_data_rate_info(
						   results[i].data_rate_info_byte),
					   results[i].mac_address, timestamp);
		}
	}
}

void wifi_scan_data_add_basic_complete_results(lr11xx_wifi_basic_complete_result_t *results,
					       uint8_t nb_results, uint32_t timestamp)
{
	prv_results.last_scan_time = timestamp;
	/* Loop over results */
	for (uint8_t i = 0; i < nb_results; i++) {
		/* Check if we already have this MAC address */
		int match_idx = prv_find_mac_match(results[i].mac_address);
		/* If MAC is already in buffer increase counter and reorder structure */
		if (match_idx >= 0) {
			prv_add_existing_result(match_idx, results[i].rssi, timestamp);
		}
		/* Otherwise add new entry */
		else {
			prv_add_new_result(results[i].rssi,
					   lr11xx_wifi_extract_signal_type_from_data_rate_info(
						   results[i].data_rate_info_byte),
					   results[i].mac_address, timestamp);
		}
	}
}

void wifi_scan_data_add_extended_full_results(lr11xx_wifi_extended_full_result_t *results,
					      uint8_t nb_results, uint32_t timestamp)
{
	prv_results.last_scan_time = timestamp;
	/* Loop over results */
	for (uint8_t i = 0; i < nb_results; i++) {
		/* Check if we already have this MAC address */
		int match_idx = prv_find_mac_match(results[i].mac_address_1);
		/* If MAC is already in buffer increase counter and reorder structure */
		if (match_idx >= 0) {
			prv_add_existing_result(match_idx, results[i].rssi, timestamp);
		}
		/* Otherwise add new entry */
		else {
			prv_add_new_result(results[i].rssi,
					   lr11xx_wifi_extract_signal_type_from_data_rate_info(
						   results[i].data_rate_info_byte),
					   results[i].mac_address_1, timestamp);
		}
	}
}

int compose_message_wifi_scan_results_single_scan(uint8_t *payload, uint8_t max_len)
{
	uint8_t nr_latest = 0; /* nr of results in latest scan */
	uint8_t idx = 1;       /* payload index */

	/* Loop over all results */
	for (uint8_t i = 0; i < prv_results.nb_results; i++) {
		/* Check if we have space */
		if (idx + SINGLE_RESULT_LEN < max_len) {
			/* Check if correct timestamp */
			if (prv_results.results[i].timestamp == prv_results.last_scan_time) {
				/* MAC */
				for (uint8_t j = 0; j < WIFI_SCAN_MAC_ADDRESS_STORE_LENGTH; j++) {
					payload[idx + j] = prv_results.results[i].mac_address[j];
				}
				idx += WIFI_SCAN_MAC_ADDRESS_STORE_LENGTH;
				/* signal */
				payload[idx] = (uint8_t)(prv_results.results[i].rssi + 128);
				idx++;
				/* counter */
				payload[idx] = 1;
				idx++;
				/* timestamp */
				memcpy(payload + idx, &prv_results.results[i].timestamp,
				       sizeof(prv_results.results[i].timestamp));
				idx += sizeof(prv_results.results[i].timestamp);

				nr_latest += 1;
			}
		}
	}

	/* First byte is nr of results */
	payload[0] = nr_latest;

	return idx;
}

int compose_message_wifi_scan_results(uint8_t *payload, uint8_t max_len, uint8_t max_res,
				      bool clear)
{
	uint8_t idx = 0; /* payload index */

	/* First byte is nr of results */
	payload[0] = prv_results.nb_results;
	idx++;

	/* Sort results according to count nr. */
	prv_sort_wifi_scan_results_by_count();

	/* Loop over results */
	for (uint8_t i = 0; i < prv_results.nb_results; i++) {
		/* Check if we have space */
		if (idx + SINGLE_RESULT_LEN < max_len && i < max_res) {
			/* MAC */
			for (uint8_t j = 0; j < WIFI_SCAN_MAC_ADDRESS_STORE_LENGTH; j++) {
				payload[idx + j] = prv_results.results[i].mac_address[j];
			}
			idx += WIFI_SCAN_MAC_ADDRESS_STORE_LENGTH;
			/* signal */
			payload[idx] = (uint8_t)(prv_results.results[i].rssi + 128);
			idx++;
			/* counter */
			payload[idx] = prv_results.results[i].counter;
			idx++;
			/* timestamp */
			memcpy(payload + idx, &prv_results.results[i].timestamp,
			       sizeof(prv_results.results[i].timestamp));
			idx += sizeof(prv_results.results[i].timestamp);
		}
	}

	/* Reset results buffer if specified */
	memset(&prv_results, 0, sizeof(struct wifi_scan_results));

	return idx;
}

struct wifi_scan_results *wifi_scan_data_get_results(void)
{
	return &prv_results;
}

void wifi_scan_data_clear_results(void)
{
	memset(&prv_results, 0, sizeof(struct wifi_scan_results));
}
