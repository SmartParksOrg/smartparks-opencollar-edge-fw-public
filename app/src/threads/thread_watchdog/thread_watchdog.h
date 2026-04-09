/** @file thread_watchdog.h
 *
 * @brief Watchdog for threads.
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2020 Irnas. All rights reserved.
 */

#ifndef THREAD_WATCHDOG_H
#define THREAD_WATCHDOG_H

#include <zephyr/kernel.h>

/* Threads max response time */
#define THREAD_MAIN_MAX_RESPONSE   60                         // In seconds
#define THREAD_SENSOR_MAX_RESPONSE 45                         // In seconds
#define THREAD_LR_GPS_MAX_RESPONSE 900                        // In seconds
#define THREAD_FLASH_MAX_RESPONSE  THREAD_LR_GPS_MAX_RESPONSE // 30  //In seconds

/*!
 * @brief Initialize and start Watchdog.
 * Start timer periodically feeding watchdog.
 *
 * @retval  Return 0 if ok, otherwise negative error status.
 */
int init_watchdog(void);

/*!
 * @brief Main thread report.
 *
 * @retval  void.
 */
void main_thread_report(void);

/*!
 * @brief Sensor thread report.
 *
 * @retval  void.
 */
void sensor_thread_report(void);

/*!
 * @brief LR and GPS thread report.
 *
 * @retval  void.
 */
void lr_gps_thread_report(void);

/*!
 * @brief Flash thread report.
 *
 * @retval  void.
 */
void flash_thread_report(void);

#endif
