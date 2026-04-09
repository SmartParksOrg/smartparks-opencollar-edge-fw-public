/** @file charging_interface.h
 *
 * @brief Interface to handle battery and charging
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2023 Irnas. All rights reserved.
 */

#ifndef CHARGING_INTERFACE_H
#define CHARGING_INTERFACE_H

#include <zephyr/kernel.h>

/** Setup charging voltage divider.
 * If applicable, initialize charging disable pin and attach interrupt to charging status pin.
 *
 * @return zero on success, or a negative error code.
 */
int charging_interface_init(void);

/**
 * @brief Measure charging voltage. If measurement is successful, copy value in provided argument.
 *
 * @param val[out] measurement value
 * @return int zero on success, or a negative error code.
 */
int charging_interface_measure(int *val);

/**
 * @brief Enable charging by setting chg disable pin to logical INACTIVE
 *
 * @return int
 */
int charging_enable(void);

/**
 * @brief Disable charging by setting chg disable pin to logical ACTIVE
 *
 * @return int
 */
int charging_disable(void);

int charging_interface_handler(void);

#endif // CHARGING_INTERFACE_H
