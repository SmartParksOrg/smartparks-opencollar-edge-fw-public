/** @file pangolin_logic.h
 *
 * @brief This file contains the logic for outdoor detection for Pangolins.
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2025 Irnas.  All rights reserved.
 */

#ifndef PANGOLIN_LOGIC_H
#define PANGOLIN_LOGIC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <zephyr/kernel.h>

#include <common_utils.h>

/**
 * @brief Configuration structure for Pangolin outdoor detection logic.
 *
 * This structure contains the configuration parameters for the outdoor detection logic
 * specific to Pangolins. It includes bias values, weights for the temperature sensor,
 * accelerometer sensor, and hour of day.
 */
struct pangolin_logic_cfg {
	int16_t bias;                 /* Bias value for the temperature sensor */
	int16_t temperature_weight;   /* Weight for the temperature sensor */
	int16_t accelerometer_weight; /* Weight for the accelerometer sensor */
	int16_t hour_weight;          /* Weight for the hour of day */
	int32_t time_offset_seconds;  /* Time offset in seconds */
};

struct pangolin_logic_data {
	float *temp_mean_history_values_buf; /* Pointer to the array of mean temperature history
						values */
	size_t temp_mean_history_values_buf_size; /* Size of the mean temperature history values
						     buffer */
	float *temp_inter_fix_values; /* Pointer to the array of inter-fix temperature values */
	size_t temp_inter_fix_i;      /* Number of inter-fix temperature values */

	struct accelerometer_data
		*acc_inter_fix_values; /* Pointer to the array of inter-fix accelerometer values */
	size_t acc_inter_fix_i;        /* Number of inter-fix accelerometer values */
	uint32_t first_inter_fix_time; /* Time of first inter-fix cycle */
};

/**
 * @brief Calculate the outdoor probability based on the current sensor data.
 *
 * @param[out] pangolin_calculation_ctx Pointer to the context structure.
 *
 * @retval 0 on success.
 * @retval -EINVAL if one of the pointers in @a pangolin_calculation_ctx is NULL.
 * @retval -ERANGE if the outdoor_probability value is out of range.
 */
int pangolin_outdoor_probability_calculation(float *outdoor_probability,
					     struct pangolin_logic_cfg cfg,
					     struct pangolin_logic_data *data);

#ifdef __cplusplus
}
#endif

#endif /* PANGOLIN_LOGIC_H */
