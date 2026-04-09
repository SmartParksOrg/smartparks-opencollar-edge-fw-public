/** @file temperature_sensor.h
 *
 * @brief Interface to temperature sensor.
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2022 Irnas. All rights reserved.
 */

#ifndef TEMP_SENSOR_H
#define TEMP_SENSOR_H

#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>

/*!
 * @brief Init the temperature sensor, get bindings form DT.
 *
 *
 * @return 0 if init ok, -err if not.
 */
int temperature_sensor_init(void);

/**
 * @brief Get temperature readings.
 *
 * @param temp sensor value structure to hold new data.
 * @return int 0 if ok, -err if not.
 */
int temperature_sensor_get_data(struct sensor_value *t_mes);

/*!
 * @brief Handle temperature sensor. Update val every defined interval.
 *
 */
int temperature_sensor_handle(void);

#endif // TEMP_SENSOR_H
