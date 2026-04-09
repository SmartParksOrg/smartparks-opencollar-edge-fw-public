/** @file air_quality.h
 *
 * @brief Air quality measurement module.
 *
 * This module joins the BME680 and BMV080 sensors to provide air quality measurements.
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2025 Irnas.  All rights reserved.
 */

#ifndef AIR_QUALITY_H
#define AIR_QUALITY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <bme69x_sensor.h>
#include <bmv080_sensor.h>

#include <zephyr/kernel.h>

/**
 * @brief Initialize air quality sensors (BME690 and BMV080).
 *
 * @return 0 on success, negative error code on failure.
 */
int air_quality_init(void);

/**
 * @brief get pointers to the latest air quality data if available.
 *
 * @param bmv080_data Pointer to store the address of the latest BMV080 data.
 * @param bme690_data Pointer to store the address of the latest BME690 data.
 */
void air_quality_get_data_pointers(bmv080_output_t **bmv080_data, bsec_output_t **bme690_data);

/**
 * @brief Handle reading data from air quality sensors.
 *
 * This function is responsible for fetching and processing data from the air quality
 * sensors. The BME690 sensor is managed by the BSEC library, which specifies when to read
 * data.
 *
 * @return 0 on success, negative error code on failure.
 */
int air_quality_handle(void);

#ifdef __cplusplus
}
#endif

#endif /* AIR_QUALITY_H */
