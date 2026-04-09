/** @file external_switch.c
 *
 * @brief This module handles support for generic external switch sensors. This module connects to
 * the fence port available on specific devices. More information is available in the README.md
 * file.
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2025 Irnas. All rights reserved.
 */

#include <zephyr/device.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/logging/log.h>

#include <fence_port_common.h>
#include <settings_def.h>

#include <external_switch.h>

LOG_MODULE_REGISTER(external_switch);

/* Significant duration is classified as durations larger than the maximum debounce duration */
#define MINIMUM_SIGNIFICANT_DETECTION_DURATION_MS 2000

static bool prv_external_switch_initialized = false;

/* GPIO pin controlling external switch power */
static const struct gpio_dt_spec fence_port_gpio_spec =
	GPIO_DT_SPEC_GET(DT_NODELABEL(fence_port_gpio), gpios);

/* GPIO pin input for external switch detection */
static const struct gpio_dt_spec fence_port_ain_gpio_spec =
	GPIO_DT_SPEC_GET(DT_NODELABEL(fence_port_analog), gpios);

static struct gpio_callback fence_port_ain_gpio_cb;

static uint32_t prv_external_switch_last_trigger_timestamp = 0;
static uint32_t prv_external_switch_last_state_changed_timestamp = 0;
static uint32_t prv_external_switch_last_significant_timestamp_to_send = 0;
static bool prv_external_switch_send_last_significant_timestamp = false;

static enum external_switch_state prv_external_switch_state = EXTERNAL_SWITCH_INACTIVE;
static enum external_switch_state prv_external_switch_last_report = EXTERNAL_SWITCH_INACTIVE;

static uint32_t prv_external_switch_number_of_impulses = 0;

static int prv_external_switch_selected_pull = 0;
static int prv_external_switch_gpio_power = 0;

/**
 * @brief Get the external switch state based on the GPIO input.
 *
 * @param input The GPIO input value (true for HIGH, false for LOW).
 * @return The external switch state (ACTIVE or INACTIVE).
 */
static enum external_switch_state prv_external_switch_get_state_from_input(bool input)
{
	if (Main_settings.external_switch_detection_trigger_type->def_val == 0) {
		/* Trigger type 0 - Switch is active when GPIO is LOW */
		if (input == 0) {
			return EXTERNAL_SWITCH_ACTIVE;
		} else {
			return EXTERNAL_SWITCH_INACTIVE;
		}

	} else if (Main_settings.external_switch_detection_trigger_type->def_val == 1) {
		/* Trigger type 1 - Switch is active when GPIO is HIGH */
		if (input == 1) {
			return EXTERNAL_SWITCH_ACTIVE;
		} else {
			return EXTERNAL_SWITCH_INACTIVE;
		}
	} else {
		LOG_ERR("Unsupported external switch detection trigger type");
		return EXTERNAL_SWITCH_INACTIVE;
	}
}

/**
 * @brief Timer expiry function for external switch state correction.
 *
 * This function is called when the correctional timer expires. It's used for correcting the
 * state of the external switch, which can have an incorrect state if the input changed
 * during the debounce window.
 *
 * @param timer Pointer to the timer structure.
 */
static void prv_external_switch_correctional_timer_expiry_fn(struct k_timer *timer)
{
	/* Check if external switch state is the same as the locally saved state */
	int current_state = gpio_pin_get_dt(&fence_port_ain_gpio_spec);
	if (current_state < 0) {
		LOG_ERR("Failed to read external switch GPIO pin: %d", current_state);
		return;
	}

	/* Get activity from GPIO input */
	enum external_switch_state current_activity =
		prv_external_switch_get_state_from_input(current_state);

	if (current_activity != prv_external_switch_state) {
		LOG_INF("External switch state corrected: %s",
			current_activity == EXTERNAL_SWITCH_ACTIVE ? "ACTIVE" : "INACTIVE");
		prv_external_switch_state = current_activity;
	}
}

K_TIMER_DEFINE(external_switch_correctional_timer, prv_external_switch_correctional_timer_expiry_fn,
	       NULL);

/**
 * @brief Work handler for external switch state changes.
 *
 * @param work Pointer to the work structure.
 */
