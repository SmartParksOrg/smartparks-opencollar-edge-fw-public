/** @file bmv080_sensor.h
 *
 * @brief BMV080 sensor interface module.
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2025 Irnas.  All rights reserved.
 */

#ifndef BMV080_SENSOR_H
#define BMV080_SENSOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <bmv080_irnas.h>

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>

/**
 * @brief Fetch BMV080 sensor data.
 *
 * NOTE: this function needs to be called at least once per second to ensure proper data retrieval.
 * When new data is available a callback will be called if registered. Regardless of callback
 * registration, the latest data can be accessed via sensor_channel_get.
 *
 * @param bmv080_dev Pointer to the BMV080 sensor device.
 */
void bmv080_sensor_fetch(const struct device *bmv080_dev);

/**
 * @brief Get the latest BMV080 sensor output.
 *
 * @param output Pointer to the output structure to fill.
 *
 * @retval 0 on success.
 * @retval -EAGAIN if no new data is available.
 */
int bmv080_sensor_get_latest_output(bmv080_output_t *output);

/**
 * @brief Initialize the BMV080 sensor sampling mode.
 *
 * @return 0 on success, negative error code on failure.
 */
int bmv080_sensor_init(void);

#ifdef __cplusplus
}
#endif

#endif /* BMV080_SENSOR_H */
