/** @file reed.c
 *
 * @brief File containing globally used functions
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2020 Irnas. All rights reserved.
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/reboot.h>

#include "common_functions.h"
#include "definitions.h"
#include "global_time.h"
#include "led.h"
#include "status.h"
#include "thread_com.h"
#include "thread_operation.h"

#include "reed.h"

LOG_MODULE_REGISTER(reed);

enum reed_status {
	REED_INACTIVE = 0,
	REED_ACTIVE = 1,
};

#if DT_NODE_EXISTS(DT_NODELABEL(reed))
const struct gpio_dt_spec reed_gpio = GPIO_DT_SPEC_GET(DT_NODELABEL(reed), gpios);
static struct gpio_callback reed_cb;
enum reed_status reed = REED_INACTIVE; // REED status
#endif                                 // DT_NODE_EXISTS(DT_NODELABEL(reed))

/* PRIVATE FUNCTIONS */

#if DT_NODE_EXISTS(DT_NODELABEL(reed))
static void reed_work_handler(struct k_work *work)
{
	// Do something and enable interrupt
	int stat = gpio_pin_get_dt(&reed_gpio);
	LOG_INF("Reed handler triggered, logical value of pin: %d raw value: %d", stat,
		gpio_pin_get_raw(reed_gpio.port, reed_gpio.pin));

	if (stat < 0) {
		LOG_ERR("Eroor reading reed pin, reboot!");
		k_sleep(K_SECONDS(2));
		sys_reboot(0);
	}

	// Do something
	reed = stat;
	led_blink(10, LED_G);

	if (reed == REED_ACTIVE) {
		LOG_INF("REED active detected! Go to LOW POWER hibernation mode mode!");
		gpio_pin_interrupt_configure_dt(&reed_gpio, GPIO_INT_LEVEL_INACTIVE);
		hibernation_mode();
	} else if (reed == REED_INACTIVE) {
		LOG_INF("REED inactive detected, reboot in 2s!");
		k_sleep(K_SECONDS(2));
		sys_reboot(0);
	}
}

/* Define work handler */
K_WORK_DELAYABLE_DEFINE(reed_work, reed_work_handler);

/**
 * @brief Reed callback function. Disable int and sbmit work.
 *
 * @param dev
 * @param cb
 * @param pin
 */
static void reed_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pin)
{
	gpio_pin_interrupt_configure_dt(&reed_gpio, GPIO_INT_DISABLE);
	k_work_schedule(&reed_work, K_MSEC(50));
}
#endif

/*!
 * @brief Init REED.
 *
 *
 * @return negative error code, 0 is successful.
 */
int reed_init(void)
{
#if DT_NODE_EXISTS(DT_NODELABEL(reed))
	// Bind device
	if (!device_is_ready(reed_gpio.port)) {
		if (reed_gpio.port) {
			LOG_ERR("Device %s is not ready", reed_gpio.port->name);
			return -ENODEV;
		}
		LOG_ERR("GPIO device not defined in DT for REED pin!");
		return -EIO;
	}
	LOG_INF("GPIO REED device get binding done");

	// Configure pin
	int ret = gpio_pin_configure_dt(&reed_gpio, GPIO_INPUT);
	if (ret < 0) {
		LOG_ERR("REED pin configure failed!");
		return -EIO;
	}
	LOG_INF("REED pin configure done");
	LOG_INF("Initial state, logical value of pin: %d raw value: %d",
		gpio_pin_get_dt(&reed_gpio), gpio_pin_get_raw(reed_gpio.port, reed_gpio.pin));

	// Init cb
	gpio_init_callback(&reed_cb, reed_callback, BIT(reed_gpio.pin));
	if (gpio_add_callback(reed_gpio.port, &reed_cb) < 0) {
		LOG_ERR("Could not set gpio callback for reed");
		return -EIO;
	}

	// Configure interrupt
	gpio_pin_interrupt_configure_dt(&reed_gpio, GPIO_INT_LEVEL_ACTIVE);

	// Set initial status
	reed = REED_INACTIVE;
#endif // DT_NODE_EXISTS(DT_NODELABEL(reed))
	return 0;
}

void reed_check(void)
{
#if DT_NODE_EXISTS(DT_NODELABEL(reed))
	int stat = gpio_pin_get_dt(&reed_gpio);
	if (stat != reed) {
		k_sleep(K_SECONDS(3));
		stat = gpio_pin_get_dt(&reed_gpio);
		if (stat != reed) {
			LOG_WRN("Problem! We did not handle interrupt as we should! Reed status: "
				"%d but it "
				"should be: %d!",
				reed, stat);
			reed = stat;
			if (reed == REED_ACTIVE) {
				LOG_INF("REED active detected! Go to LOW POWER hibernation mode "
					"mode!");
				gpio_pin_interrupt_configure_dt(&reed_gpio,
								GPIO_INT_LEVEL_INACTIVE);
				hibernation_mode();
			} else if (reed == REED_INACTIVE) {
				LOG_INF("REED inactive detected, reboot in 2s!");
				k_sleep(K_SECONDS(2));
				sys_reboot(0);
			}
		}
	}
#endif // DT_NODE_EXISTS(DT_NODELABEL(reed))
}
