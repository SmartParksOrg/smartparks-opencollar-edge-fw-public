/** @file charging_voltage_adjustment.c
 *
 * @brief Interface that adjusts (interpolates) the provided voltage measurement
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2024 Irnas. All rights reserved.
 */

#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <charging_voltage_adjustment.h>

LOG_MODULE_REGISTER(CHARGING_VOLTAGE_ADJUSTMENT);

static int starting_voltage_mV = 4000;

/**
 * This array is used for voltage adjustment. Measurements are in mV.
 *
 * The array consists of (at different battery levels, averaged) charging voltage measurements
 * performed by the device at charging voltages ranging from 4V to 31V, increasing by 1V. These
 * values are used to interpolate the correct charging voltage.
 */
static const uint16_t charging_voltage_averaged_measurements[28] = {
	3034,  4021,  4877,  5696,  6798,  7867,  8956,  10074, 11163, 12220,
	13275, 14372, 15430, 16508, 17597, 18649, 19726, 20792, 21860, 22922,
	23992, 25086, 26110, 27228, 28252, 29328, 30377, 31456};

/* Maximum allowed under-flowing / over-flowing voltage (mV)  */
#define CHARGING_VOLTAGE_MAX_OVER_UNDER_FLOW 500

/**
 * @brief find the index of the closest voltage in the charging_voltage_averaged_measurements array
 *
 * @param[in] voltage measured voltage
 *
 * @retval int -1 if voltage is smaller than the smallest value in the
 * charging_voltage_averaged_measurements array (padded by CHARGING_VOLTAGE_MAX_OVER_UNDER_FLOW)
 * @retval int -2 if voltage is greater than the greatest value in the
 * charging_voltage_averaged_measurements array (padded by CHARGING_VOLTAGE_MAX_OVER_UNDER_FLOW)
 * @retval int index of the closest voltage in the charging_voltage_averaged_measurements array
 */
static int prv_find_closest_voltage_index(uint16_t voltage)
{
	if (voltage <
	    charging_voltage_averaged_measurements[0] - CHARGING_VOLTAGE_MAX_OVER_UNDER_FLOW) {
		return -1;
	}
	if (voltage >
	    charging_voltage_averaged_measurements[27] + CHARGING_VOLTAGE_MAX_OVER_UNDER_FLOW) {
		return -2;
	}

	for (int i = 1; i < ARRAY_SIZE(charging_voltage_averaged_measurements); i++) {
		if (charging_voltage_averaged_measurements[i] > voltage) {
			return i - 1;
		}
	}
	return ARRAY_SIZE(charging_voltage_averaged_measurements) - 1;
}

/**
 * @brief Linear interpolation function
 *
 * This function interpolates the value of y for a given x, using two known
 * points (x1, y1) and (x2, y2).
 *
 * @param[in] x1 x coordinate of the first known point
 * @param[in] y1 y coordinate of the first known point
 * @param[in] x2 x coordinate of the second known point
 * @param[in] y2 y coordinate of the second known point
 * @param[in] x x coordinate to interpolate
 *
 * @retval (double) interpolated y
 */
static double prv_linear_interpolation(double x1, double y1, double x2, double y2, double x)
{
	return y1 + (y2 - y1) / (x2 - x1) * (x - x1);
}

int charging_voltage_adjust(int *voltage_ptr)
{
	int voltage = *voltage_ptr;
	int closest_voltage_index = prv_find_closest_voltage_index(voltage);

	if (closest_voltage_index == -1) {
		LOG_WRN("Charging voltage too low");
		return -1;
	} else if (closest_voltage_index == -2) {
		LOG_WRN("Charge voltage too high");
		return -2;
	}

	double interpolated_voltage = prv_linear_interpolation(
		charging_voltage_averaged_measurements[closest_voltage_index],
		closest_voltage_index * 1000 + starting_voltage_mV,
		charging_voltage_averaged_measurements[closest_voltage_index + 1],
		(closest_voltage_index + 1) * 1000 + starting_voltage_mV, voltage);

	*voltage_ptr = (int)interpolated_voltage;
	return 0;
}
