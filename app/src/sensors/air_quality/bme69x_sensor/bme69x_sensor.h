/** @file bme69x_sensor.h
 *
 * @brief BME69X sensor interface module.
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2025 Irnas.  All rights reserved.
 */

#ifndef BME69X_SENSOR_H
#define BME69X_SENSOR_H

#ifdef __cplusplus
extern "C" {
#endif

/* BME69X includes */
#include <bme69x.h>
#include <bme69x_defs.h>
#include <bme69x_types.h>
#include <bsec_interface.h>

#include <zephyr/kernel.h>

/**
 * @brief Get the latest BME69X sensor output.
 *
 * @param output Pointer to the output structure to fill.
 *
 * @retval 0 on success.
 * @retval -EAGAIN if no new data is available.
 */
int bsec_get_latest_output(bsec_output_t *out);

/**
 * @brief Setup BSEC library and BME69X sensor.
 *
 * @return BSEC_OK on success, negative error code on failure.
 */
bsec_library_return_t bsec_setup();

/**
 * @brief Get (BSEC library controlled) BME690 settings and set the BME690 sensor. If the library
 *        requests a measurement, a sensor task will be triggered.
 *
 * @param [out] sleep_timer is used by bsec to determine when to call bsec_sensor_control() again.
 */
int bsec_controller(int64_t *sleep_timer);

#ifdef __cplusplus
}
#endif

#endif /* BME69X_SENSOR_H */
