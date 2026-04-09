/** @file air_quality.c
 *
 * @brief Air quality measurement module.
 *
 * This module joins the BME680 and BMV080 sensors to provide air quality measurements.
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2025 Irnas. All rights reserved.
 */

#include <zephyr/logging/log.h>

#include <air_quality.h>

LOG_MODULE_REGISTER(air_quality);

#define AIR_QUALITY_STARTUP_DELAY_S 30

/* BME69X BSEC library controlled sleep timer
 * This is updated by bsec_controller() and used to determine when to call
 * bsec_controller() again.
 */
int64_t prv_bme69x_BSEC_sleep_timer = 0;

bmv080_output_t prv_bmv080_latest_output;
bool prv_bmv080_send_data = false;

bsec_output_t prv_bme690_latest_outputs[8];
bool prv_bme690_send_data = false;

/**
 * @brief Get available data from sensors.
 */
static void prv_get_available_data(void)
{
	if (bmv080_sensor_get_latest_output(&prv_bmv080_latest_output) == 0) {
		prv_bmv080_send_data = true;
	}
	if (bsec_get_latest_output(prv_bme690_latest_outputs) == 0) {
		prv_bme690_send_data = true;
	}
}

void air_quality_get_data_pointers(bmv080_output_t **bmv080_data, bsec_output_t **bme690_data)
{
	if (bmv080_data != NULL && prv_bmv080_send_data) {
		*bmv080_data = &prv_bmv080_latest_output;
		prv_bmv080_send_data = false;
	}

	if (bme690_data != NULL && prv_bme690_send_data) {
		*bme690_data = &prv_bme690_latest_outputs[0];
		prv_bme690_send_data = false;
	}
}

int air_quality_init(void)
{
	/* BME69X initialization - BSEC */
	int ret = bsec_setup();
	if (ret != BSEC_OK) {
		LOG_ERR("BSEC setup failed with error code: %d", ret);
		return ret;
	} else {
		LOG_INF("BSEC setup successful");
	}

	bsec_controller(&prv_bme69x_BSEC_sleep_timer);

	/* BMV080 initialization */
	ret = bmv080_sensor_init();
	if (ret) {
		LOG_ERR("Failed to init BMV080 sensor.");
		return ret;
	}

	return 0;
}

int air_quality_handle(void)
{
	if (k_uptime_get() < AIR_QUALITY_STARTUP_DELAY_S * 1000) {
		/* Wait for all threads to initialize */
		return 0;
	}

	int err;
	/* BME69X - BSEC library handling */
	if (k_uptime_get() * 1000000LL >= prv_bme69x_BSEC_sleep_timer) {
		err = bsec_controller(&prv_bme69x_BSEC_sleep_timer);
		if (err) {
			LOG_ERR("BSEC controller failed with error code: %d", err);
			return err;
		}
	}

	/* BMV080 - sensor handling is done in its own thread via k_work and k_timer so we don't do
	 * anything here. */

	/* Get available data from sensors */
	prv_get_available_data();

	return 0;
}
