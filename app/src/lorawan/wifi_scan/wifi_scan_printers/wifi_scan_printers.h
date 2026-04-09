/**
 * @file wifi_scan_printers.h
 * @brief
 *
 * @copyright (c) 2023 Irnas. All rights reserved.
 *
 */

#ifndef WIFI_SCAN_PRINTERS_H
#define WIFI_SCAN_PRINTERS_H

#include "lr11xx_wifi.h"
#include <zephyr/kernel.h>

/**
 * @brief Print basic mac type channel results structure.
 *
 * @param[in] results
 * @param[in] nb_results
 */
void wifi_scan_print_basic_basic_mac_type_channel_results(
	lr11xx_wifi_basic_mac_type_channel_result_t *results, uint8_t nb_results);

/**
 * @brief Print basic complete results structure.
 *
 * @param[in] results
 * @param[in] nb_results
 */
void wifi_scan_print_basic_complete_results(lr11xx_wifi_basic_complete_result_t *results,
					    uint8_t nb_results);

/**
 * @brief Print basic full results structure.
 *
 * @param[in] results
 * @param[in] nb_results
 */
void wifi_scan_print_extended_full_results(lr11xx_wifi_extended_full_result_t *results,
					   uint8_t nb_results);

#endif /* WIFI_SCAN_PRINTERS_H */
