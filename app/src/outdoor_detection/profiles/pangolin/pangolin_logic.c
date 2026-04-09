/** @file pangolin_logic.c
 *
 * @brief This file contains the logic for outdoor detection for Pangolins.
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2025 Irnas. All rights reserved.
 */

#include <math.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <common_utils.h>
#include <global_time.h>
#include <outdoor_detection.h>
#include <settings_def.h>

#include <pangolin_logic.h>

LOG_MODULE_REGISTER(pangolin_logic);

int pangolin_outdoor_probability_calculation(float *outdoor_probability,
					     struct pangolin_logic_cfg cfg,
					     struct pangolin_logic_data *data)
{
	if (data == NULL || outdoor_probability == NULL ||
	    data->temp_mean_history_values_buf == NULL || data->temp_inter_fix_values == NULL ||
	    data->acc_inter_fix_values == NULL) {
		return -EINVAL; /* Invalid input pointers */
	}

	/* Temperature probability calculation */
	float temperature_probability = 0.0f;

	float temp_mean_dev_z_20 = 0.0f;
	int err = common_utils_mean_dev_z_20(
		data->temp_mean_history_values_buf, data->temp_mean_history_values_buf_size,
		data->temp_inter_fix_values, data->temp_inter_fix_i, &temp_mean_dev_z_20);
	if (err < 0) {
		return err; /* Error in temperature mean deviation calculation */
	}

	if (temp_mean_dev_z_20 <= -1.46) {
		if (common_utils_max_z_diff(data->temp_inter_fix_values, data->temp_inter_fix_i) <=
		    1.40) {
			temperature_probability = 0.72;
		} else {
			temperature_probability = 0.38;
		}
	} else {
		if (temp_mean_dev_z_20 <= -0.87) {
			temperature_probability = 0.27;
		} else {
			if (temp_mean_dev_z_20 <= 2.74) {
				temperature_probability = 0.03;
			} else {
				temperature_probability = 0.5;
			}
		}
	}

	/* Accelerometer probability calculation */

	float accelerometer_probability = 0.0f;
	if (common_utils_y_z_mean(data->acc_inter_fix_values, data->acc_inter_fix_i) <= -9.87) {
		if (common_utils_x_z_max_diff(data->acc_inter_fix_values, data->acc_inter_fix_i) <=
		    9.02) {
			if (common_utils_x_z_avg_diff(data->acc_inter_fix_values,
						      data->acc_inter_fix_i) <= 5.39) {
				accelerometer_probability = 0.10;

			} else {
				if (common_utils_mag_var(data->acc_inter_fix_values,
							 data->acc_inter_fix_i) <= 0.23) {
					accelerometer_probability = 0.33;
				} else {
					accelerometer_probability = 0.88;
				}
			}
		} else {
			if (common_utils_mag_var(data->acc_inter_fix_values,
						 data->acc_inter_fix_i) <= 0.20) {
				accelerometer_probability = 0.57;
			} else {
				accelerometer_probability = 0.83;
			}
		}
	} else {
		if (common_utils_x_z_avg_diff(data->acc_inter_fix_values, data->acc_inter_fix_i) <=
		    6.96) {
			accelerometer_probability = 0.02;
		} else {
			if (common_utils_x_z_sum_diff(data->acc_inter_fix_values,
						      data->acc_inter_fix_i) <= 13.73) {
				accelerometer_probability = 0.48;
			} else {
				if (common_utils_mag_var(data->acc_inter_fix_values,
							 data->acc_inter_fix_i) <= 0.16) {
					accelerometer_probability = 0.09;
				} else {
					accelerometer_probability = 0.33;
				}
			}
		}
	}

	/* Hour probability calculation */

	float hour_probability = 0.0f;

	uint32_t offset_time = data->first_inter_fix_time + cfg.time_offset_seconds;

	uint8_t current_hour = (offset_time / 3600) % 24;
	if (common_utils_hour_sin(current_hour) <= 0.0f) {
		if (common_utils_hour_cos(current_hour) <= -0.13f) {
			hour_probability = 0.07f;
		} else {
			if (common_utils_hour_cos(current_hour) <= 0.92f) {
				hour_probability = 0.42f;
			} else {
				hour_probability = 0.21f;
			}
		}
	} else {
		hour_probability = 0.02;
	}

	LOG_DBG("Bias: %d, Temperature weight: %d, Accelerometer weight: %d, Hour weight: "
		"%d",
		cfg.bias, cfg.temperature_weight, cfg.accelerometer_weight, cfg.hour_weight);

#ifdef CONFIG_DEBUG
	/* Print all probabilities */
	common_utils_print_float_as_int(&temperature_probability, sizeof(float),
					"Temperature probability: ");
	common_utils_print_float_as_int(&accelerometer_probability, sizeof(float),
					"Accelerometer probability: ");
	common_utils_print_float_as_int(&hour_probability, sizeof(float), "Hour probability: ");
#endif /* CONFIG_DEBUG */

	float prediction_value =
		((float)cfg.bias / 1000) +
		(((float)cfg.temperature_weight / 1000) * temperature_probability) +
		(((float)cfg.accelerometer_weight / 1000) * accelerometer_probability) +
		(((float)cfg.hour_weight / 1000) * hour_probability);

	/* Print prediction values */

	*outdoor_probability = 1 / (1 + expf(-prediction_value));

	if (*outdoor_probability < 0.0f || *outdoor_probability > 1.0f) {
		return -ERANGE; /* Outdoor probability out of range */
	}
	return 0;
}
