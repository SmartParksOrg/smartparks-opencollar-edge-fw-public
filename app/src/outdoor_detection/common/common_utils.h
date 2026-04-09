/** @file common_utils.h
 *
 * @brief This file contains common utility functions and definitions used by outdoor detection
 * profiles.
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2025 Irnas.  All rights reserved.
 */

#ifndef COMMON_UTILS_H
#define COMMON_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <zephyr/kernel.h>

struct accelerometer_data {
	float x;
	float y;
	float z;
};

/**
 * @brief Calculate the absolute deviation of the X newly sampled samples from the mean of the last
 * 20 readings.
 *
 * NOTE: Newly sampled values should have it's mean in the mean buffer at the time of calculation.
 *
 * Expert report 'Paul_Kasko_mixture_of_experts.pdf` notes this function as `temp_mean_dev_z_20`.
 *
 * @param[in] mean_values_buf Pointer to the array of mean values (at least 20 values).
 * @param[in] mean_values_buf_len Length of the mean values buffer.
 * @param[in] new_values_buf Pointer to the array of new values.
 * @param[in] new_values_buf_len Number of new values.
 * @param[out] mean_dev_z_20 Pointer to the float where the calculated mean deviation will be
 * stored.
 *
 * @return int 0 on success, negative value on error.
 */
int common_utils_mean_dev_z_20(float *mean_values_buf, size_t mean_values_buf_len,
			       float *new_values_buf, int new_values_buf_len, float *mean_dev_z_20);

/**
 * @brief calculate maximum absolute deviation of the provided array.
 *
 * Expert report 'Paul_Kasko_mixture_of_experts.pdf` notes this function as `temp_max_z_diff`.
 *
 * @param[in] float_array Pointer to the array of floats.
 * @param[in] float_array_len Number of values in float_array.
 *
 * @return Maximum absolute deviation.
 */
float common_utils_max_z_diff(float *float_array, size_t float_array_len);

/**
 * @brief Calculate the mean of the sum between Y- and Z-axis of the provided accelerometer data.
 *
 * Expert report 'Paul_Kasko_mixture_of_experts.pdf` notes this function as `acc_y+z_mean`.
 *
 * @param[in] buf Pointer to the array of accelerometer data.
 * @param[in] buf_len Number of accelerometer data samples.
 *
 * @return Mean of the sum between Y- and Z-axis.
 */
float common_utils_y_z_mean(struct accelerometer_data *buf, size_t buf_len);

/**
 * @brief The maximum difference between the X- and Z-axis acceleration from the provided
 * accelerometer data.
 *
 * Expert report 'Paul_Kasko_mixture_of_experts.pdf` notes this function as `acc_x-z_max`.
 *
 * @param[in] buf Pointer to the array of accelerometer data.
 * @param[in] buf_len Number of accelerometer data samples.
 *
 * @return Maximum difference between X- and Z-axis acceleration.
 */
float common_utils_x_z_max_diff(struct accelerometer_data *buf, size_t buf_len);

/**
 * @brief The average difference between X- and Z-axis acceleration.
 *
 * Expert report 'Paul_Kasko_mixture_of_experts.pdf` notes this function as `acc_x-z_mean`.
 *
 * @param[in] buf Pointer to the array of accelerometer data.
 * @param[in] buf_len Number of accelerometer data samples.
 *
 * @return The average difference between X- and Z-axis acceleration.
 */
float common_utils_x_z_avg_diff(struct accelerometer_data *buf, size_t buf_len);

/**
 * @brief The variance of the provided acceleration vector magnitude.
 *
 * Expert report 'Paul_Kasko_mixture_of_experts.pdf` notes this function as `mag_var`.
 *
 * @param[in] buf Pointer to the array of accelerometer data.
 * @param[in] buf_len Number of accelerometer data samples.
 *
 * @return The variance of the acceleration vector magnitude.
 */
float common_utils_mag_var(struct accelerometer_data *buf, size_t buf_len);

/**
 * @brief The total cumulative difference between X- and Z-axis of the provided accelerometer data.
 *
 * Expert report 'Paul_Kasko_mixture_of_experts.pdf` notes this function as `acc_x-z_sum`.
 *
 * @param[in] buf Pointer to the array of accelerometer data.
 * @param[in] buf_len Number of accelerometer data samples.
 *
 * @return Cumulative difference between X- and Z-axis.
 */
float common_utils_x_z_sum_diff(struct accelerometer_data *buf, size_t buf_len);

/**
 * @brief A cyclical encoding of the hour of day using the sine function.
 *
 * Expert report 'Paul_Kasko_mixture_of_experts.pdf` notes this function as `hour_sin`.
 *
 * @param[in] hour
 *
 * @return float.
 */
float common_utils_hour_sin(float hour);

/**
 * @brief A cyclical encoding of the hour of day using the cosine function
 *
 * Expert report 'Paul_Kasko_mixture_of_experts.pdf` notes this function as `hour_cos`.
 *
 * @param[in] hour
 *
 * @return float
 */
float common_utils_hour_cos(float hour);

/* -------------- Misc -------------- */

#ifdef CONFIG_DEBUG
/**
 * @brief Print a float value as an integer in a string format.
 *
 * This function is only used for debugging purposes.
 *
 * This function is used for printing floats because using CONFIG_NEWLIB_LIBC_FLOAT_PRINTF results
 * in `errata 103`.
 *
 * @param[in] buf Pointer to the float buffer containing the value to be printed.
 * @param[in] buf_size Size of the buffer.
 * @param[in] string that will be printed before the float value.
 */
void common_utils_print_float_as_int(float *buf, size_t buf_size, char *string);
#endif /* CONFIG_DEBUG */

#ifdef __cplusplus
}
#endif

#endif /* COMMON_UTILS_H */
