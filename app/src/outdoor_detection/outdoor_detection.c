/** @file outdoor_detection.c
 *
 * @brief This file contains the implementation of the outdoor detection module.
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2025 Irnas. All rights reserved.
 */

#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <communication.h>
#include <global_time.h>
#include <pangolin_logic.h>
#include <settings_def.h>
#include <values_def.h>

#include <outdoor_detection.h>

LOG_MODULE_REGISTER(outdoor_detection);

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif /* M_PI */

#define OUTDOOR_DETECTION_FIX_INTERVAL_S           3600 /* 60 minutes */
#define OUTDOOR_DETECTION_DATA_SAMPLING_INTERVAL_S 900  /* 15 minutes */

#define OUTDOOR_DETECTION_MAX_INTER_FIX_SAMPLES                                                    \
	((OUTDOOR_DETECTION_FIX_INTERVAL_S / OUTDOOR_DETECTION_DATA_SAMPLING_INTERVAL_S) + 1)

extern const struct device *prv_lis2dw12_sensor;

static bool prv_outdoor_detected = false; /* Outdoor detection status */

/* The inter-fix mean temperature value for the last 20 fixes */
float prv_temperature_mean_history[20];
static int prv_temperature_mean_history_i = 0;
float prv_temperature_standard_deviation = 0.0f;

float prv_temperature_values_inter_fix[OUTDOOR_DETECTION_MAX_INTER_FIX_SAMPLES];
int prv_temperature_values_inter_fix_i = 0;

struct accelerometer_data
	prv_accelerometer_values_inter_fix[OUTDOOR_DETECTION_MAX_INTER_FIX_SAMPLES];
int prv_accelerometer_values_inter_fix_i = 0;

static uint32_t prv_first_inter_fix_time = 0;

static uint32_t prv_last_fix_check_time =
	0; /* Last time the outdoor detection fix check was performed */
static uint32_t prv_last_data_sampling_check_time =
	0; /* Last time the outdoor detection data sampling check was performed */

/* This bool is used for outside modules from different threads to check the status of the outdoor
 * detection. */
static bool prv_fix_interval_elapsed = false;

/* -------------- Private functions -------------- */

static int prv_outdoor_detection_fix_check_interval(void)
{
	/* Check if the outdoor detection interval has passed */
	uint32_t current_time = k_uptime_get_32();

	if (current_time - prv_last_fix_check_time >= OUTDOOR_DETECTION_FIX_INTERVAL_S * 1000) {
		prv_last_fix_check_time = current_time;
		prv_fix_interval_elapsed = true;
		return 1; /* Interval has passed */
	}

	return 0; /* Interval has not passed */
}

static int prv_outdoor_detection_data_sampling_check_interval(void)
{
	/* Check if the outdoor detection interval has passed */
	uint32_t current_time = k_uptime_get_32();

	if (current_time - prv_last_data_sampling_check_time >=
	    OUTDOOR_DETECTION_DATA_SAMPLING_INTERVAL_S * 1000) {
		prv_last_data_sampling_check_time = current_time;
		return 1; /* Interval has passed */
	}

	return 0; /* Interval has not passed */
}

/**
 * @brief Sample the temperature from the LIS2DW12 sensor.
 *
 * @param[out] temperature Pointer to a float where the sampled temperature will be stored.
 *
 * @retval -EINVAL if the temperature pointer is NULL.
 * @retval -ENODEV if the LIS2DW12 sensor is not initialized.
 * @return 0 on success, negative error code on failure.
 */
static int prv_sample_temperature(float *temperature)
{
	if (temperature == NULL) {
		LOG_ERR("Temperature pointer is NULL!");
		return -EINVAL; /* Invalid argument */
	}

	if (!prv_lis2dw12_sensor) {
		LOG_ERR("LIS2DW12 sensor not initialized!");
		return -ENODEV; /* Device not ready */
	}

	struct sensor_value val;
	int err = sensor_sample_fetch(prv_lis2dw12_sensor);
	if (err < 0) {
		LOG_ERR("Failed to fetch temperature sample: %d", err);
		return err;
	}

	err = sensor_channel_get(prv_lis2dw12_sensor, SENSOR_CHAN_AMBIENT_TEMP, &val);
	if (err < 0) {
		LOG_ERR("Failed to get temperature channel: %d", err);
		return err;
	}

	LOG_INF("Accelerometer temperature sample: %d.%04d°C", val.val1, val.val2);

	*temperature =
		(float)((float)val.val1 + ((float)val.val2 / 1000000.0f)); /* Convert to float */

	return 0;
}

