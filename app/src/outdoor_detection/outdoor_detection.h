/** @file outdoor_detection.h
 *
 * @brief This file contains the interface for the outdoor detection module. This module is
 * primarily used for detecting when the device is outdoors, so a GPS fix can be performed at that
 * moment.
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2025 Irnas.  All rights reserved.
 */

#ifndef OUTDOOR_DETECTION_H
#define OUTDOOR_DETECTION_H

#include <stdbool.h>
#include <zephyr/kernel.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the outdoor detection module.
 *
 * @return int 0 on success, negative value on error.
 */
int outdoor_detection_init(void);

/**
 * @brief handle the outdoor detection logic. This function should be called periodically.
 *
 */
void outdoor_detection_handle(void);

/**
 * @brief Get the current outdoor detection status.
 *
 * @return true if outdoor is detected, false otherwise.
 */
bool outdoor_detection_get_status(void);

/**
 * @brief Clear the outdoor detection status.
 *
 * This function resets the outdoor detection status to false.
 */
void outdoor_detection_clear_status(void);

/**
 * @brief Check if the outdoor detection is ready to be checked.
 *
 * This function is meant to be used by outside modules to check if the outdoor detection interval
 * elapsed.
 *
 * @return true if ready to check, false otherwise.
 */
bool outdoor_detection_get_fix_interval_elapsed(void);

/**
 * @brief Clear the outdoor detection fix interval elapsed flag.
 *
 * This function resets the flag that indicates if the outdoor detection fix interval has elapsed.
 * This function is added because we need to reset the flag after outside modules have read it.
 */
void outdoor_detection_clear_fix_interval_elapsed(void);

#ifdef __cplusplus
}
#endif

#endif /* OUTDOOR_DETECTION_H */
