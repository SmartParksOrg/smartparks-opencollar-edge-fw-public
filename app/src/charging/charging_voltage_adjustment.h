/** @file charging_voltage_adjustment.h
 *
 * @brief Interface that adjusts (interpolates) the provided voltage measurement
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2024 Irnas.  All rights reserved.
 */

#ifndef CHARGING_VOLTAGE_ADJUSTMENT_H
#define CHARGING_VOLTAGE_ADJUSTMENT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <zephyr/kernel.h>

/**
 * @brief adjusts the supplied voltage
 *
 * @param[in, out] voltage_ptr pointer to the measurement that will be adjusted
 *
 * @retval 0 if the voltage is adjusted successfully
 * @retval -1 if the provided charging voltage is too small.
 * @retval -2 if the provided charging voltage is too big.
 */
int charging_voltage_adjust(int *voltage_ptr);

#ifdef __cplusplus
}
#endif

#endif /* CHARGING_VOLTAGE_ADJUSTMENT_H */
