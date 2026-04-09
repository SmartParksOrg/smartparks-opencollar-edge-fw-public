/** @file vhf.h
 *
 * @brief VHF - very high frequency burst transmission functionality.
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2024 Irnas.  All rights reserved.
 */

#ifndef VHF_H
#define VHF_H

#ifdef __cplusplus
extern "C" {
#endif

#include <zephyr/kernel.h>
#include <zephyr/types.h>

/**
 * @brief Get the time interval we are currently in.
 *
 * NOTE: If both intervals are set to the same hour, interval 1 is selected.
 *
 * The function checks if we are in-between or outside of the two intervals. The classification of
 * being in-between and outside of the two intervals is daily separated, meaning that in-between
 * represents the time that is between the two intervals on the same day, while outside represents
 * the time that is outside of the two intervals on the same day.
 *
 * If we are in-between the two intervals, the preceding interval is selected.
 *
 * If we are outside of the two intervals, the subsequent interval is selected.
 *
 * @return int interval 1 or 2
 */
uint8_t vhf_get_current_time_interval(void);

/**
 * @brief Configure LoRaWAN module for VHF burst transmission.
 *
 * @param context - LoRaWAN module context
 *
 * @return int 0 on success, negative error code otherwise
 */
int vhf_configure(const void *context);

/**
 * @brief Transmit a VHF burst.
 *
 * See README.md for more information regarding user configurable settings.
 */
void vhf_send_burst(void);

/**
 * @brief Get the VHF burst minimum allowed interval duration in seconds.
 *
 * @return int The minimum allowed interval duration.
 */
int vhf_get_interval_min_seconds(void);

#ifdef __cplusplus
}
#endif

#endif /* VHF_H */
