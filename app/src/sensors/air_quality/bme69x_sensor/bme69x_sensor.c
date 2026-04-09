/** @file bme69x_sensor.c
 *
 * @brief BME69X sensor interface module.
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2025 Irnas. All rights reserved.
 */

#include <zephyr/logging/log.h>

#include <bme69x_sensor.h>

LOG_MODULE_REGISTER(bme69x_sensor);

BUILD_ASSERT(DT_NODE_EXISTS(DT_NODELABEL(bme69x)), "bme69x not defined in DT");

static const struct device *prv_bme69x_sensor_dev = DEVICE_DT_GET(DT_NODELABEL(bme69x));

/* BME69X - configuration */
static struct bme69x_conf bme_conf;
static struct bme69x_heatr_conf bme_heatr_conf;

/* BME69X - BSEC 3.2.1.0
 * BSEC uses configuration data to setup the library. This data is provided by Bosch and
 * is specific to the sensor variant. The array bsec_config_iaq is provided by Bosch and
 * can be found in the BSEC software package.
 */
extern const uint8_t bsec_config_iaq[];
static const uint32_t bsec_config_iaq_len = 554;
static uint8_t bsec_instance_mem[1384] __aligned(4);
static void *bsec_inst = (void *)bsec_instance_mem;

static uint8_t bsec_work_buffer[BSEC_MAX_PROPERTY_BLOB_SIZE];

bsec_sensor_configuration_t requested_virtual_sensors[] = {
	{(1.0f / 300.0f), BSEC_OUTPUT_IAQ},          {(1.0f / 300.0f), BSEC_OUTPUT_RAW_TEMPERATURE},
	{(1.0f / 300.0f), BSEC_OUTPUT_RAW_PRESSURE}, {(1.0f / 300.0f), BSEC_OUTPUT_RAW_HUMIDITY},
	{(1.0f / 300.0f), BSEC_OUTPUT_RAW_GAS},
};

bsec_sensor_configuration_t actual_sensors[8];
uint8_t num_actual = 8;

bsec_output_t prv_latest_outputs[8];
bool prv_data_available = false;

/* Interval at which the BSEC state is saved */
#define SAVE_BSEC_STATE_MEASUREMENT_CYCLE_INTERVAL 100

#define BSEC_SAMPLE_RATE BSEC_SAMPLE_RATE_ULP
uint8_t *bsecInstance[8];

/**
 * @brief Print BSEC sensor settings.
 *
 * @param bsec_bme_settings_t settings structure containing the settings for the BME sensor
 */
static void prv_print_settings(bsec_bme_settings_t settings)
{
	LOG_INF("BSEC sensor control settings:");
	LOG_INF("Next call: %lld", settings.next_call);
	LOG_INF("Process data: %u", settings.process_data);
	LOG_INF("Heater temperature: %u", settings.heater_temperature);
	LOG_INF("Heater duration: %u", settings.heater_duration);
	LOG_INF("Heater temperature profile: %u %u %u %u %u %u %u %u %u %u",
		settings.heater_temperature_profile[0], settings.heater_temperature_profile[1],
		settings.heater_temperature_profile[2], settings.heater_temperature_profile[3],
		settings.heater_temperature_profile[4], settings.heater_temperature_profile[5],
		settings.heater_temperature_profile[6], settings.heater_temperature_profile[7],
		settings.heater_temperature_profile[8], settings.heater_temperature_profile[9]);
	LOG_INF("Heater duration profile: %u %u %u %u %u %u %u %u %u %u",
		settings.heater_duration_profile[0], settings.heater_duration_profile[1],
		settings.heater_duration_profile[2], settings.heater_duration_profile[3],
		settings.heater_duration_profile[4], settings.heater_duration_profile[5],
		settings.heater_duration_profile[6], settings.heater_duration_profile[7],
		settings.heater_duration_profile[8], settings.heater_duration_profile[9]);
	LOG_INF("Heater profile length: %u", settings.heater_profile_len);
	LOG_INF("Run gas: %u", settings.run_gas);
	LOG_INF("Pressure oversampling: %u", settings.pressure_oversampling);
	LOG_INF("Humidity oversampling: %u", settings.humidity_oversampling);
	LOG_INF("Temperature oversampling: %u", settings.temperature_oversampling);
	LOG_INF("Trigger measurement: %u", settings.trigger_measurement);
}

