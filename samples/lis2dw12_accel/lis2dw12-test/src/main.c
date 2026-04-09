/*
 * Copyright (c) 2020 Irnas d.o.o.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>

BUILD_ASSERT(DT_NODE_EXISTS(DT_NODELABEL(lis2dw12_accel)),
	     "lis2dw12 accelerometer is not defined in DT");

const struct device *lis2dw12_sensor = DEVICE_DT_GET(DT_NODELABEL(lis2dw12_accel));
struct sensor_value val[3];
struct sensor_trigger trig;
struct sensor_trigger trig2;

static void fetch_and_display(const struct device *sensor)
{
	if (!sensor) {
		printk("Failed to init LIS2DW12!\n");
	}
	int err = sensor_sample_fetch(sensor);
	if (!err) {
		err = sensor_channel_get(sensor, SENSOR_CHAN_ACCEL_XYZ, val);
	}
	if (val < 0) {
		printk("ERROR: LIS2DW12 Update failed: %d\n", err);
	} else {
		printk("x: %d.%04d; y: %d.%04d; z: %d.%04d\n", val[0].val1, val[0].val2,
		       val[1].val1, val[1].val2, val[2].val1, val[2].val2);
	}
}

#ifdef CONFIG_IRNAS_LIS2DW12_TRIGGER
static void trigger_handler(const struct device *dev, const struct sensor_trigger *trig)
{
	fetch_and_display(dev);
}
#endif

void main(void)
{
	if (!lis2dw12_sensor) {
		printk("Failed to init LIS2DW12!\n");
	} else {
		printk("Bind to LIS2DW12.\n");
	}

	if (!device_is_ready(lis2dw12_sensor)) {
		printk("Device %s is not ready\n", lis2dw12_sensor->name);
	} else {
		printk("Device %s is ready\n", lis2dw12_sensor->name);
	}

#if CONFIG_IRNAS_LIS2DW12_TRIGGER

	struct sensor_value odr_attr;

	/* set LIS2DW12 accel sampling frequency to 400 Hz */

	odr_attr.val1 = 100;
	odr_attr.val2 = 0;

	if (sensor_attr_set(lis2dw12_sensor, SENSOR_CHAN_ACCEL_XYZ, SENSOR_ATTR_SAMPLING_FREQUENCY,
			    &odr_attr) < 0) {
		printk("Cannot set sampling frequency for LIS2DW12 accel\n");
		return;
	}

#if CONFIG_IRNAS_LIS2DW12_MOTION_DETECTION
	printk("test T\n");
	trig.type = SENSOR_TRIG_DELTA;
	// trig.chan = SENSOR_CHAN_ACCEL_XYZ;
	if (sensor_trigger_set(lis2dw12_sensor, &trig, trigger_handler)) {
		printk("Failed to set trigger!");
	}

#endif // CONFIG_IRNAS_LIS2DW12_MOTION_DETECTION

#if CONFIG_IRNAS_LIS2DW12_FREE_FALL
	printk("test FF\n");

	trig.type = SENSOR_TRIG_FREEFALL;
	if (sensor_trigger_set(lis2dw12_sensor, &trig, trigger_handler)) {
		printk("Failed to set trigger!");
	}
#endif // CONFIG_IRNAS_LIS2DW12_FREE_FALL
#endif // CONFIG_IRNAS_LIS2DW12_TRIGGER

	while (1) {

		fetch_and_display(lis2dw12_sensor);
		k_msleep(100);
	}
}
