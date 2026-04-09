/** @file bmv080_sensor.c
 *
 * @brief BMV080 sensor interface module.
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2025 Irnas. All rights reserved.
 */

#include <zephyr/logging/log.h>

#include <bmv080_irnas.h>
#include <bmv080_types.h>

#include <bmv080_sensor.h>

LOG_MODULE_REGISTER(bmv080_sensor);

BUILD_ASSERT(DT_NODE_EXISTS(DT_NODELABEL(bmv080)), "bmv080 not defined in DT");

static bool prv_data_available = false;
static bmv080_output_t prv_latest_output;
static struct sensor_value prv_obstruction;

/**
 * @brief Callback function called when new BMV080 data is available.
 *
 * @param output The latest sensor output data.
 * @param user_data User data pointer (not used).
 */
static void prv_bmv080_sensor_data_available_callback(bmv080_output_t output, void *user_data)
{
	prv_latest_output = output;
	prv_data_available = true;

	/* Print all values is output */
	LOG_INF("BMV080 new data available:\n"
		"  PM1.0 mass concentration: %.2f µg/m³\n"
		"  PM2.5 mass concentration: %.2f µg/m³\n"
		"  PM10 mass concentration: %.2f µg/m³\n"
		"  Number concentration (1.0µm): %.2f #/cm³\n"
		"  Number concentration (2.5µm): %.2f #/cm³\n"
		"  Number concentration (10µm): %.2f #/cm³\n"
		"  Is obstructed: %s",
		output.pm1_mass_concentration, output.pm2_5_mass_concentration,
		output.pm10_mass_concentration, output.pm1_number_concentration,
		output.pm2_5_number_concentration, output.pm10_number_concentration,
		output.is_obstructed ? "yes" : "no");
}

static struct k_work bmv080_work;

/**
 * @brief Work handler to fetch BMV080 data.
 *
 * @param work Pointer to the work structure.
 */
static void bmv080_work_handler(struct k_work *work)
{
	bmv080_sensor_fetch(DEVICE_DT_GET(DT_NODELABEL(bmv080)));

	sensor_channel_get(DEVICE_DT_GET(DT_NODELABEL(bmv080)), (int)SENSOR_CHAN_BMV080_OBSTRUCTION,
			   &prv_obstruction);
}

/**
 * @brief Timer handler to periodically submit work for fetching BMV080 data.
 *
 * @param timer_id Pointer to the timer structure.
 */
static void bmv080_timer_handler(struct k_timer *timer_id)
{
	k_work_submit(&bmv080_work);
}

K_TIMER_DEFINE(bmv080_timer, bmv080_timer_handler, NULL);

void bmv080_sensor_fetch(const struct device *bmv080_dev)
{
	int ret = sensor_sample_fetch(bmv080_dev);
	if (ret) {
		LOG_ERR("Failed to fetch sample: %d", ret);
	}
}

int bmv080_sensor_get_latest_output(bmv080_output_t *output)
{
	if (prv_data_available) {
		memcpy(output, &prv_latest_output, sizeof(bmv080_output_t));
		prv_data_available = false;
		return 0;
	}
	if (!prv_data_available && prv_obstruction.val1 == true) {
		static bmv080_output_t empty_output;
		empty_output.is_obstructed = true;
		memcpy(output, &empty_output, sizeof(bmv080_output_t));
		return 0;
	}
	return -ENODATA;
}

int bmv080_sensor_init(void)
{
	const struct device *bmv080_dev = DEVICE_DT_GET(DT_NODELABEL(bmv080));
	if (!bmv080_dev) {
		LOG_ERR("BMV080 device not found");
		return -EIO;
	}

	if (!device_is_ready(bmv080_dev)) {
		LOG_ERR("Device %s is not ready", bmv080_dev->name);
		return -EIO;
	} else {
		LOG_INF("Device %s is ready", bmv080_dev->name);
	}

	/* Register custom callback */
	irnasBMV080_set_data_ready_callback(prv_bmv080_sensor_data_available_callback);

	/* Parameters: Measurement algorithm */
	bmv080_set_parameter(irnasBMV080_get_handle(bmv080_dev), "measurement_algorithm",
			     (void *)E_BMV080_MEASUREMENT_ALGORITHM_FAST_RESPONSE);

	/* Parameters: Obstruction detection */
	bmv080_set_parameter(irnasBMV080_get_handle(bmv080_dev), "do_obstruction_detection",
			     (void *)true);

	/* Parameters: Integration time - Sensor ON time (part of duty cycle period) */
	float integration_time_s = 10.0f;
	bmv080_set_parameter(irnasBMV080_get_handle(bmv080_dev), "integration_time",
			     (void *)&integration_time_s);

	/* Parameters: Duty cycling period - Total period of measurement cycle */
	int duty_cycling_period_s = 300;
	bmv080_set_parameter(irnasBMV080_get_handle(bmv080_dev), "duty_cycling_period",
			     (void *)&duty_cycling_period_s);

	bmv080_start_duty_cycling_measurement(irnasBMV080_get_handle(bmv080_dev),
					      (bmv080_callback_tick_t)((uint32_t)k_uptime_get),
					      E_BMV080_DUTY_CYCLING_MODE_0);

	k_work_init(&bmv080_work, bmv080_work_handler);

	/* The bmv080 Bosch SDK expects the `bmv080_serve_interrupt` function (the zephyr
	 * sensor_sample_fetch does just that) to be called at least once per second, with 1.0
	 * second being too long (based on testing). 999 ms seems to work well. */
	k_timer_start(&bmv080_timer, K_MSEC(999), K_MSEC(999));

	return 0;
}