/**
 * @brief Print raw sampled data from sensors.
 *
 * @param inputs Array of bsec_input_t structures containing the raw sampled data.
 * @param n_inputs Number of inputs in the array.
 */
static void prv_print_raw_sampled_data(bsec_input_t *inputs, int n_inputs)
{
	/* print inputs */
	for (int i = 0; i < n_inputs; i++) {
		LOG_INF("Sensor ID: %d, Signal: %.2f, Timestamp: %lld", inputs[i].sensor_id,
			inputs[i].signal, inputs[i].time_stamp);
	}
}

/**
 * @brief Print processed data from BSEC.
 *
 * @param outputs Array of bsec_output_t structures containing the processed data.
 * @param n_outputs Number of outputs in the array.
 */
static void prv_print_processed_data(bsec_output_t *outputs, int n_outputs)
{
	for (int i = 0; i < n_outputs; ++i) {
		if (outputs[i].sensor_id == BSEC_OUTPUT_IAQ) {
			LOG_INF("IAQ: %.2f", outputs[i].signal);
		} else if (outputs[i].sensor_id == BSEC_OUTPUT_RAW_TEMPERATURE) {
			LOG_INF("Raw Temperature: %.2f C", outputs[i].signal);
		} else if (outputs[i].sensor_id == BSEC_OUTPUT_RAW_PRESSURE) {
			LOG_INF("Raw Pressure: %.2f Pa", outputs[i].signal);
		} else if (outputs[i].sensor_id == BSEC_OUTPUT_RAW_HUMIDITY) {
			LOG_INF("Raw Humidity: %.2f %%", outputs[i].signal);
		} else if (outputs[i].sensor_id == BSEC_OUTPUT_RAW_GAS) {
			LOG_INF("Raw Gas Resistance: %.2f Ohm", outputs[i].signal);
		}
	}
}

/**
 * @brief Set the BME690 sensor to FORCED mode, fetch sensor data, and pass it to BSEC for
 * processing. Afterwards set the device to SLEEP mode.
 */
static void prv_sensor_task()
{
	if (!device_is_ready(prv_bme69x_sensor_dev)) {
		LOG_ERR("Sensor not ready!");
		return;
	}

	/* Set sensor to FORCED mode */
	struct sensor_value bme_mode_val = {.val1 = BME69X_FORCED_MODE, .val2 = 0};

	if (sensor_attr_set(prv_bme69x_sensor_dev, SENSOR_CHAN_ALL, (int)SENSOR_ATTR_MODE,
			    &bme_mode_val) < 0) {
		LOG_ERR("Failed to set BME69X mode");
		return;
	}

	/* Fetch sensor data */
	if (sensor_sample_fetch(prv_bme69x_sensor_dev) < 0) {
		LOG_ERR("Sensor fetch failed");
		return;
	}

	/* Get sensor data */
	struct sensor_value temp, press, hum, gas;
	sensor_channel_get(prv_bme69x_sensor_dev, SENSOR_CHAN_AMBIENT_TEMP, &temp);
	sensor_channel_get(prv_bme69x_sensor_dev, SENSOR_CHAN_PRESS, &press);
	sensor_channel_get(prv_bme69x_sensor_dev, SENSOR_CHAN_HUMIDITY, &hum);
	sensor_channel_get(prv_bme69x_sensor_dev, SENSOR_CHAN_GAS_RES, &gas);

	/* Convert sensor data to double */
	bsec_input_t inputs[5];
	int n_inputs = 0;

	const int64_t timestamp_ns = k_uptime_get() * 1000000LL;

	inputs[n_inputs++] = (bsec_input_t){.sensor_id = BSEC_INPUT_TEMPERATURE,
					    .signal = sensor_value_to_double(&temp),
					    .time_stamp = timestamp_ns};

	inputs[n_inputs++] = (bsec_input_t){.sensor_id = BSEC_INPUT_HUMIDITY,
					    .signal = sensor_value_to_double(&hum),
					    .time_stamp = timestamp_ns};

	inputs[n_inputs++] = (bsec_input_t){.sensor_id = BSEC_INPUT_PRESSURE,
					    .signal = sensor_value_to_double(&press),
					    .time_stamp = timestamp_ns};

	inputs[n_inputs++] = (bsec_input_t){.sensor_id = BSEC_INPUT_GASRESISTOR,
					    .signal = sensor_value_to_double(&gas),
					    .time_stamp = timestamp_ns};

	/* Print inputs */
	prv_print_raw_sampled_data(inputs, n_inputs);

	/* Pass data to BSEC and process it */
	bsec_output_t outputs[BSEC_NUMBER_OUTPUTS];
	uint8_t n_outputs = BSEC_NUMBER_OUTPUTS;

	bsec_library_return_t status =
		bsec_do_steps(bsec_inst, inputs, n_inputs, outputs, &n_outputs);

	/* Print processed data */
	if (status == BSEC_OK) {
		prv_print_processed_data(outputs, n_outputs);

		memcpy(prv_latest_outputs, outputs, sizeof(prv_latest_outputs));
		prv_data_available = true;
	} else {
		LOG_ERR("BSEC processing failed with error code: %d", status);
	}

	/* Set sensor to SLEEP mode */
	bme_mode_val.val1 = BME69X_SLEEP_MODE;
	bme_mode_val.val2 = 0;

	if (sensor_attr_set(prv_bme69x_sensor_dev, SENSOR_CHAN_ALL, (int)SENSOR_ATTR_MODE,
			    &bme_mode_val) < 0) {
		LOG_ERR("Failed to set BME69X mode");
		return;
	}
}

