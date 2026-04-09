/** @file uart_pm.h
 *
 * @brief Interface for power management of uart peripheral
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2022 Irnas. All rights reserved.
 */

#ifndef UART_PM_H
#define UART_PM_H

#include <zephyr/kernel.h>
#include <zephyr/types.h>

/**
 * @brief Disable UART peripheral.
 *
 * @param name device name
 * @return int o on success or -EIO if device name doesn't match uart0 or uart1 device.
 */
int uart_pm_disable(const char *name);

/**
 * @brief Enable UART peripheral.
 *
 * @param name device name
 * @return int o on success or -EIO if device name doesn't match uart0 or uart1 device.
 */
int uart_pm_enable(const char *name);

#endif // UART_PM_H
