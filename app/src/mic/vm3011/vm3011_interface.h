/** @file vm3011_interface.h
 *
 * @brief Interface for microphone
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2022 Irnas. All rights reserved.
 */

#ifndef VM3011_INTERFACE_H
#define VM3011_INTERFACE_H

#include <zephyr/audio/dmic.h>
#include <zephyr/kernel.h>

#include "vm3011.h"

#include <zephyr/logging/log.h>

int vm3011_init(void);
int vm3011_start_sampling(void);

int vm3011_enable(bool status);

#endif
