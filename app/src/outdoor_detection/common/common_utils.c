/** @file common_util.c
 *
 * @brief This file contains common utility functions and definitions used by outdoor detection
 * profiles.
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2025 Irnas. All rights reserved.
 */

#include <math.h>
#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <common_utils.h>

LOG_MODULE_REGISTER(common_utils);

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif /* M_PI */

int common_utils_mean_dev_z_20(float *mean_values_buf, size_t mean_values_buf_len,
			       float *new_values_buf, int new_values_buf_len, float *mean_dev_z_20)
{
	if (mean_values_buf == NULL || new_values_buf == NULL || mean_dev_z_20 == NULL ||
	    mean_values_buf_len < 20 || new_values_buf_len <= 0) {
		return -EINVAL; /* Invalid input pointers */
	}

	/* Calculate the new values mean */
	float new_values_mean = 0.0f;
	for (int i = 0; i < new_values_buf_len; i++) {
		new_values_mean += new_values_buf[i];
	}
	new_values_mean /= new_values_buf_len;

	/* Calculate rolling mean of previous 20 periods */
	float sum = 0.0f;
	for (int i = 0; i < mean_values_buf_len; i++) {
		sum += mean_values_buf[i];
	}
	float temp_history_20_mean = sum / mean_values_buf_len;

	/* Calculate the (population) variance of previous 20 periods */
	float variance_prv = 0.0f;
	for (int i = 0; i < mean_values_buf_len; i++) {
		variance_prv += (mean_values_buf[i] - temp_history_20_mean) *
				(mean_values_buf[i] - temp_history_20_mean);
	}
	variance_prv /= mean_values_buf_len;

	/* Calculate standard deviation from variance */
	float standard_deviation = sqrt(variance_prv);

	if (standard_deviation == 0.0f) {
		*mean_dev_z_20 = 0.0f;
		return 0;
	}

	/* Calculate final deviation */
	*mean_dev_z_20 = (new_values_mean - temp_history_20_mean) / (standard_deviation);

	return 0;
}

float common_utils_max_z_diff(float *float_array, size_t float_array_len)
{
	if (float_array_len < 2) {
		return 0.0f;
	}

	/* Calculate the first-order difference */
	float differences[float_array_len - 1];
	for (int i = 0; i < float_array_len - 1; i++) {
		differences[i] = float_array[i + 1] - float_array[i];
	}

	/* Calculate mean difference */
	float mean_diff = 0.0f;
	for (int i = 0; i < float_array_len - 1; i++) {
		mean_diff += differences[i];
	}
	mean_diff /= (float_array_len - 1);

	/* Calculate absolute deviation from mean */
	float deviations[float_array_len - 1];
	for (int i = 0; i < float_array_len - 1; i++) {
		deviations[i] = fabs(differences[i] - mean_diff);
	}

	/* Get max deviation from mean */
	float max_deviation = 0.0f;
	for (int i = 0; i < float_array_len - 1; i++) {
		if (deviations[i] > max_deviation) {
			max_deviation = deviations[i];
		}
	}

	/* Calculate variance */
	float variance = 0.0f;
	for (int i = 0; i < float_array_len - 1; i++) {
		variance += (deviations[i]) * (deviations[i]);
	}
	variance /= (float_array_len - 1);

	float standard_deviation = sqrt(variance);

	return max_deviation / standard_deviation;
}

float common_utils_y_z_mean(struct accelerometer_data *buf, size_t buf_len)
{
	float y_z_sum_mean = 0.0f;
	for (int i = 0; i < buf_len; i++) {
		y_z_sum_mean += buf[i].y + buf[i].z;
	}
	if (buf_len == 0) {
		return 0.0f; /* Avoid division by zero */
	}
	return y_z_sum_mean / buf_len;
}

float common_utils_x_z_max_diff(struct accelerometer_data *buf, size_t buf_len)
{
	/* Starting with `max_diff` set at a specific value (e.g. 0) results in buggy behavior. */
	float max_diff = buf[0].x - buf[0].z;

	for (int i = 0; i < buf_len; i++) {
		float diff = buf[i].x - buf[i].z;
		if (diff > max_diff) {
			max_diff = buf[i].x - buf[i].z;
		}
	}
	return max_diff;
}

float common_utils_x_z_avg_diff(struct accelerometer_data *buf, size_t buf_len)
{
	float acc_x_z_avg_diff = 0.0f;
	for (int i = 0; i < buf_len; i++) {
		acc_x_z_avg_diff += buf[i].x - buf[i].z;
	}
	if (buf_len == 0) {
		return 0.0f; /* Avoid division by zero */
	}
	return acc_x_z_avg_diff / buf_len;
}

float common_utils_mag_var(struct accelerometer_data *buf, size_t buf_len)
{
	if (buf_len == 0) {
		return 0.0f; /* Avoid division by zero */
	}

	/* Calculate the magnitude of the acceleration vector for each data point */
	float magnitude_array[buf_len];
	for (int i = 0; i < buf_len; i++) {
		magnitude_array[i] = sqrt(pow(buf[i].x, 2) + pow(buf[i].y, 2) + pow(buf[i].z, 2));
	}

	/* Calculate the mean of the magnitude array */
	float magnitude_mean = 0.0f;
	for (int i = 0; i < buf_len; i++) {
		magnitude_mean += magnitude_array[i];
	}
	magnitude_mean /= buf_len;

	/* Calculate the variance of the magnitude array */
	float variance = 0.0f;
	for (int i = 0; i < buf_len; i++) {
		variance += (magnitude_array[i] - magnitude_mean) *
			    (magnitude_array[i] - magnitude_mean);
	}
	variance /= buf_len - 1;

	return variance;
}

float common_utils_x_z_sum_diff(struct accelerometer_data *buf, size_t buf_len)
{
	float x_z_sum_diff = 0.0f;
	for (int i = 0; i < buf_len; i++) {
		x_z_sum_diff += buf[i].x - buf[i].z;
	}
	return x_z_sum_diff;
}

float common_utils_hour_sin(float hour)
{
	/* Convert hour to radians */
	float radians = (hour * 2.0f * M_PI) / 24.0f;
	return sin(radians);
}

float common_utils_hour_cos(float hour)
{
	/* Convert hour to radians */
	float radians = (hour * 2.0f * M_PI) / 24.0f;
	return cos(radians);
}

#ifdef CONFIG_DEBUG
void common_utils_print_float_as_int(float *buf, size_t buf_size, char *string)
{
	printf("%s", string);
	for (size_t i = 0; i < buf_size / sizeof(float); i++) {
		if (buf[i] < 0.0f) {
			if ((int)buf[i] == 0) {
				printf("-%d.%06d ", (int)buf[i],
				       (int)((-buf[i] - (int)-buf[i]) * 1000000));

			} else {
				printf("%d.%06d ", (int)buf[i],
				       (int)((-buf[i] - (int)-buf[i]) * 1000000));
			}
		} else {
			printf("%d.%06d ", (int)buf[i], (int)((buf[i] - (int)buf[i]) * 1000000));
		}
	}
	printf("\n");
}
#endif /* CONFIG_DEBUG */