/**
 * @brief Sample the accelerometer data from the LIS2DW12 sensor.
 *
 * @param[out] buf Pointer to a float array where the sampled accelerometer data will be stored.
 *
 * @retval -EINVAL if the buffer is NULL or too small.
 * @retval -ENOMEM if the buffer size is too small to hold accelerometer data.
 * @retval -ENODEV if the LIS2DW12 sensor is not initialized.
 * @return 0 on success, negative error code on failure.
 */
static int prv_sample_accelerometer(float *buf, size_t buf_size)
{
	if (buf == NULL) {
		LOG_ERR("Buffer for accelerometer data is NULL!");
		return -EINVAL; /* Invalid argument */
	}

	if (buf_size < 3) {
		LOG_ERR("Buffer size is too small for accelerometer data!");
		return -ENOMEM; /* Not enough memory */
	}

	if (!prv_lis2dw12_sensor) {
		LOG_ERR("LIS2DW12 sensor not initialized!");
		return -ENODEV; /* Device not ready */
	}

	struct sensor_value val[3];
	int err = sensor_sample_fetch(prv_lis2dw12_sensor);
	if (err < 0) {
		LOG_ERR("Failed to fetch accelerometer sample: %d", err);
		return err;
	}

	err = sensor_channel_get(prv_lis2dw12_sensor, SENSOR_CHAN_ACCEL_XYZ, val);
	if (err < 0) {
		LOG_ERR("Failed to get accelerometer channel: %d", err);
		return err;
	}

	buf[0] = val[0].val1 + (val[0].val2 / 1000000.0f); /* X-axis */
	buf[1] = val[1].val1 + (val[1].val2 / 1000000.0f); /* Y-axis */
	buf[2] = val[2].val1 + (val[2].val2 / 1000000.0f); /* Z-axis */

	return 0;
}

/**
 * @brief calculate mean and save inter-fix temperature readings to the history array.
 *
 */
static void prv_save_mean_to_temperature_history(void)
{
	float temp_sum = 0.0f;
	for (int i = 0; i < prv_temperature_values_inter_fix_i; i++) {
		temp_sum += prv_temperature_values_inter_fix[i];
	}

	/* move history left */
	memmove(&prv_temperature_mean_history[0], &prv_temperature_mean_history[1],
		sizeof(prv_temperature_mean_history) - sizeof(prv_temperature_mean_history[0]));

	prv_temperature_mean_history[ARRAY_SIZE(prv_temperature_mean_history) - 1] =
		temp_sum / prv_temperature_values_inter_fix_i;

	/* Reset i */
	prv_temperature_values_inter_fix_i = 0;
}

/**
 * @brief calculate mean and save inter-fix accelerometer readings to the history array.
 */
static void prv_save_mean_to_accelerometer_history(void)
{

	float acc_x_sum = 0.0f;
	float acc_y_sum = 0.0f;
	float acc_z_sum = 0.0f;

	for (int i = 0; i < prv_accelerometer_values_inter_fix_i; i++) {
		acc_x_sum += prv_accelerometer_values_inter_fix[i].x;
		acc_y_sum += prv_accelerometer_values_inter_fix[i].y;
		acc_z_sum += prv_accelerometer_values_inter_fix[i].z;
	}

	struct accelerometer_data data;
	data.x = acc_x_sum / prv_accelerometer_values_inter_fix_i;
	data.y = acc_y_sum / prv_accelerometer_values_inter_fix_i;
	data.z = acc_z_sum / prv_accelerometer_values_inter_fix_i;

	/* Reset i */
	prv_accelerometer_values_inter_fix_i = 0;
}

