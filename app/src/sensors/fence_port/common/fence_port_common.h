/** @file fence_port_common.h
 *
 * @brief Common functions for fence port module. This module controls the configuration of fence
 * port pins.
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2025 Irnas.  All rights reserved.
 */

#ifndef FENCE_PORT_COMMON_H
#define FENCE_PORT_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

enum fence_port_common_analog_pin_cfg {
	/* This is the initial state, and not meant to be used outside of initialization */
	FENCE_PORT_AIN_INIT = 0,
	/* Analog pin configured as ADC input */
	FENCE_PORT_AIN_ANALOG = 1,
	/* Analog pin configured as GPIO input */
	FENCE_PORT_AIN_GPIO = 2
};

enum fence_port_common_gpio_pin_state {
	FENCE_PORT_GPIO_OUTPUT_INACTIVE = 0,
	FENCE_PORT_GPIO_OUTPUT_ACTIVE = 1
};

/**
 * @brief Get the pin specifications for the fence ports based on enabled settings.
 *
 * @return 0 if successful, negative error code otherwise.
 */
int fence_port_common_init(void);

/**
 * @brief Check if fence port pins are correctly configured and reconfigures them if
 * necessary.
 *
 * @return 0 if successful, negative error code otherwise.
 */
int fence_port_common_check_settings(void);

#ifdef __cplusplus
}
#endif

#endif /* FENCE_PORT_COMMON_H */