int bsec_get_latest_output(bsec_output_t *out)
{
	if (prv_data_available && out != NULL) {
		memcpy(out, &prv_latest_outputs, sizeof(prv_latest_outputs));
		prv_data_available = false;

		return 0;
	}

	return -EIO;
}

bsec_library_return_t bsec_setup()
{
	bsec_library_return_t status;

	status = bsec_init((void *)bsec_instance_mem);
	if (status != BSEC_OK) {
		LOG_ERR("BSEC init failed: %d\n", status);
		return status;
	}

	status = bsec_set_configuration(bsec_inst, bsec_config_iaq, bsec_config_iaq_len,
					bsec_work_buffer, ARRAY_SIZE(bsec_work_buffer));
	if (status != BSEC_OK) {
		LOG_ERR("BSEC config load failed: %d\n", status);
		return status;
	}

	status = bsec_update_subscription(bsec_inst, requested_virtual_sensors,
					  sizeof(requested_virtual_sensors) /
						  sizeof(requested_virtual_sensors[0]),
					  actual_sensors, &num_actual);

	if (status != BSEC_OK) {
		LOG_ERR("BSEC subscription update failed with error code: %d", status);
		return status;
	}

	return status;
}

int bsec_controller(int64_t *sleep_timer)
{
	bsec_bme_settings_t settings;
	bsec_library_return_t status =
		bsec_sensor_control(bsec_inst, k_uptime_get() * 1000000LL, &settings);
	if (status != BSEC_OK) {
		LOG_ERR("BSEC sensor control failed with error code: %d", status);
		return -1;
	}

	*sleep_timer = settings.next_call;
	prv_print_settings(settings);

	if (settings.run_gas) {
		bme_heatr_conf.enable = settings.run_gas;
		bme_heatr_conf.heatr_temp = settings.heater_temperature;
		bme_heatr_conf.heatr_dur = settings.heater_duration;
		bme_heatr_conf.heatr_temp_prof = settings.heater_temperature_profile;
		bme_heatr_conf.heatr_dur_prof = settings.heater_duration_profile;
		bme_heatr_conf.profile_len = settings.heater_profile_len;

		/* Set heater duration in PARALLEL mode */
		struct sensor_value bme_heatr_conf_val = {.val1 = (int32_t)&bme_heatr_conf,
							  .val2 = 0};

		sensor_attr_set(prv_bme69x_sensor_dev, SENSOR_CHAN_ALL, (int)SENSOR_ATTR_HEATER_ALL,
				&bme_heatr_conf_val);
	}
	if (settings.trigger_measurement) {
		bme_conf.os_hum = settings.humidity_oversampling;
		bme_conf.os_pres = settings.pressure_oversampling;
		bme_conf.os_temp = settings.temperature_oversampling;

		struct sensor_value bme_conf_val = {.val1 = (int32_t)&bme_conf, .val2 = 0};

		sensor_attr_set(prv_bme69x_sensor_dev, SENSOR_CHAN_ALL,
				(int)SENSOR_ATTR_CONFIGURATION, &bme_conf_val);
		/* Trigger measurement */
		prv_sensor_task();
	}

	return 0;
}
