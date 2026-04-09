/** @file reed.h
 *
 * @brief Interface for reed pin
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2023 Irnas. All rights reserved.
 */

#ifndef REED_H
#define REED_H

#include <zephyr/kernel.h>

/*!
 * @brief Init REED pin.
 * If reed pin is defined in DT, configure interrupt and action on detected active/low.
 *
 * @return negative error code, 0 is successful.
 */
int reed_init(void);

/**
 * @brief Safety feature to check level of reed pin and exit sleep mode if needed.
 *
 */
void reed_check(void);

#endif // REED_H