static void external_switch_work_handler(struct k_work *work)
{
	/* Read the GPIO pin state */
	int state = gpio_pin_get_dt(&fence_port_ain_gpio_spec);
	if (state < 0) {
		LOG_ERR("Failed to read external switch GPIO pin: %d", state);
		return;
	}

	/* Determine if switch is active or inactive and update timestamp */
	prv_external_switch_state = prv_external_switch_get_state_from_input(state);

	/* If the state has changed from Active to Inactive, set the total duration as significant,
	 * so the report sends this duration. This solves the issue where the state changed before
	 * the report was sent, sending the wrong activity duration */
	if (prv_external_switch_last_report == EXTERNAL_SWITCH_ACTIVE &&
	    prv_external_switch_state == EXTERNAL_SWITCH_INACTIVE &&
	    k_uptime_get() - prv_external_switch_last_state_changed_timestamp >=
		    MINIMUM_SIGNIFICANT_DETECTION_DURATION_MS) {
		prv_external_switch_last_significant_timestamp_to_send =
			prv_external_switch_last_state_changed_timestamp;
		prv_external_switch_send_last_significant_timestamp = true;
	}

	/* Only update when state is active, so we retain the total duration when switch is released
	 */
	if (prv_external_switch_state != EXTERNAL_SWITCH_INACTIVE ||
	    Main_settings.external_switch_send_inactivity_report->def_val) {
		prv_external_switch_last_state_changed_timestamp = k_uptime_get();
	}

	/* Check if we're counting impulses or detecting activity */
	if (Main_settings.external_switch_counter_enabled->def_val) {
		if (prv_external_switch_state == EXTERNAL_SWITCH_ACTIVE) {
			prv_external_switch_number_of_impulses++;
			LOG_INF("External switch impulses count: %d",
				prv_external_switch_number_of_impulses);
		}
	} else {
		/* Start correctional timer */
		k_timer_start(&external_switch_correctional_timer,
			      K_MSEC(Main_settings.external_switch_detection_trigger_debounce_ms
					     ->def_val),
			      K_NO_WAIT);

		LOG_INF("External switch state updated: %s",
			prv_external_switch_state == EXTERNAL_SWITCH_ACTIVE ? "ACTIVE"
									    : "INACTIVE");
	}
}

/* Register stop scan work */
K_WORK_DEFINE(work_external_switch, external_switch_work_handler);

/**
 * @brief Callback function for external switch GPIO events.
 *
 * This function is called when the external switch GPIO pin state changes. It checks the debounce
 * time and submits a work item to handle the state change.
 *
 * @param dev Pointer to the device structure.
 * @param cb Pointer to the GPIO callback structure.
 * @param pins Bitmask of the GPIO pins that triggered the callback.
 */
static void prv_external_switch_cb(const struct device *dev, struct gpio_callback *cb,
				   uint32_t pins)
{
	if (k_uptime_get() - prv_external_switch_last_trigger_timestamp <
	    Main_settings.external_switch_detection_trigger_debounce_ms->def_val) {
		return;
	}

	prv_external_switch_last_trigger_timestamp = k_uptime_get();
	/* Add work to queue */
	k_work_submit(&work_external_switch);
}

int external_switch_init(void)
{
	int err = 0;

	if (prv_external_switch_initialized) {
		return -EALREADY;
	}

	/* Configure AIN pin for external switch */
	switch (Main_settings.external_switch_input_pull->def_val) {
	case 0:
		err = gpio_pin_configure_dt(&fence_port_ain_gpio_spec, GPIO_INPUT);
		break;
	case 1:
		err = gpio_pin_configure_dt(&fence_port_ain_gpio_spec, GPIO_INPUT | GPIO_PULL_UP);
		break;
	case 2:
		err = gpio_pin_configure_dt(&fence_port_ain_gpio_spec, GPIO_INPUT | GPIO_PULL_DOWN);
		break;
	default:
		LOG_ERR("Unsupported external switch input pull setting, setting no pull");
		err = gpio_pin_configure_dt(&fence_port_ain_gpio_spec, GPIO_INPUT);
	}

	if (err) {
		LOG_ERR("Failed to configure external switch input pin: %d", err);
		return err;
	}

	prv_external_switch_selected_pull = Main_settings.external_switch_input_pull->def_val;

	/* Configure GPIO pin */
	if (Main_settings.external_switch_detection_gpio_pin_power_enabled->def_val) {
		err = gpio_pin_configure_dt(&fence_port_gpio_spec, GPIO_OUTPUT_ACTIVE);
	} else {
		err = gpio_pin_configure_dt(&fence_port_gpio_spec, GPIO_OUTPUT_INACTIVE);
	}
	if (err) {
		LOG_ERR("Failed to configure external switch GPIO pin: %d", err);
		return err;
	}

	prv_external_switch_gpio_power =
		Main_settings.external_switch_detection_gpio_pin_power_enabled->def_val;

	/* Add interrupt routine */
	gpio_init_callback(&fence_port_ain_gpio_cb, prv_external_switch_cb,
			   BIT(fence_port_ain_gpio_spec.pin));
	err = gpio_add_callback(fence_port_ain_gpio_spec.port, &fence_port_ain_gpio_cb);
	if (err) {
		LOG_ERR("Failed to add GPIO callback: %d", err);
		return err;
	}
	err = gpio_pin_interrupt_configure_dt(&fence_port_ain_gpio_spec, GPIO_INT_EDGE_BOTH);
	if (err) {
		LOG_ERR("Failed to configure external switch GPIO interrupt: %d", err);
		return err;
	}

	prv_external_switch_initialized = true;

	/* On init add first detection to work queue */
	k_work_submit(&work_external_switch);

	return 0;
}

