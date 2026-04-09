/** @file global_time.h
 *
 * @brief Interface for tracking time trough application
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2020 Irnas. All rights reserved.
 */

#ifndef GLOBAL_TIME_H
#define GLOBAL_TIME_H

#include <zephyr/kernel.h>

void init_ref_time(void);
void update_ref_time(uint32_t);
void reset_time_from_settings(void);
void update_time(void);
uint32_t unix_to_gps(uint32_t);

// Get functions
uint32_t get_global_unix_time(void);
uint64_t get_unix_time_in_ms(void);
uint32_t get_global_gps_time(void);

#endif
