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

LOG_MODULE_REGISTER(global_time, LOG_LEVEL_INF); // init logging

// Set time
uint32_t global_unix_time = 0;
uint64_t global_unix_time_ms = 0;
uint32_t global_gps_time = 0;
uint64_t global_time_update = 0;

/* Serialize the reference, uptime anchor and persistent copy across all readers.
 *
 * We couldn't use atomic assignments here because they would still let a reader combine a new
 * reference with the old uptime anchor, producing an incorrect timestamp. The whole update must be
 * protected together.
 */
K_MUTEX_DEFINE(time_lock);
static void update_time_locked(void);

/*!
 * @brief Initialize unix time based on stored value.
 *
 * @return /
 */
void init_ref_time(void)
{
	/* Prevent clock access from interleaving with loading and installing the stored reference.
	 */
	k_mutex_lock(&time_lock, K_FOREVER);
	// Initialize time
	// Read latest time
	uint32_t ref_time = 0;
	nvs_storage_read(STORAGE_unix_time, &ref_time, sizeof(ref_time));
	LOG_INF("Stored time: %d, settings time: %d", ref_time, Main_settings.init_time->def_val);
	if (ref_time < Main_settings.init_time->def_val) {
		ref_time = Main_settings.init_time->def_val;
	}

	Main_values.ublox_time->def_val = ref_time;
	global_time_update = k_uptime_get();
	update_time_locked();
	k_mutex_unlock(&time_lock);
}

/*!
 * @brief Update reference unix time.
 *
 * @return /
 */
void update_ref_time(uint32_t new_time)
{
	/* Keep the new reference, its uptime anchor and the persisted time consistent for readers.
	 */
	k_mutex_lock(&time_lock, K_FOREVER);
	/* GPS validation happens before this call. Accept backward corrections too. */
	Main_values.ublox_time->def_val = new_time;
	global_time_update = k_uptime_get();
	update_time_locked();
	LOG_INF("Updated reference time: %u", new_time);
	k_mutex_unlock(&time_lock);
}

/*!
 * @brief Reset time reference from settings.
 *
 *
 * @return /
 */
void reset_time_from_settings(void)
{
	/* Prevent readers or GPS updates from observing a partially reset clock reference. */
	k_mutex_lock(&time_lock, K_FOREVER);
	uint32_t ref_time = Main_settings.init_time->def_val;
	Main_values.ublox_time->def_val = ref_time;
	global_time_update = k_uptime_get();
	update_time_locked();
	k_mutex_unlock(&time_lock);
}

/*!
 * @brief Update unix time based on a reference time and elapsed time since last update.
 *
 *
 * @return /
 */
void update_time(void)
{
	/* Keep the reference stable during calculation and serialize writes to stored time. */
	k_mutex_lock(&time_lock, K_FOREVER);
	update_time_locked();
	k_mutex_unlock(&time_lock);
}

/**
 * @brief Refresh application time from its reference and elapsed uptime, then store it.
 *
 * Update the Unix seconds, Unix milliseconds and GPS time values together and write
 * the current Unix seconds to nonvolatile storage.
 *
 * @pre The calling thread holds time_lock.
 */
static void update_time_locked(void)
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
	/* Refresh and copy the timestamp under one lock so another update cannot replace it. */
	k_mutex_lock(&time_lock, K_FOREVER);
	update_time_locked();
	uint32_t timestamp = global_unix_time;
	k_mutex_unlock(&time_lock);
	return timestamp;
}

/*!
 * @brief Get the current unix time in milliseconds.
 *
 *
 * @return /
 */
uint64_t get_unix_time_in_ms(void)
{
	/* Protect both the refresh and the 64-bit read, which can tear on this 32-bit target. */
	k_mutex_lock(&time_lock, K_FOREVER);
	update_time_locked();
	uint64_t timestamp = global_unix_time_ms;
	k_mutex_unlock(&time_lock);
	return timestamp;
}

/*!
 * @brief Update and return gps time.
 *
 *
 * @return /
 */
uint32_t get_global_gps_time(void)
{
	/* Keep GPS time calculation and the returned snapshot tied to the same reference. */
	k_mutex_lock(&time_lock, K_FOREVER);
	update_time_locked();
	uint32_t timestamp = global_gps_time;
	k_mutex_unlock(&time_lock);
	return timestamp;
}
