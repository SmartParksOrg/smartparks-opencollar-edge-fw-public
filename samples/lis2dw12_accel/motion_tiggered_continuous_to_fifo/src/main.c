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

#include <irnas_lis2dw12_types.h>

LOG_MODULE_REGISTER(lis2_cont_to_fifo_sample);

BUILD_ASSERT(DT_NODE_EXISTS(DT_NODELABEL(lis2dw12_accel)),
	     "lis2dw12 accelerometer is not defined in DT");

#define NUMBER_OF_SAMPLES_PER_MOTION_EVENT 50

const struct device *lis2dw12_sensor = DEVICE_DT_GET(DT_NODELABEL(lis2dw12_accel));

static int accel_data_buf_itr = 0;

/* We currently can not control the amount of samples we want to read from the FIFO buffer. All
 * available samples get read and saved into the buffer when read. If we have less space available
 * in the buffer than there are samples in the FIFO we could get buffer overflow. Therefore we
 * extended the buffer for the maximum amount of possible FIFO samples (LIS2DW12_FIFO_MAX_SAMPLES).
 *
 * TLDR: We add LIS2DW12_FIFO_MAX_SAMPLES (32) samples for each axis to avoid potential overflow.
 */
struct sensor_value
	accel_data_buf[(NUMBER_OF_SAMPLES_PER_MOTION_EVENT + LIS2DW12_FIFO_MAX_SAMPLES) * 3];

struct sensor_trigger prv_trig;
struct sensor_trigger prv_trig2;

static void prv_trigger_handler_motion(const struct device *dev, const struct sensor_trigger *trig);

/**
 * @brief Set the accelerometer's FIFO buffer mode.
 *
 * @param[in] mode The FIFO mode to set. This should be one of the lis2dw12_fmode_t enumerations.
 */
static void prv_set_fifo_mode(lis2dw12_fmode_t mode)
{
	/* Put FIFO into `bypass` mode
	 *
	 * The FIFO modes are (enumerated as specified in the datasheet):
	 *  0. LIS2DW12_BYPASS_MODE,
	 *  1. LIS2DW12_FIFO_MODE,
	 *  3. LIS2DW12_CONT_TO_FIFO_MODE,
	 *  4. LIS2DW12_BYPASS_TO_STREAM_MODE,
	 *  6. LIS2DW12_CONTINUOUS_MODE.
	 */
	struct sensor_value fifo_mode;
	fifo_mode.val1 = mode;
	fifo_mode.val2 = 0;

	int err = sensor_attr_set(lis2dw12_sensor, SENSOR_CHAN_ACCEL_XYZ,
				  (int)SENSOR_ATTR_IRNAS_LIS2DW12_FIFO_MODE, &fifo_mode);
	if (err < 0) {
		LOG_ERR("Failed to set FIFO mode: %d", err);
	} else {
		LOG_DBG("FIFO mode set to %d", fifo_mode.val1);
	}
}

/**
 * @brief Fetch and display the accelerometer data.
 *
 * @param sensor device structure (for the LIS2DW12 sensor).
 */
static void prv_fetch_and_display(const struct device *sensor)
{
	if (!sensor) {
		LOG_ERR("Failed to init LIS2DW12!");
	}
	int err = sensor_sample_fetch(sensor);
	if (!err) {
		err = sensor_channel_get(sensor, SENSOR_CHAN_ACCEL_XYZ, accel_data_buf);
	}
	if (err < 0) {
		LOG_ERR("ERROR: LIS2DW12 Update failed: %d", err);
	} else {
		LOG_INF("x: %d.%04d; y: %d.%04d; z: %d.%04d", accel_data_buf[0].val1,
			accel_data_buf[0].val2, accel_data_buf[1].val1, accel_data_buf[1].val2,
			accel_data_buf[2].val1, accel_data_buf[2].val2);
	}
}

/**
 * @brief print the first NUMBER_OF_SAMPLES_PER_MOTION_EVENT elements of the accel_data_buf
 *
 */
static void prv_print_buff(struct sensor_value *buf)
{
	LOG_INF("Accelerometer data: ");
	for (int i = 0; i < NUMBER_OF_SAMPLES_PER_MOTION_EVENT * 3; i += 3) {
		/* We use printk here for increased readability */
		LOG_PRINTK("x: %d.%04d; y: %d.%04d; z: %d.%04d\n", buf[i].val1, buf[i].val2,
			   buf[i + 1].val1, buf[i + 1].val2, buf[i + 2].val1, buf[i + 2].val2);

		k_sleep(K_MSEC(5)); /* Sleep so we don't drop logs */
	}
}