/**
 * @brief Perform outdoor detection check based on the current sensor data.
 *
 * @param[out] outdoor_detected Pointer to a boolean variable where the result will be
 * stored.
 *
 * @retval 0 on success.
 * @retval -EINVAL if the outdoor_detected pointer is NULL.
 */
int prv_outdoor_detection_check(bool *outdoor_detected)
{
	if (outdoor_detected == NULL) {
		LOG_ERR("Outdoor detected pointer is NULL");
		return -EINVAL; /* Invalid pointer */
	}

	struct pangolin_logic_cfg cfg = {
		.bias = Main_settings.outdoor_detection_parameters->def_val[0] |
			Main_settings.outdoor_detection_parameters->def_val[1] << 8,
		.temperature_weight = Main_settings.outdoor_detection_parameters->def_val[2] |
				      Main_settings.outdoor_detection_parameters->def_val[3] << 8,
		.accelerometer_weight = Main_settings.outdoor_detection_parameters->def_val[4] |
					Main_settings.outdoor_detection_parameters->def_val[5] << 8,
		.hour_weight = Main_settings.outdoor_detection_parameters->def_val[6] |
			       Main_settings.outdoor_detection_parameters->def_val[7] << 8,
		.time_offset_seconds =
			Main_settings.outdoor_detection_parameters->def_val[8] |
			Main_settings.outdoor_detection_parameters->def_val[9] << 8 |
			Main_settings.outdoor_detection_parameters->def_val[10] << 16 |
			Main_settings.outdoor_detection_parameters->def_val[11] << 24,
	};

	float outdoor_probability = 0.0f;

	struct pangolin_logic_data pangolin_data = {
		.temp_mean_history_values_buf = prv_temperature_mean_history,
		.temp_mean_history_values_buf_size = ARRAY_SIZE(prv_temperature_mean_history),
		.temp_inter_fix_values = prv_temperature_values_inter_fix,
		.temp_inter_fix_i = prv_temperature_values_inter_fix_i,
		.acc_inter_fix_values = prv_accelerometer_values_inter_fix,
		.acc_inter_fix_i = prv_accelerometer_values_inter_fix_i,
		.first_inter_fix_time = prv_first_inter_fix_time,
	};

	int err =
		pangolin_outdoor_probability_calculation(&outdoor_probability, cfg, &pangolin_data);
	if (err < 0) {
		LOG_ERR("Outdoor detection calculation failed with error: %d", err);
		return err;
	}

	if (outdoor_probability >= ((float)Main_settings.outdoor_detection_tau->def_val / 100)) {
		LOG_INF("Detected outdoor probability is above threshold: %d %% < %d %%",
			(int)((outdoor_probability - (int)outdoor_probability) * 100),
			Main_settings.outdoor_detection_tau->def_val);
		*outdoor_detected = true;
	} else {
		LOG_INF("Detected outdoor probability is below threshold: %d %% < %d %%",
			(int)((outdoor_probability - (int)outdoor_probability) * 100),
			Main_settings.outdoor_detection_tau->def_val);
		*outdoor_detected = false;
	}

#ifdef CONFIG_DEBUG
	common_utils_print_float_as_int(&outdoor_probability, sizeof(outdoor_probability),
					"Probability: ");
#endif /* CONFIG_DEBUG */

	return 0; /* Outdoor not detected */
}

/* -------------- -------------- Public functions -------------- -------------- */

int outdoor_detection_init(void)
{
	struct sensor_value temperature_val;
	prv_first_inter_fix_time = get_global_unix_time();

	if (!prv_lis2dw12_sensor) {
		LOG_ERR("Failed to init LIS2DW12!");
		return -EIO; /* Device not found */
	}

	for (int i = 0; i < 20; i++) {
		int err = sensor_sample_fetch(prv_lis2dw12_sensor);
		if (!err) {
			err = sensor_channel_get(prv_lis2dw12_sensor, SENSOR_CHAN_AMBIENT_TEMP,
						 &temperature_val);
		}

		LOG_DBG("Temperature: %d.%d", temperature_val.val1, temperature_val.val2);

		/* Fill temperature history with fresh data */
		prv_temperature_mean_history[i] =
			temperature_val.val1 + (temperature_val.val2 / 1000000.0f);
	}

	prv_temperature_mean_history_i = 0;

	return 0;
}

