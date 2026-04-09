/*
 * Copyright (c) 2020 Irnas d.o.o.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main);

#include "fence.h"

/* LED device */
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_NODELABEL(led_r), gpios);

void main(void)
{
	LOG_INF("Fence sample");

	/* Configure LED */
	if (!device_is_ready(led.port)) {
		return;
	}

	gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);

	fence_init();

	k_sleep(K_SECONDS(1));

	gpio_pin_set_dt(&led, 0);

	while (1) {

		fence_measure(10, 100000, NULL, NULL);
		k_msleep(1000);
	}
}
