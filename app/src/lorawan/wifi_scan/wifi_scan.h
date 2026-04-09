/**
 * @file wifi_scan.h
 * @brief
 *
 * @copyright (c) 2023 Irnas. All rights reserved.
 *
 */

#ifndef WIFI_SCAN_H
#define WIFI_SCAN_H

#include "lr11xx_wifi.h"
#include <zephyr/kernel.h>

/**
 * @brief External callback for gnss scan done event
 *
 */
typedef void (*wifi_scan_results_handler_t)(int err);

/**
 * @brief Register external callback for wifi scan done event
 *
 */
void wifi_scan_results_handler_register(wifi_scan_results_handler_t);

/**
 * @brief Perform default wifi scan, using default Kconfig parameters.
 *
 * @param[in] context
 */
void wifi_scan_default(const void *context);

/* EvaTODO add custom scan option */

/**
 * @brief Store current scan results to wifi scan storage using provided timestamp.
 *
 * @param[in] timestamp
 */
void wifi_scan_store_results(uint32_t timestamp);

#endif /* WIFI_SCAN_H */
