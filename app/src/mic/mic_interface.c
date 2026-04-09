/** @file mic_interface.c
 *
 * @brief Interface for microphone
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2023 Irnas. All rights reserved.
 */

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "mic_interface.h"

LOG_MODULE_REGISTER(mic_interface);

int mic_init(void)
{
	int err = 0;
#ifdef CONFIG_MIC_VM1010
	LOG_INF("Using VM1010 mic, start init.");
	err = vm1010_init();
	// vm1010_measure_enable(true);
	vm1010_disable();
#endif
#ifdef CONFIG_VM3011
	LOG_INF("Using VM3011 mic, start init.");
	err = vm3011_init();
	// vm3011_enable(0);
#endif
#ifdef CONFIG_T5838
	LOG_INF("Using T5838 mic, start init.");
	err = t5838_init();
#endif

	return 0;
}
