/** @file operation_test.h
 *
 * @brief Test module
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2021 Irnas. All rights reserved.
 */

#ifndef OPERATION_TEST_H
#define OPERATION_TEST_H

#include <zephyr/kernel.h>
#include <zephyr/types.h>

/**
 * @brief Configure shell and output user message.
 * Function disabled shell echo and prints out user test message.
 *
 */
void operation_test_setup(void);

/**
 * @brief Enter low power state.
 *
 */
void test_low_power(void);

#endif // OPERATION_TEST_H
