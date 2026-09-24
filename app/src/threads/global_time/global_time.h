/** @file global_time.h
 *
 * @brief Interface for tracking time trough application
 *
 * All functions except unix_to_gps() acquire an internal mutex and access nonvolatile
 * storage. Call them from thread context, where blocking is permitted.
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2020 Irnas. All rights reserved.
 */

#ifndef GLOBAL_TIME_H
#define GLOBAL_TIME_H

#include <zephyr/kernel.h>

/**
 * @brief Initialize the clock reference from stored time and the initial-time setting.
 *
 * Use the later of the stored Unix time and Main_settings.init_time, anchor it to
 * current uptime, and refresh the calculated and stored time values.
 */
void init_ref_time(void);

/**
 * @brief Set a validated UTC reference and anchor it to current uptime.
 *
 * Accept forward and backward corrections, then refresh the calculated and stored
 * time values. Callers must validate GPS samples before passing them here;
 * gps_ublox_get_datetime() performs that validation.
 *
 * @param[in] new_time Validated Unix time in seconds.
 */
void update_ref_time(uint32_t new_time);

/**
 * @brief Reset the clock reference to Main_settings.init_time.
 *
 * Anchor the configured Unix time to current uptime and refresh the calculated and
 * stored time values, even if the configured time is earlier than the current clock.
 */
void reset_time_from_settings(void);

/**
 * @brief Refresh application time from its reference and elapsed uptime.
 *
 * Update Unix seconds, Unix milliseconds and GPS time together, then write the
 * current Unix seconds to nonvolatile storage.
 */
void update_time(void);

/**
 * @brief Convert Unix seconds to the application's GPS-epoch time representation.
 *
 * Subtract the epoch offset of 315964800 seconds without applying leap-second adjustments.
 *
 * @param[in] unix_t Unix time in seconds, at or after the GPS epoch.
 * @return Seconds relative to the GPS epoch using the fixed epoch offset.
 */
uint32_t unix_to_gps(uint32_t unix_t);

/**
 * @brief Refresh application time and return a consistent Unix-seconds snapshot.
 *
 * Write the refreshed Unix seconds to nonvolatile storage before returning.
 *
 * @return Current application Unix time in seconds.
 */
uint32_t get_global_unix_time(void);

/**
 * @brief Refresh application time and return a consistent Unix-milliseconds snapshot.
 *
 * Write the refreshed Unix seconds to nonvolatile storage before returning.
 *
 * @return Current application Unix time in milliseconds.
 */
uint64_t get_unix_time_in_ms(void);

/**
 * @brief Refresh application time and return a consistent GPS-epoch snapshot.
 *
 * Write the refreshed Unix seconds to nonvolatile storage before returning.
 *
 * @return Current application time converted by unix_to_gps(), in seconds.
 */
uint32_t get_global_gps_time(void);

#endif
