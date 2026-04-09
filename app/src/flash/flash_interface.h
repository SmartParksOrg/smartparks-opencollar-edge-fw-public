/** @file flash_interface.h
 *
 * @brief Interface for flash and data logging
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2020 Irnas. All rights reserved.
 */

#ifndef FLASH_INTERFACE_H_
#define FLASH_INTERFACE_H_

#include "thread_com.h"
#include <zephyr/kernel.h>

/* Basic functionality */

/**
 * @brief Init external flash.
 *
 * Check if device is ready. Read current writing position from internal memory and clear next block
 * if required.
 * Write "test" reboot message to flash and read it back to check if flash is working.
 *
 * @return negative integer error code or 0 if ok.
 */
int init_flash(void);

/**
 * @brief Test flash by writing "reboot message" to flash.
 *
 * @return int error code or 0 if ok.
 */
int test_flash(void);

/**
 * @brief Disable external flash.
 *
 * Sets flash enable status to false.
 *
 */
void disable_flash(void);

/**
 * @brief Return flash status.
 *
 * @return status
 */
bool get_flash_status(void);

/**
 * @brief Clear flash data.
 *
 * Reset write offset and left space variables. Clear first block.
 *
 * @return negative integer error code or 0 if ok.
 */
int clear_flash_data(void);

/* Thread communication */

/**
 * @brief Check if any new message is received from other threads. Parse and execute message.
 *
 * @return integer error code in human readable form, defined in lsErrorToString() 0 - OK
 */
int handle_flash_thread_messages(void);

/*!
 * @brief Check if flash status needs to be send. Send if so.
 *
 * @return negative integer error code, 0 - OK
 */
int handle_flash_status();

#endif
