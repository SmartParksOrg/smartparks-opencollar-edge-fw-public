/** @file lis2dw12_sensor_fifo.c
 *
 * @brief LIS2DW12 sensor FIFO handling functions
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2025 Irnas. All rights reserved.
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "lis2dw12_reg.h"
#include <irnas_lis2dw12_types.h>
#include <lis2dw12_sensor.h>

#include <lis2dw12_sensor_fifo.h>

LOG_MODULE_REGISTER(LIS2DW12_FIFO);

#ifdef CONFIG_IRNAS_LIS2DW12_FIFO

extern const struct device *prv_lis2dw12_sensor;
extern bool lis2dw12_sensor_fifo_enabled;

void lis2dw12_sensor_set_fifo_mode(lis2dw12_fmode_t mode)
{
	/* Put FIFO into `bypass` mode
	 *
	 * The FIFO modes are (enumerated as specified in the datasheet):
	 *  0. Bypass,
	 *  1. FIFO,
	 *  3. Continuous to FIFO,
	 *  4. Bypass to Continuous,
	 *  6. Continuous.
	 */
	struct sensor_value fifo_mode;
	fifo_mode.val1 = mode;
	fifo_mode.val2 = 0;

	int err = sensor_attr_set(prv_lis2dw12_sensor, SENSOR_CHAN_ACCEL_XYZ,
				  (int)SENSOR_ATTR_IRNAS_LIS2DW12_FIFO_MODE, &fifo_mode);
	if (err < 0) {
		LOG_ERR("Failed to set FIFO mode: %d", err);
	} else {
		LOG_DBG("FIFO mode set to bypass");
	}
}

int lis2dw12_sensor_fifo_fetch(const struct device *sensor, struct sensor_value *buf,
			       size_t buf_len)
{
	if (!sensor) {
		LOG_ERR("LIS2DW12 in not enabled!");
		return -EINVAL;
	}
	if (buf == NULL) {
		LOG_ERR("Provided buffer is NULL.");
		return -EINVAL;
	}

	int num_of_available_values = 0;
	int num_of_read_values = 0;

	int err = sensor_sample_fetch(sensor);
	if (err) {
		LOG_ERR("Failed to fetch samples from sensor: %d", err);
		return err;
	}

	/* Read number of available samples. */
	struct sensor_value fifo_count;
	err = sensor_channel_get(sensor, (int)SENSOR_CHAN_IRNAS_LIS2DW12_FIFO_COUNT, &fifo_count);
	if (err < 0) {
		LOG_ERR("Could not retrieve FIFO count from lis2dw12 driver: %d", err);
		return err;
	}

	if (fifo_count.val1 == 0) {
		LOG_WRN("No samples available in FIFO buffer.");
		return -ENODATA;
	}

	LOG_DBG("FIFO count: %d", fifo_count.val1);
	/* Each sample has x,y and z values */
	num_of_available_values = fifo_count.val1 * 3;

	/* Check if there is enough space in the provided buffer */
	if (num_of_available_values > buf_len) {
		LOG_ERR("Not enough space in provided buffer to read all available FIFO "
			"samples.");
		return -ENOMEM;
	}

	/* Read samples from FIFO and save them into the provided buffer */
	err = sensor_channel_get(sensor, (int)SENSOR_CHAN_IRNAS_ACCEL_XYZ_FIFO, buf);
	if (err < 0) {
		LOG_ERR("Data retrieval failed: %d", err);
		return err;
	}

	num_of_read_values = num_of_available_values;

	return num_of_read_values;
}

int lis2dw12_sensor_fifo_check_mode(void)
{
	/* Get fifo mode */
	struct sensor_value fifo_mode;
	int err = sensor_attr_get(prv_lis2dw12_sensor, SENSOR_CHAN_ACCEL_XYZ,
				  (int)SENSOR_ATTR_IRNAS_LIS2DW12_FIFO_MODE, &fifo_mode);
	if (err < 0) {
		LOG_ERR("Failed to get FIFO mode: %d", err);
		return err;
	} else {
		/* Check if FIFO is in bypass mode */
		if (fifo_mode.val1 != LIS2DW12_BYPASS_TO_STREAM_MODE) {
			/* Set FIFO mode to bypass-to-continuous */
			lis2dw12_sensor_set_fifo_mode(LIS2DW12_BYPASS_TO_STREAM_MODE);
		}
	}

	return 0;
}

int lis2dw12_fifo_watermark_trigger_set(const struct device *dev, sensor_trigger_handler_t handler)
{
	struct sensor_trigger trig_fifo;
	trig_fifo.chan = SENSOR_CHAN_ACCEL_XYZ;
	trig_fifo.type = (int)SENSOR_TRIG_IRNAS_FIFO_WATERMARK;
	int err = sensor_trigger_set(dev, &trig_fifo, handler);
	if (err) {
		LOG_ERR("Failed to set FIFO trigger, err: %d", err);
	}
	return err;
}

#endif /* CONFIG_IRNAS_LIS2DW12_FIFO */
