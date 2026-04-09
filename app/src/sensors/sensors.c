/** @file sensors.c
 *
 * @brief Interface to sensor drives, accelerometer, GPS
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2020 Irnas. All rights reserved.
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#ifdef CONFIG_LIS2DW12_SENSOR
#include "lis2dw12_sensor.h"
#endif

#ifdef CONFIG_AIR_QUALITY
#include <air_quality.h>
#endif /* CONFIG_AIR_QUALITY */

#include "temperature_sensor.h"

#include "definitions.h"
#include "generated_settings.h"
#include "status.h"
#include "thread_com.h"

#include "sensors.h"

#define CONFIG_LOG_SENSORS_LEVEL 3

LOG_MODULE_REGISTER(SENSORS, CONFIG_LOG_SENSORS_LEVEL);

// Sensor values
struct sensor_value lis2dw12_accel_val[3];

uint64_t last_sensors_read = 0; // Last time temperature was read

uint8_t sensors_buf[MAX_BUF_SIZE];

/* Check read period */
static bool sensors_check_read_period(void)
{
	if ((uint32_t)(k_uptime_get() - last_sensors_read) > SENSORS_READ_INTERVAL ||
	    last_sensors_read == 0) {
		last_sensors_read = k_uptime_get();
		return true;
	}
	return false;
}

/*!
 * @brief Init all available sensors.
 *
 *
 * @return negative error code if one of the sensors has failed, 0 if all successful.
 */
int sensors_init(void)
{
	int err = 0;
	bool accel_available = false; // Do we have at least one accel functional?

	// LIS2DW12
#ifdef CONFIG_LIS2DW12_SENSOR
	err = lis2dw12_sensor_init();
	if (err) {
		LOG_ERR("Failed to init lis2dw12 sensor.");
	} else {
		accel_available = true;
	}
#else
	LOG_INF("lis2dw12 sensor not supported on tracker");
#endif // CONFIG_LIS2DW12_SENSOR

	// Check if we have any functional accelerometer
	if (!accel_available) {
		sys_err.acc = -EIO;
	}

#ifdef CONFIG_AIR_QUALITY
	err = air_quality_init();
	if (err) {
		LOG_ERR("Failed to init air quality sensors.");
		return err;
	}
#endif /* CONFIG_AIR_QUALITY */

	return err;
}

/*!
 * @brief Set all sensors to power down mode and disable interrupts.
 *
 *
 * @return negative error code, 0 is successful.
 */
void sensors_disable(void)
{
	// ToDo
}

/*!
 * @brief Read all available sensors.
 *
 *
 * @return negative error code, 0 is successful.
 */
int sensors_read(void)
{
	int err = 0;
	bool accel_read = false; // Check if we had any successful accel read

	// LIS2DW12
#if DT_NODE_EXISTS(DT_NODELABEL(lis2dw12_accel))
	err = sensors_lis2dw12_read();
	if (!err) {
		accel_read = true;
	}
#endif

	if (!accel_read) {
		LOG_ERR("Failed to read any accelerometer!");
		sys_err.acc = -EIO;
	} else {
		LOG_INF("Accelerometer read successful");
		sys_err.acc = 0;
	}

	return err;
}

/*!
 * @brief Read lis2dw12 sensor.
 *
 *
 * @return negative error code, 0 is successful.
 */
int sensors_lis2dw12_read(void)
{
	int err = 0;

#ifdef CONFIG_LIS2DW12_SENSOR

	err = lis2dw12_sensor_get_data(lis2dw12_accel_val);

	if (!err) {
		// Store to values struct
		Main_values.lis2_acc_x->def_val[0] = (int16_t)(lis2dw12_accel_val[0].val1);
		Main_values.lis2_acc_x->def_val[1] = (int16_t)(lis2dw12_accel_val[0].val2 / 100);
		Main_values.lis2_acc_y->def_val[0] = (int16_t)(lis2dw12_accel_val[1].val1);
		Main_values.lis2_acc_y->def_val[1] = (int16_t)(lis2dw12_accel_val[1].val2 / 100);
		Main_values.lis2_acc_z->def_val[0] = (int16_t)(lis2dw12_accel_val[2].val1);
		Main_values.lis2_acc_z->def_val[1] = (int16_t)(lis2dw12_accel_val[2].val2 / 100);
		LOG_INF("LIS2DW x: %d.%04d; y: %d.%04d; z: %d.%04d",
			Main_values.lis2_acc_x->def_val[0], Main_values.lis2_acc_x->def_val[1],
			Main_values.lis2_acc_y->def_val[0], Main_values.lis2_acc_y->def_val[1],
			Main_values.lis2_acc_z->def_val[0], Main_values.lis2_acc_z->def_val[1]);
	} else if (err == -EBUSY) {
		LOG_ERR("Sensor was busy. Try again later!");
		err = 0;
	}

#endif // CONFIG_LIS2DW12_SENSOR

	return err;
}

/* Handle all sensors */
void sensors_handle(void)
{
	if (sensors_check_read_period()) {
		LOG_INF("Read all sensors!");
		sensors_read();
	}

	sensors_update_settings();

#ifdef CONFIG_AIR_QUALITY
	int err = air_quality_handle();
	if (err) {
		LOG_ERR("Air quality handle read failed.");
	}
#endif /* CONFIG_AIR_QUALITY */
}

/* Check if any setting was updated */
void sensors_update_settings(void)
{
#ifdef CONFIG_LIS2DW12_SENSOR
	lis2dw12_sensor_update_settings();
#endif // CONFIG_LIS2DW12_SENSOR
}

void sensors_handle_commands(void)
{
	// Check if new message
	mb_msg_dest msg_origin;
	mb_msg_action msg_action;
	uint8_t msg_port;
	uint8_t msg_max_rsp_len = 0;

	// Forever wait for new message
	int msg_size = thread_get_sensors(&msg_origin, &msg_action, &msg_port, sensors_buf,
					  &msg_max_rsp_len);
	if (msg_size > 0) {
		LOG_INF("Got msg of len: %d and id: %d in sensors thread.", msg_size,
			sensors_buf[0]);
		if (msg_action == MB_MSG_EXECUTE) {
			// Not implemented yet ...
		} else {
			LOG_WRN("Other message types not supported in sensor thread!");
		}
	}
}

/*** end of file ***/
