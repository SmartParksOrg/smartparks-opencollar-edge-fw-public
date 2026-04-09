/** @file vm1010.h
 *
 * @brief Driver for microphone VM1010
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2020 Irnas. All rights reserved.
 */

#ifndef VM1010_H
#define VM1010_H

#include <zephyr/kernel.h>

/*!
 * @brief Initialize vm1010.
 *
 * @return negative integer error code or 0 if ok.
 */
int vm1010_init(void);

/*!
 * @brief Disable vm1010.
 *
 * @return negative integer error code or 0 if ok.
 */
int vm1010_disable(void);

/**
 * @brief Sample mic - trivial sampling implemented.
 *
 * @return int
 */
int vm1010_sample(void);

/*!
 * @brief Change mic operation mode.
 *
 * @param mode 0 - normal, 1 - wake on sound
 *
 * @return negative integer error code or 0 if ok.
 */
int vm1010_set_mode(uint8_t mode);

/*!
 * @brief Periodically check mic.
 *
 * @return 1 if sound was detected, 0 if not, negative error otherwise.
 */
int vm1010_get_detected(void);

/*!
 * @brief Periodically check mic.
 *
 * @return negative integer error code or 0 if ok.
 */
int handle_microphone(void);

#endif
