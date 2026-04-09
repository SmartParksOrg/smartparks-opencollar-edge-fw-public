/** @file battery_interface.h
 *
 * @brief Interface to handle battery and charging
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2023 Irnas. All rights reserved.
 */

#ifndef BATTERY_INTERFACE_H
#define BATTERY_INTERFACE_H

#include <zephyr/kernel.h>

/** Setup battery voltage divider.
 *
 * @return zero on success, or a negative error code.
 */
int battery_interface_init(void);

/**
 * @brief Measure battery. If measurement is successful, copy value in provided argument.
 * Keep in mind function will not update averaging structure. Call battery_interface_update_level()
 * for that instead.
 *
 * @param val[out] measurement value
 * @return int zero on success, or a negative error code.
 */
int battery_interface_measure(int *val);

int battery_interface_handler(void);

#endif
