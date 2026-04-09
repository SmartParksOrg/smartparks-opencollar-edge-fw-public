/** @file battery_interface.c
 *
 * @brief Interface to handle battery and charging
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2020 Irnas. All rights reserved.
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/reboot.h>

#include <voltage_divider.h>

#include "battery_interface.h"
#include "common_functions.h"
#include "definitions.h"
#include "generated_settings.h"
#include "status.h"

LOG_MODULE_REGISTER(BATTERY_INTERFACE, 3);

#define BATTERY_N_SAMPLES_AVG       10
#define BATTERY_MEASURE_INTERVAL_MS BATTERY_READ_INTERVAL_MS

/* Local structure defines */
enum battery_level {
	BATTERY_FULL = 0,
	BATTERY_NORMAL = 1,
	BATTERY_LOW = 2,
	BATTERY_CRITICAL = 3,
	BATTERY_NO_DATA = 4,
};

struct battery_average {
	int val[BATTERY_N_SAMPLES_AVG];
	uint8_t N;
	uint8_t idx;
	int avg;
	enum battery_level bat_level;
};

/* Device bindings */
const struct device *battery_dev = DEVICE_DT_GET(DT_PATH(vbatt));

/* Local variables */
uint64_t last_battery_sample = 0; // Last time battery was sampled

struct battery_average bat_avg;

/* PRIVATE FUNCTIONS */

/**
 * @brief Initialize battery measurement structure.
 *
 * @param avg structure to initialize
 */
static void battery_interface_init_average(struct battery_average *avg)
{
	/* Initiate battery level */
	avg->bat_level = BATTERY_NO_DATA;

	/* Initialize average structure */
	for (int i = 0; i < BATTERY_N_SAMPLES_AVG; i++) {
		avg->val[i] = 0;
	}
	avg->idx = 0;
	avg->avg = 0;
	avg->N = 0;
}

/**
 * @brief Add new value and re-calculate average.
 *
 * @param new_val - new value
 * @param avg - average structure
 * @return int
 */
static int battery_interface_update_average(int new_val, struct battery_average *avg)
{
	// Add new measurement to array
	avg->val[avg->idx] = new_val;
	avg->idx = (avg->idx + 1) % BATTERY_N_SAMPLES_AVG;

	// Increase nr. of samples
	if (avg->N < BATTERY_N_SAMPLES_AVG) {
		avg->N++;
	}

	// Calculate new average
	avg->avg = 0;
	for (uint8_t i = 0; i < avg->N; i++) {
		avg->avg += avg->val[i];
	}
	avg->avg /= avg->N;

	return 0;
}

/**
 * @brief Analyze average battery level.
 * If not enough samples are collected, set level to no_data.
 * Otherwise, check old state and perform action.
 *
 * @param avg
 */
