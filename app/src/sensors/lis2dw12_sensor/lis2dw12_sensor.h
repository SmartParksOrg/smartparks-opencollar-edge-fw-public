/** @file lis2dw12_sensors.h
 *
 * @brief Interface to sensor drives, accelerometer, GPS
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2020 Irnas. All rights reserved.
 */

#ifndef LIS2DW12_SENSOR_H
#define LIS2DW12_SENSOR_H

#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>

#include "common_functions.h"
#include "generated_settings.h"

#ifdef CONFIG_IRNAS_LIS2DW12_FIFO

#define NUMBER_OF_SAMPLES_PER_MOTION_EVENT 50

#endif /* CONFIG_IRNAS_LIS2DW12_FIFO */

/*!
 * @brief Init the accelerometer sensor, get bindings form DT.
 *
 *
 * @return negative int error, or 0 if ok.
 */
int lis2dw12_sensor_init(void);

/**
 * @brief Fetch and display sensor data.
 *
 * @param[out] val - sensor value structure to store data.
 * @return int 0 or negative error code.
 */
int lis2dw12_sensor_get_data(struct sensor_value *val);

/**
 * @brief Check if any setting was updated in general settings by user or other threads.
 *
 * @return int 0 or error code
 */
int lis2dw12_sensor_update_settings(void);

#endif // LIS2DW12_SENSOR_H
