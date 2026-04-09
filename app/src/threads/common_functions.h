/** @file common_functions.h
 *
 * @brief File containing globally used functions
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2020 Irnas. All rights reserved.
 */

#ifndef COMMON_FUNCTIONS_H
#define COMMON_FUNCTIONS_H

#include <zephyr/kernel.h>

/* mutex */
#define SPI_MUTEX_TIMEOUT 30000 // Wait for mutex lock in ms
extern struct k_mutex spi_mutex;

/*!
 * @brief Init all thread mutexes.
 *
 * @retval 0 on success.
 */
int thread_init_mutex(void);

/*!
 * @brief Hibernation mode.
 */
void hibernation_mode(void);

/*!
 * @brief Low power mode.
 */
void low_power_mode(void);

/*!
 * @brief Normal power mode.
 */
void normal_mode(void);

/**
 * @brief Read factory name and save it to provided buffer.
 *
 * @param[out] uint8_t name buffer
 */
void read_factory_name(uint8_t *name);

/**
 * @brief Handle system errors. If any errors
 */
void handle_errors(void);

#endif // COMMON_FUNCTIONS_H
