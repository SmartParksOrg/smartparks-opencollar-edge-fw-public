/** @file global_time.c
 *
 * @brief Interface for tracking time trough application
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2020 Irnas. All rights reserved.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "generated_settings.h"
#include "global_time.h"
#include "nvs_storage.h"

LOG_MODULE_REGISTER(global_time); // init logging

// Set time
uint32_t global_unix_time = 0;
uint64_t global_unix_time_ms = 0;
uint32_t global_gps_time = 0;
uint64_t global_time_update = 0;

/*!
 * @brief Initialize unix time based on stored value.
 *
 * @return /
 */
void init_ref_time(void)
{
	// Initialize time
	// Read latest time
	uint32_t ref_time = 0;
	nvs_storage_read(STORAGE_unix_time, &ref_time, sizeof(ref_time));
	LOG_INF("Stored time: %d, settings time: %d", ref_time, Main_settings.init_time->def_val);
	if (ref_time < Main_settings.init_time->def_val) {
		ref_time = Main_settings.init_time->def_val;
		nvs_storage_write(STORAGE_unix_time, &Main_settings.init_time->def_val,
				  sizeof(Main_settings.init_time->def_val));
	}

	Main_values.ublox_time->def_val = ref_time;
	update_time();
}

/*!
 * @brief Update reference unix time.
 *
 * @return /
 */
void update_ref_time(uint32_t new_time)
{
	if (new_time > global_unix_time) {
		Main_values.ublox_time->def_val = new_time;
		LOG_INF("Got new reference time: %d", Main_values.ublox_time->def_val);
		nvs_storage_write(STORAGE_unix_time, &Main_values.ublox_time->def_val,
				  sizeof(Main_values.ublox_time->def_val));
		global_time_update = k_uptime_get();
	}
	update_time();
}

/*!
 * @brief Reset time reference from settings.
 *
 *
 * @return /
 */
void reset_time_from_settings(void)
{
	uint32_t ref_time = Main_settings.init_time->def_val;
	nvs_storage_write(STORAGE_unix_time, &Main_settings.init_time->def_val,
			  sizeof(Main_settings.init_time->def_val));
	Main_values.ublox_time->def_val = ref_time;
	global_time_update = k_uptime_get();
	update_time();
}

/*!
 * @brief Update unix time based on a reference time and elapsed time since last update.
 *
 *
 * @return /
 */
void update_time(void)
{
	uint64_t elapsed_ms = (uint64_t)(k_uptime_get() - global_time_update);
	uint64_t elapsed_sec = elapsed_ms / 1000;
	global_unix_time_ms = ((uint64_t)Main_values.ublox_time->def_val * 1000) + elapsed_ms;
	global_unix_time = Main_values.ublox_time->def_val + (uint32_t)elapsed_sec;
	global_gps_time = unix_to_gps(global_unix_time);
	nvs_storage_write(STORAGE_unix_time, &global_unix_time, sizeof(global_unix_time));
	LOG_DBG("New unix: %d, elapsed time since last reference update: %d s", global_unix_time,
		(uint32_t)elapsed_sec);
}

/*!
 * @brief Convert unix time to gps.
 *
 *
 * @return /
 */
uint32_t unix_to_gps(uint32_t unix_t)
{
	return unix_t - 315964800;
}

/*!
 * @brief Update and return unix time.
 *
 *
 * @return /
 */
uint32_t get_global_unix_time(void)
{
	update_time();

	return global_unix_time;
}

/*!
 * @brief Get the current unix time in milliseconds.
 *
 *
 * @return /
 */
uint64_t get_unix_time_in_ms(void)
{
	update_time();

	return global_unix_time_ms;
}

/*!
 * @brief Update and return gps time.
 *
 *
 * @return /
 */
uint32_t get_global_gps_time(void)
{
	update_time();

	return global_gps_time;
}