static void battery_interface_analyze_battery_level(struct battery_average *avg)
{
	// Check if we have enough samples to analyze data
	if (avg->N < BATTERY_N_SAMPLES_AVG) {
		LOG_DBG("Cannot get battery average, not enough samples collected!");
		// If previously we were in critical state, reboot
		if (avg->bat_level == BATTERY_CRITICAL) {
			LOG_WRN("System will reboot as battery reached critical level!");
			k_sleep(K_SECONDS(1));
			sys_reboot(0);
		}

		avg->bat_level = BATTERY_NO_DATA;
		return;
	}

	// Respond based on current battery level
	switch (avg->bat_level) {
	case BATTERY_NO_DATA: {
		if (avg->avg < CRITICAL_BATTERY_LEVEL) {
			// Go to hibernation mode
			LOG_WRN("Battery level critical: %d! Go to hibernation mode!", avg->avg);
			hibernation_mode();
			avg->bat_level = BATTERY_CRITICAL;
		} else if (avg->avg < LOW_BATTERY_LEVEL) {
			// Turn off all operation besides status send
			LOG_WRN("Battery level low: %d! Go to low power mode!", avg->avg);
			low_power_mode();
			avg->bat_level = BATTERY_LOW;
		} else if (avg->avg > FULL_BATTERY_LEVEL) {
			LOG_INF("Battery level full: %d.", avg->avg);
			avg->bat_level = BATTERY_FULL;
		} else {
			LOG_INF("Battery level normal: %d", avg->avg);
			// Return to normal operation
			avg->bat_level = BATTERY_NORMAL;
		}
		break;
	}
	case BATTERY_CRITICAL: {
		if (avg->avg > CRITICAL_BATTERY_LEVEL + BATTERY_THRESHOLD) {
			LOG_INF("Battery level not critical any more! Tracker will reboot to exit "
				"hibernation state!");
			k_sleep(K_SECONDS(1));
			sys_reboot(0);
		}
		break;
	}
	case BATTERY_LOW: {
		if (avg->avg < CRITICAL_BATTERY_LEVEL) {
			// Go to hibernation mode
			LOG_WRN("Battery level critical: %d! Go to hibernation mode!", avg->avg);
			hibernation_mode();
			avg->bat_level = BATTERY_CRITICAL;
		} else if (avg->avg > LOW_BATTERY_LEVEL + BATTERY_THRESHOLD) {
			LOG_INF("Battery level normal: %d", avg->avg);
			// Return to normal operation
			normal_mode();
			avg->bat_level = BATTERY_NORMAL;
		}
		break;
	}
	case BATTERY_NORMAL: {
		if (avg->avg < LOW_BATTERY_LEVEL) {
			// Turn off all operation besides status send
			LOG_WRN("Battery level low: %d! Go to low power mode!", avg->avg);
			low_power_mode();
			avg->bat_level = BATTERY_LOW;
		} else if (avg->avg > FULL_BATTERY_LEVEL) {
			LOG_INF("Battery level full: %d.", avg->avg);
			avg->bat_level = BATTERY_FULL;
		}
		break;
	}
	case BATTERY_FULL: {
		if (avg->avg < FULL_BATTERY_LEVEL - BATTERY_THRESHOLD) {
			LOG_INF("Battery level normal: %d", avg->avg);
			// Return to normal operation
			avg->bat_level = BATTERY_NORMAL;
		}
		break;
	}
	}

	return;
}

/* PUBLIC FUNCTIONS */

int battery_interface_init(void)
{
	/* Battery */
	if (battery_dev == NULL) {
		LOG_ERR("Battery device unknown");
		return -ENXIO;
	} else {
		if (!device_is_ready(battery_dev)) {
			LOG_ERR("Battery device defined, but not ready!");
			return -ENXIO;
		}
	}

	/* Initiate battery average sequence */
	battery_interface_init_average(&bat_avg);

	/* Test measure */
	int batt_mV;
	int err = battery_interface_measure(&batt_mV);
	// Check if in boundaries and store to values structure
	if (!err) {
		LOG_INF("Battery: %d mV", batt_mV);
	}

	return err;
}

int battery_interface_measure(int *val)
{
	if (battery_dev == NULL) {
		LOG_ERR("Battery Device unknown");
		return -ENXIO;
	}

	int err = 0;
	int batt_mV = voltage_divider_sample(battery_dev);

	// Check if in boundaries and store to values structure
	if (batt_mV < 0) {
		LOG_ERR("Failed to read battery, error: %d.", batt_mV);
		err = batt_mV;
	} else {
		/* For some HW we need to manipulate value */
#ifdef CONFIG_BOARD_CATTRACKER_NRF52840
		if (CONFIG_BOARD_REVISION == '2.0.0') // EvaTODO - does this work?
		{
			LOG_DBG("We should modify battery val from: %d", batt_mV);
			if (batt_mV < 3285) {
				batt_mV = 4250 - (int)((267 * batt_mV) / 1000);
			}
			LOG_DBG("New val: %d", batt_mV);
		}
#endif
		if (batt_mV < Main_values.batt_mV->min || batt_mV > Main_values.batt_mV->max) {
			LOG_ERR("Battery measurement out of boundaries, measured: %d mV.", batt_mV);
			err = -ERANGE;
		} else {
			LOG_DBG("Battery: %d mV", batt_mV);
			*val = batt_mV;
		}
	}

	return err;
}

int battery_interface_handler(void)
{
	int err = 0;

	if (k_uptime_get() - last_battery_sample > BATTERY_MEASURE_INTERVAL_MS ||
	    last_battery_sample == 0) {
		int batt_mV = 0;
		err = battery_interface_measure(&batt_mV);
		if (!err) {
			Main_values.batt_mV->def_val = batt_mV;
			battery_interface_update_average(batt_mV, &bat_avg);
			battery_interface_analyze_battery_level(&bat_avg);
		}
		sys_err.bat = err;

		last_battery_sample = k_uptime_get();
	}

	return err;
}
