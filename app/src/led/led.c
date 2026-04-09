/** @file led.c
 *
 * @brief File containing interface for led and led events
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2023 Irnas. All rights reserved.
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "generated_settings.h"
#include "led.h"

#define LED_TIMEOUT   100
#define BLINK_TIMEOUT 5000

LOG_MODULE_REGISTER(led);

enum led_state {
	LED_OFF = 0,
	LED_ON = 1,
	LED_BLINK = 2,
	LED_EVENT = 3,
};

struct led_dev {
	const struct gpio_dt_spec led_spec;
	uint8_t state;
	enum led_color color;
};

struct rgb_led {
	struct led_dev R;
	struct led_dev G;
	struct led_dev B;
};

struct rgb_led led = {.R =
			      {
#if DT_NODE_EXISTS(DT_NODELABEL(led_r))
				      .led_spec = GPIO_DT_SPEC_GET(DT_NODELABEL(led_r), gpios),
#else
				      .led_spec = NULL,
#endif
				      .color = LED_R,
				      .state = 0},
		      .G =
			      {
#if DT_NODE_EXISTS(DT_NODELABEL(led_g))
				      .led_spec = GPIO_DT_SPEC_GET(DT_NODELABEL(led_g), gpios),
				      .color = LED_G,
#elif DT_NODE_EXISTS(DT_NODELABEL(led_r))
				      .led_spec = GPIO_DT_SPEC_GET(DT_NODELABEL(led_r), gpios),
				      .color = LED_R,
#else
				      .led_spec = NULL,
				      .color = LED_R,
#endif
				      .state = 0},
		      .B = {
#if DT_NODE_EXISTS(DT_NODELABEL(led_b))
			      .led_spec = GPIO_DT_SPEC_GET(DT_NODELABEL(led_b), gpios),
			      .color = LED_B,
#elif DT_NODE_EXISTS(DT_NODELABEL(led_r))
			      .led_spec = GPIO_DT_SPEC_GET(DT_NODELABEL(led_r), gpios),
			      .color = LED_R,
#else
			      .led_spec = NULL,
			      .color = LED_R,
#endif
			      .state = 0}};

// LED mutex
K_MUTEX_DEFINE(led_mutex);
K_MUTEX_DEFINE(blink_mutex);

bool led_en = false;
uint64_t led_last_change = 0;       // Time of last LED state change
uint32_t my_led_blink_interval = 0; // Blink interval, if 0 led is turned off

enum led_state my_led_state = LED_OFF;
enum led_color my_led_color = LED_R;

/* PRIVATE FUNCTIONS */

/**
 * @brief Control LED.
 *
 * @param[in] mode - on (1)/ off (0)
 * @param[in] color - led color
 */
static void control_led(uint8_t mode, enum led_color color)
{
	// Check input
	if (mode > 1) {
		return;
	}

	// Check if led functionality is enabled
	if (!led_en) {
		mode = 0;
	}

	// Lock mutex
	if (!k_mutex_lock(&led_mutex, K_MSEC(LED_TIMEOUT))) {
		// RED
		if (color == LED_R || color == LED_ALL || color == LED_Y || color == LED_M) {
			if (led.R.led_spec.port) {
				gpio_pin_set_dt(&led.R.led_spec, (int)mode);
				led.R.state = mode;
			}
		}
		// GREEN
		if (color == LED_G || color == LED_ALL || color == LED_Y || color == LED_C) {
			if (led.G.led_spec.port) {
				gpio_pin_set_dt(&led.G.led_spec, (int)mode);
				led.G.state = mode;
			}
		}
		// BLUE
		if (color == LED_B || color == LED_ALL || color == LED_M || color == LED_C) {
			if (led.B.led_spec.port) {
				gpio_pin_set_dt(&led.B.led_spec, (int)mode);
				led.B.state = mode;
			}
		}
		k_mutex_unlock(&led_mutex);
	}

	led_last_change = k_uptime_get();
}

/* PUBLIC FUNCTIONS */

uint8_t led_get_state(enum led_color color)
{
	switch (color) {
	case LED_R: {
		return led.R.state;
	}
	case LED_G: {
		return led.G.state;
	}
	case LED_B: {
		return led.B.state;
	}
	case LED_ALL: {
		return (led.R.state || led.G.state || led.B.state);
	}
	default: {
		return 0;
	}
	}
}