void outdoor_detection_handle(void)
{
	if (Main_settings.outdoor_detection_enabled->def_val == false) {
		LOG_DBG("Outdoor detection is disabled, skipping check.");
		return; /* Outdoor detection is disabled */
	}

	if (prv_outdoor_detection_data_sampling_check_interval()) {
		LOG_INF("Outdoor detection data sampling interval passed, performing sampling...");
		/* Fetch inter-fix data */
		if (prv_temperature_values_inter_fix_i == 0) {
			prv_first_inter_fix_time = get_global_unix_time();
		}

		int err = prv_sample_temperature(
			&prv_temperature_values_inter_fix[prv_temperature_values_inter_fix_i]);
		if (err) {
			LOG_ERR("Failed to sample temperature: %d", err);
			return; /* Error in temperature sampling */
		}

		int temp_decimal_part = (int)((prv_temperature_values_inter_fix
						       [prv_temperature_values_inter_fix_i] -
					       (int)prv_temperature_values_inter_fix
						       [prv_temperature_values_inter_fix_i]) *
					      1000000.0f);
		if (temp_decimal_part < 0) {
			temp_decimal_part *= -1; /* Ensure positive decimal part */
		}

		err = prv_sample_accelerometer(
			(float *)&prv_accelerometer_values_inter_fix
				[prv_accelerometer_values_inter_fix_i],
			sizeof(prv_accelerometer_values_inter_fix) /
				sizeof(prv_accelerometer_values_inter_fix[0]));
		if (err) {
			LOG_ERR("Failed to sample accelerometer: %d", err);
			return; /* Error in accelerometer sampling */
		}

#ifdef CONFIG_DEBUG
		/* Print sampled data */
		common_utils_print_float_as_int(
			&prv_accelerometer_values_inter_fix[prv_accelerometer_values_inter_fix_i].x,
			sizeof(struct accelerometer_data), "Accelerometer (x,y,z): ");

		common_utils_print_float_as_int(
			&prv_temperature_values_inter_fix[prv_temperature_values_inter_fix_i],
			sizeof(float), "Temperature: ");
#endif /* CONFIG_DEBUG */

		prv_temperature_values_inter_fix_i++;
		prv_accelerometer_values_inter_fix_i++;
	}

	/* Check if the outdoor detection interval has passed */
	if (prv_outdoor_detection_fix_check_interval()) {
		LOG_INF("Outdoor detection check interval passed, performing check...");
		int err = prv_outdoor_detection_check(&prv_outdoor_detected);
		if (err < 0) {
			LOG_ERR("Outdoor detection check failed with error: %d", err);
			return; /* Error in outdoor detection check */
		}

		if (prv_outdoor_detected) {
			LOG_ERR("Outdoor conditions detected.");

		} else {
			LOG_ERR("No outdoor conditions detected.");
		}

		/* Add inter-fix data to history */
		if (prv_temperature_values_inter_fix_i > 0) {
			/* Save the mean inter-fix temperature */
			prv_save_mean_to_temperature_history();
		}
		if (prv_accelerometer_values_inter_fix_i > 0) {
			/* Save the mean inter-fix accelerometer data */
			prv_save_mean_to_accelerometer_history();
		}
	}
}

bool outdoor_detection_get_status(void)
{
	return prv_outdoor_detected;
}

void outdoor_detection_clear_status(void)
{
	prv_outdoor_detected = false; /* Clear outdoor detection status */
}

bool outdoor_detection_get_fix_interval_elapsed(void)
{
	return prv_fix_interval_elapsed;
}

void outdoor_detection_clear_fix_interval_elapsed(void)
{
	prv_fix_interval_elapsed = false;
}
