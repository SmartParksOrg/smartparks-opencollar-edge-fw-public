/** @file temp_sensor.h
 *
 * @brief Interface to temperature sensor.
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2022 Irnas. All rights reserved.
 */

#include <string.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "definitions.h"
#include "generated_settings.h"

#include "temperature_sensor.h"

LOG_MODULE_REGISTER(TEMP_SENSOR, 3);

// Temp
#ifdef CONFIG_TEMP_NRF5
static const struct device *temp_dev = DEVICE_DT_GET_ANY(nordic_nrf_temp);
#else
static const struct device *temp_dev;
#endif

struct sensor_value temp;
uint64_t last_temp_read = 0; // Last time temperature was read

/*!
 * @brief Get temperature readings and update values in internal values structure.
 *
 *
 * @return 0 if init ok, -err if not.
 */
static int temperature_sensor_update_data(void)
{
	// Check if device binding
	if (!temp_dev) {
		return -ENXIO;
	}

	int err = sensor_sample_fetch(temp_dev);
	if (err) {
		return err;
	}

	err = sensor_channel_get(temp_dev, SENSOR_CHAN_DIE_TEMP, &temp);
	if (err) {
		return err;
	}

	LOG_INF("temp: %d.%04d", temp.val1, temp.val2 / 100);
	// Store to values struct
	Main_values.mcu_temp->def_val[0] = (int16_t)(temp.val1);
	Main_values.mcu_temp->def_val[1] = (int16_t)(temp.val2 / 100);

	return 0;
}

int temperature_sensor_init(void)
{
	if (temp_dev == NULL) {
		LOG_ERR("No device TEMP_0 found; did initialization fail?");
		return -ENXIO;
	} else {
		LOG_INF("Found device TEMP_0");
		if (!device_is_ready(temp_dev)) {
			LOG_ERR("Temperature device not ready!");
			temp_dev = NULL;
			return -ENXIO;
		}
		return 0;
	}
}

int temperature_sensor_get_data(struct sensor_value *t_mes)
{
	// Check if device binding
	if (!temp_dev) {
		return -ENXIO;
	}

	int err = sensor_sample_fetch(temp_dev);
	if (err) {
		return err;
	}

	err = sensor_channel_get(temp_dev, SENSOR_CHAN_DIE_TEMP, t_mes);
	if (err) {
		return err;
	}

	return 0;
}

/* NOTE: approach with worker and timeout is implemented as
timeout is not handled in nrf5_temp sensor driver - sensor_sample_fetch()
indefinitely waits for data and never exists. To avoid sensor thread timeout and
wtd reboot we use this approach */

static void temperature_timeout_handler(struct k_timer *dummy)
{
	// Data timeout indicates temperature sensor is not working
	LOG_ERR("Temperature sensor timeout, disable sensor!");
	temp_dev = NULL;
	// Store to values struct
	Main_values.mcu_temp->def_val[0] = 0;
	Main_values.mcu_temp->def_val[1] = 0;
}

// Define timer to timeout temperature sensor
K_TIMER_DEFINE(temperature_timer, temperature_timeout_handler, NULL);

static void temperature_work_handler(struct k_work *work)
{
	temperature_sensor_update_data();
	k_timer_stop(&temperature_timer);
}

/* Define work handler */
K_WORK_DEFINE(temperature_work, temperature_work_handler);

int temperature_sensor_handle(void)
{
	// Check if device binding
	if (!temp_dev) {
		LOG_ERR("Temperature sensor not working!");
		return -ENXIO;
	}

	int err = 0;
	if ((uint32_t)((k_uptime_get() - last_temp_read)) > TEMP_READ_INTERVAL ||
	    last_temp_read == 0) {
		k_timer_start(&temperature_timer, K_SECONDS(5), K_NO_WAIT);
		k_work_submit(&temperature_work);
		last_temp_read = k_uptime_get();
	}

	return err;
}
