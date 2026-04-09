/*
 * Copyright (c) 2020 Irnas d.o.o.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

/* LED device */
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_NODELABEL(led_r), gpios);

void main(void)
{
	printk("Blinky example\n");
	bool led_is_on = true;
	int ret;

	k_sleep(K_MSEC(1000));

	/* Configure LED */
	if (!device_is_ready(led.port)) {
		return;
	}

	ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		return;
	}
	int count = 0;

	while (1) {
		gpio_pin_set_dt(&led, (int)led_is_on);
		led_is_on = !led_is_on;
		printk("Led status: %d\n", led_is_on);
		k_msleep(1000);
	}
}