/**
 * @brief Fetch accelerometer data and fill the buffer with it. After enough data is
 * collected, reset fifo mode.
 *
 * Reset FIFO mode procedure:
 * 1. Set FIFO mode to `bypass` mode.
 * 2. Set FIFO mode to `bypass-to-continuous` mode.
 *
 * @param[in] sensor device structure (for the LIS2DW12 sensor).
 */
static void prv_read_from_fifo(const struct device *sensor)
{
	if (!sensor) {
		LOG_ERR("Failed to init LIS2DW12!");
	}

	int err = sensor_sample_fetch(sensor);
	if (!err) {
		struct sensor_value fifo_count;
		/* Read number of available samples */
		err = sensor_channel_get(sensor, (int)SENSOR_CHAN_IRNAS_LIS2DW12_FIFO_COUNT,
					 &fifo_count);
		if (err < 0) {
			LOG_ERR("Could not retrieve FIFO count from lis2dw12 driver: %d", err);
		} else {
			LOG_INF("FIFO count: %d", fifo_count.val1);
		}

		/* Read samples from FIFO */
		err = sensor_channel_get(sensor, (int)SENSOR_CHAN_IRNAS_ACCEL_XYZ_FIFO,
					 &accel_data_buf[accel_data_buf_itr]);
		if (err < 0) {
			LOG_ERR("Data retrieval failed: %d", err);
		} else {
			accel_data_buf_itr += fifo_count.val1 * 3;
		}

		/* Check if enough samples were read */
		if (accel_data_buf_itr >= NUMBER_OF_SAMPLES_PER_MOTION_EVENT * 3) {
			prv_print_buff(accel_data_buf);
			accel_data_buf_itr = 0;

			/* Set to bypass mode to reset FIFO buffer */
			prv_set_fifo_mode(LIS2DW12_BYPASS_MODE);
			prv_set_fifo_mode(LIS2DW12_BYPASS_TO_STREAM_MODE);
		}
	}
}

/**
 * @brief Handler for motion detection trigger.
 *
 * @param[in] dev interrupting device
 * @param[in] trig trigger structure
 */
static void prv_trigger_handler_motion(const struct device *dev, const struct sensor_trigger *trig)
{
	LOG_WRN("Motion detected!");
	prv_fetch_and_display(dev);
}

/**
 * @brief Handler for watermark detection trigger.
 *
 * @param[in] dev interrupting device
 * @param[in] trig trigger structure
 */
static void prv_trigger_handler_watermark(const struct device *dev,
					  const struct sensor_trigger *trig)
{
	LOG_WRN("Watermark detected!");
	prv_read_from_fifo(dev);
}

int main(void)
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

	struct sensor_value odr_attr;

	/* set LIS2DW12 accel sampling frequency to 12.5 Hz */
	odr_attr.val1 = 12;
	odr_attr.val2 = 0;

	int err;
	err = sensor_attr_set(lis2dw12_sensor, SENSOR_CHAN_ACCEL_XYZ,
			      SENSOR_ATTR_SAMPLING_FREQUENCY, &odr_attr);
	if (err < 0) {
		LOG_ERR("Cannot set sampling frequency for LIS2DW12 accel");
		return err;
	}

	/* Set FIFO to bypass-to-continuous mode */
	prv_set_fifo_mode(LIS2DW12_BYPASS_TO_STREAM_MODE);

	/* Set motion trigger */
	prv_trig.type = SENSOR_TRIG_DELTA;
	if (sensor_trigger_set(lis2dw12_sensor, &prv_trig, prv_trigger_handler_motion)) {
		LOG_ERR("Failed to set trigger!");
	}

	/* Set FIFO watermark trigger
	 * Note: The FIFO watermark level is configured in the board's respective .dts file.
	 */
	prv_trig2.type = (int)SENSOR_TRIG_IRNAS_FIFO_WATERMARK;
	prv_trig2.chan = SENSOR_CHAN_ACCEL_XYZ;
	if (sensor_trigger_set(lis2dw12_sensor, &prv_trig2, prv_trigger_handler_watermark)) {
		LOG_ERR("Failed to set trigger!");
	}

	return 0;
}
