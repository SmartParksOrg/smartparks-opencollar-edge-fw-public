/**
 * @file gnss.h
 * @brief
 *
 * @copyright (c) 2023 Irnas. All rights reserved.
 *
 */

#ifndef GNSS_H
#define GNSS_H

#include <zephyr/kernel.h>

#include "lr11xx_gnss_types.h"

#define GNSS_TIMEOUT_S 10

/**
 * @brief External callback for gnss scan done event
 *
 */
typedef void (*gnss_results_handler_t)(int err);

/**
 * @brief Register external callback for nav message
 *
 */
void gnss_results_handler_register(gnss_results_handler_t);

/**
 * @brief Perform autonomous gnss scan.
 *
 * @param[in] context
 * @param[in] max_sv
 * @param[in] constellation
 */
void gnss_scan_autonomous(const void *context, uint8_t max_sv,
			  lr11xx_gnss_constellation_mask_t constellation);

/**
 * @brief Perform assisted gnss scan
 *
 * @param[in] context
 * @param[in] max_sv
 * @param[in] constellation
 */
void gnss_scan_assisted(const void *context, uint8_t max_sv,
			lr11xx_gnss_constellation_mask_t constellation);

/**
 * @brief Store GPS reference position fort next scan.
 *
 * @param[in] assistance_position
 */
void gnss_scan_set_ref_position(lr11xx_gnss_solver_assistance_position_t assistance_position);

/**
 * @brief Set GPS reference time for next scan.
 *
 * @param[in] gps_time
 */
void gnss_scan_set_ref_gps_time(uint32_t gps_time);

/**
 * @brief Get last nav data.
 *
 * @param[out] nav nav payload
 * @return uint8_t nav size
 */
uint8_t gnss_get_last_nav_data(uint8_t *nav);

/**
 * @brief Get last sat data.
 *
 * @param[in] sat satellite payload
 * @return uint8_t payload size
 */
uint8_t gnss_get_last_sat_data(uint8_t *sat, uint8_t *n_sat);

#endif /* GNSS_H */
