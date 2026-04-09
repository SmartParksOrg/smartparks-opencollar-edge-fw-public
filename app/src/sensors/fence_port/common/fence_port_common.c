/** @file fence_port_common.c
 *
 * @brief Common functions for fence port module. This module controls the configuration of fence
 * port pins.
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2025 Irnas. All rights reserved.
 */

#include <zephyr/device.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <external_switch.h>
#include <fence.h>
#include <generated_settings.h>

#include <fence_port_common.h>

LOG_MODULE_REGISTER(fence_port_common);

int fence_port_common_init(void)
{
	/* Check which functionality is enabled and initialize said functionality */
	return fence_port_common_check_settings();
}

int fence_port_common_check_settings(void)
{
	int err = 0;
	/* Check if both fence and external switch detection are enabled */
	if (Main_settings.fence_enabled->def_val &&
	    Main_settings.external_switch_detection_enabled->def_val) {
		LOG_ERR("Fence cannot be enabled at the same time as the external switch because "
			"they share the same port.");
		LOG_WRN("Disabling external switch functionality.");

		Main_settings.external_switch_detection_enabled->def_val = false;
		err = fence_init();
		if (err == -EALREADY) {
			return 0;
		}

		return err;
	}

	/* Check and set appropriate mode */
	if (Main_settings.fence_enabled->def_val) {
		err = external_switch_deinit();
		if (err && err != -EALREADY) {
			LOG_ERR("Failed to deinitialize external switch: %d", err);
			return err;
		}

		err = fence_init();
		if (err && err != -EALREADY) {
			LOG_ERR("Failed to initialize fence: %d", err);
			return err;
		}

	} else if (Main_settings.external_switch_detection_enabled->def_val) {
		err = fence_deinit();
		if (err && err != -EALREADY) {
			LOG_ERR("Failed to deinitialize fence: %d", err);
			return err;
		}

		err = external_switch_init();
		if (err && err != -EALREADY) {
			LOG_ERR("Failed to initialize external switch: %d", err);
			return err;
		}

	} else {
		LOG_DBG("Fence and external switch detection features are disabled.");

		err = external_switch_deinit();
		if (err && err != -EALREADY) {
			LOG_ERR("Failed to deinitialize external switch: %d", err);
			return err;
		}

		err = fence_deinit();
		if (err && err != -EALREADY) {
			LOG_ERR("Failed to deinitialize fence: %d", err);
			return err;
		}
	}

	return 0;
}
