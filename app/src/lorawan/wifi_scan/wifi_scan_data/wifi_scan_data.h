/** @file wifi_scan.h
 *
 * @brief Interface to process wifi scan results
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2023 Irnas. All rights reserved.
 */

#ifndef WIFI_SCAN_DATA_H
#define WIFI_SCAN_DATA_H

#include "lr11xx_wifi.h"
#include <zephyr/kernel.h>

#define WIFI_SCAN_STORE_MAX_RES            12
#define WIFI_SCAN_SEND_MAX_RES             3
#define WIFI_SCAN_MAC_ADDRESS_STORE_LENGTH 3

/**
 * @brief Single result structure
 *
 */
struct wifi_scan_result {
	int8_t rssi;
	lr11xx_wifi_mac_address_t mac_address;
	uint32_t timestamp;
	uint8_t counter;
	uint8_t signal_type;
};

struct wifi_scan_results {
	uint8_t nb_results;
	struct wifi_scan_result results[WIFI_SCAN_STORE_MAX_RES];
	uint32_t last_scan_time;
};

/**
 * @brief Add basic mac type channel results structure to storage result structure.
 *
 * @param[in] results
 * @param[in] nb_results
 */
void wifi_scan_data_add_basic_basic_mac_type_channel_results(
	lr11xx_wifi_basic_mac_type_channel_result_t *results, uint8_t nb_results,
	uint32_t timestamp);

/**
 * @brief Add basic complete results structure to storage result structure.
 *
 * @param[in] results
 * @param[in] nb_results
 */
void wifi_scan_data_add_basic_complete_results(lr11xx_wifi_basic_complete_result_t *results,
					       uint8_t nb_results, uint32_t timestamp);

/**
 * @brief Add basic full results structure to storage result structure.
 *
 * @param[in] results
 * @param[in] nb_results
 */
void wifi_scan_data_add_extended_full_results(lr11xx_wifi_extended_full_result_t *results,
					      uint8_t nb_results, uint32_t timestamp);

/**
 * @brief Compose payload for wifi scan results at latest scan.
 *
 * @param[in] payload pointer to payload buffer
 * @param[in] max_len max length of payload buffer
 * @return int payload length
 */
int compose_message_wifi_scan_results_single_scan(uint8_t *payload, uint8_t max_len);

/**
 * @brief Compose payload for wifi scan results.
 *
 * @param[in] payload pointer to payload buffer
 * @param[in] max_len max length of payload buffer
 * @param[in] max_res max number of results to send
 * @param[in] clear clear results after composing
 * @return int payload length
 */
int compose_message_wifi_scan_results(uint8_t *payload, uint8_t max_len, uint8_t max_res,
				      bool clear);

/**
 * @brief Return pointer to the result structure.
 *
 * @return struct wifi_scan_results*
 */
struct wifi_scan_results *wifi_scan_data_get_results(void);

/**
 * @brief Clear results structure.
 *
 */
void wifi_scan_data_clear_results(void);

#endif
