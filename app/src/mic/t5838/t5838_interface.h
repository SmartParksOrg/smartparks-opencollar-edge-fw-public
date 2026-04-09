/** @file t5838.h
 *
 * @brief Microphone t5838 functions
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2024 Irnas. All rights reserved.
 */

#ifndef T5838_H
#define T5838_H

#include <zephyr/audio/dmic.h>
#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>

/**
 * @brief init microphone
 *
 * This configures the microphone and puts it into a low-power state.
 *
 * @return int 0 on success, negative error code otherwise
 */
int t5838_init(void);

#endif
