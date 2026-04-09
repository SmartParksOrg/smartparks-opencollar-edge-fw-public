/** @file mic_inter.h
 *
 * @brief Interface for microphone
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2023 Irnas. All rights reserved.
 */

#ifndef MIC_INTERFACE_H
#define MIC_INTERFACE_H

#include <zephyr/kernel.h>

#ifdef CONFIG_VM1010
#include "vm1010.h"
#endif

#ifdef CONFIG_VM3011
#include "vm3011_interface.h"
#endif

#ifdef CONFIG_T5838
#include <t5838_interface.h>
#endif

int mic_init(void);

#endif
