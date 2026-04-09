/*
 * Copyright (c) 2020 Irnas d.o.o.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* NOTE: Current settings are not optima, experiment for double tap... */

#include <string.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "lis2dw12_reg.h"

LOG_MODULE_REGISTER(lis2_temperature_sample);

BUILD_ASSERT(DT_NODE_EXISTS(DT_NODELABEL(lis2dw12_accel)),
	     "lis2dw12 accelerometer is not defined in DT");

const struct device *lis2dw12_sensor = DEVICE_DT_GET(DT_NODELABEL(lis2dw12_accel));

/**
 * @brief Fetch and display the accelerometer data.
 *
 * @param sensor device structure (for the LIS2DW12 sensor).
 */
static void fetch_and_display(const struct device *sensor)
{
	struct sensor_value val;

	if (!sensor) {
		LOG_ERR("Failed to init LIS2DW12!");
		return;
	}

	int err = sensor_sample_fetch(sensor);
	if (err) {
		LOG_ERR("Failed to fetch data from the sensor, err: %d", err);
		return;
	}

	err = sensor_channel_get(sensor, SENSOR_CHAN_AMBIENT_TEMP, &val);
	if (err < 0) {
		LOG_ERR("Failed to get temperature data from the sensor, err: %d", err);
		return;
	}

	LOG_INF("Temperature: %d.%04d C", val.val1, val.val2);
}

void main(void)
{
	if (!lis2dw12_sensor) {
		LOG_ERR("Failed to init LIS2DW12!");
	} else {
		LOG_INF("Bind to LIS2DW12.");
	}

	if (!device_is_ready(lis2dw12_sensor)) {
		LOG_ERR("Device %s is not ready", lis2dw12_sensor->name);
	} else {
		LOG_INF("Device %s is ready", lis2dw12_sensor->name);
	}

	while (1) {
		k_msleep(1000);
		fetch_and_display(lis2dw12_sensor);
	}
}