int led_init(void)
{
	int ret = 0;

	// RED
	if (led.R.led_spec.port == NULL) {
		LOG_WRN("GPIO device get binding failed for RED LED!");
		return ret;
	} else {
		ret = gpio_pin_configure_dt(&led.R.led_spec, GPIO_OUTPUT_INACTIVE);
		if (ret < 0) {
			LOG_ERR("RED LED pin configure failed!");
			return -EIO;
		}
		LOG_INF("RED LED pin configure done");
	}

	// GREEN
	if (led.G.led_spec.port == NULL) {
		LOG_WRN("GPIO device get binding failed for GREEN LED!");
		return ret;
	}
	// Configure only if we have green led
	else if (led.G.color == LED_G) {
		ret = gpio_pin_configure_dt(&led.G.led_spec, GPIO_OUTPUT_INACTIVE);
		if (ret < 0) {
			LOG_ERR("GREEN LED pin configure failed!");
			return -EIO;
		}
		LOG_INF("GREEN LED pin configure done");
	}

	// BLUE
	if (led.B.led_spec.port == NULL) {
		LOG_WRN("GPIO device get binding failed for BLUE LED!");
		return ret;
	}
	// Configure only if we have blue led
	else if (led.B.color == LED_B) {
		ret = gpio_pin_configure_dt(&led.B.led_spec, GPIO_OUTPUT_INACTIVE);
		if (ret < 0) {
			LOG_ERR("BLUE LED pin configure failed!");
			return -EIO;
		}
		LOG_INF("BLUE LED pin configure done");
	}

	// get led status from nvs
	led_en = Main_settings.led_enabled->def_val;
	LOG_INF("LED status: %d", led_en);

	my_led_state = LED_OFF;
	control_led(1, LED_R);
	k_sleep(K_MSEC(200));
	control_led(0, LED_R);
	k_sleep(K_MSEC(200));
	control_led(1, LED_G);
	k_sleep(K_MSEC(200));
	control_led(0, LED_G);
	k_sleep(K_MSEC(200));
	control_led(1, LED_B);
	k_sleep(K_MSEC(200));
	control_led(0, LED_B);
	k_sleep(K_MSEC(200));

	return 0;
}

void led_change_status(bool stat)
{
	led_en = stat;
	if (!led_en) {
		control_led(0, LED_ALL);
	}
}

void led_turn_on(enum led_color color)
{
	// Turn other colors off
	if (led_get_state(LED_ALL)) {
		control_led(0, LED_ALL);
	}

	control_led(1, color);
	my_led_state = LED_ON;
	my_led_color = color;
}

void led_turn_off(enum led_color color)
{
	control_led(0, LED_ALL);
	my_led_state = LED_OFF;
}

void led_blink(uint8_t n, enum led_color color)
{
	// Check if another blink fn is in progress
	if (!k_mutex_lock(&blink_mutex, K_MSEC(BLINK_TIMEOUT))) {
		// Record start state
		uint8_t start_state_R = led_get_state(LED_R);
		uint8_t start_state_G = led_get_state(LED_G);
		uint8_t start_state_B = led_get_state(LED_B);

		enum led_state start_state = my_led_state;
		my_led_state = LED_EVENT;
		// If any LED color is turned ON, turn it off
		control_led(0, LED_ALL);
		k_sleep(K_MSEC(200));

		for (uint8_t i = 0; i < n; i++) {
			control_led(1, color);
			k_sleep(K_MSEC(200));
			control_led(0, color);
			k_sleep(K_MSEC(200));
		}

		// Return to initial state
		control_led(start_state_R, LED_R);
		control_led(start_state_G, LED_G);
		control_led(start_state_B, LED_B);

		my_led_state = start_state;

		k_mutex_unlock(&blink_mutex);
	}
}

void led_blink_interval(uint32_t change_interval, enum led_color color)
{
	my_led_blink_interval = change_interval;
	if (my_led_blink_interval == 0) {
		my_led_state = LED_OFF;
	} else {
		my_led_state = LED_BLINK;
		my_led_color = color;
	}
}

void led_handler(void)
{
	switch (my_led_state) {
	case LED_OFF: {
		if (led_get_state(LED_ALL)) {
			control_led(0, LED_ALL);
		}
		break;
	}
	case LED_ON: {
		if (!led_get_state(my_led_color)) {
			control_led(1, my_led_color);
		}
		break;
	}
	case LED_BLINK: {
		// If interval is not defined, do nothing
		if (my_led_blink_interval == 0) {
			if (led_get_state(LED_ALL)) {
				my_led_state = LED_OFF;
				control_led(0, LED_ALL);
			}
			return;
		}

		if ((uint32_t)((k_uptime_get() - led_last_change)) >= my_led_blink_interval) {
			uint8_t new_state = (led_get_state(my_led_color) + 1) % 2;
			control_led(new_state, my_led_color);
		}
		break;
	}
	case LED_EVENT: {
		// Pass
		break;
	}
	}
}