int external_switch_deinit(void)
{
	if (prv_external_switch_initialized == false) {
		return -EALREADY;
	}

	/* Disable GPIO functionality */
	int ret = gpio_pin_configure_dt(&fence_port_ain_gpio_spec, GPIO_DISCONNECTED);
	if (ret < 0) {
		LOG_ERR("Failed to disconnect GPIO: %d", ret);
		return ret;
	}

	/* Check if callback is registered and disable it */
	if (fence_port_ain_gpio_cb.handler) {
		ret = gpio_remove_callback(fence_port_ain_gpio_spec.port, &fence_port_ain_gpio_cb);
		if (ret < 0) {
			LOG_ERR("Failed to remove GPIO callback: %d", ret);
			return ret;
		}
	}

	/* Configure enable pin */
	ret = gpio_pin_configure_dt(&fence_port_gpio_spec, GPIO_OUTPUT_INACTIVE);
	if (ret) {
		LOG_ERR("Failed to configure fence enable pin");
		return ret;
	}

	/* Disable interrupt on external switch GPIO pin */
	gpio_pin_interrupt_configure_dt(&fence_port_ain_gpio_spec, GPIO_INT_DISABLE);

	prv_external_switch_initialized = false;
	return ret;
}

int external_switch_check_pin_configuration(void)
{
	int err = 0;
	/* Check if pull and GPIO power configuration changed, if so, reinitialize */
	if (prv_external_switch_selected_pull !=
		    Main_settings.external_switch_input_pull->def_val ||
	    prv_external_switch_gpio_power !=
		    Main_settings.external_switch_detection_gpio_pin_power_enabled->def_val) {
		LOG_INF("External switch pin configuration changed, reinitializing...");
		err = external_switch_deinit();
		if (err && err != -EALREADY) {
			LOG_ERR("Failed to deinitialize external switch: %d", err);
			return err;
		}
		k_sleep(K_MSEC(1));
		err = external_switch_init();
		if (err && err != -EALREADY) {
			LOG_ERR("Failed to initialize external switch: %d", err);
			return err;
		}
		k_sleep(K_MSEC(1));
	}
	return err;
}

int external_switch_active(enum external_switch_state *active, uint32_t *duration_ms)
{
	int err;
	if (prv_external_switch_initialized == false) {
		err = fence_port_common_check_settings();
		return err;
	}

	if (!active || !duration_ms) {
		return -EINVAL;
	}

	/* Check if external switch state has changed - read GPIO */
	*active = prv_external_switch_state;

	if (prv_external_switch_send_last_significant_timestamp) {
		*duration_ms =
			k_uptime_get() - prv_external_switch_last_significant_timestamp_to_send;
		prv_external_switch_send_last_significant_timestamp = false;
	} else {
		*duration_ms = k_uptime_get() - prv_external_switch_last_state_changed_timestamp;
	}

	return 0;
}

uint32_t external_switch_get_impulse_count(void)
{
	uint32_t count = prv_external_switch_number_of_impulses;
	prv_external_switch_number_of_impulses = 0;
	return count;
}

int external_switch_send_report_check(bool *send_report, bool *force_send)
{
	if (!send_report || !force_send) {
		return -EINVAL;
	}

	if (Main_settings.external_switch_counter_enabled->def_val) {
		*send_report = true;
		*force_send = false;
		return 0;
	}

	if (prv_external_switch_last_report != prv_external_switch_state) {
		/* If the last reported state is different from the current state, we need to send a
		 * report ASAP */
		*send_report = true;
		*force_send = true;
		return 0;
	}

	/* Check if we send empty messages when the switch is inactive */
	if (Main_settings.external_switch_send_inactivity_report->def_val &&
	    prv_external_switch_state == EXTERNAL_SWITCH_INACTIVE) {
		*send_report = true;
		*force_send = false;
		return 0;
	}

	/* Check if the switch is pressed */
	if (prv_external_switch_state == EXTERNAL_SWITCH_ACTIVE) {
		*send_report = true;
		*force_send = false;
		return 0;
	}

	*send_report = false;
	*force_send = false;
	return 0;
}

void external_switch_last_report_set(enum external_switch_state state)
{
	prv_external_switch_last_report = state;
}
