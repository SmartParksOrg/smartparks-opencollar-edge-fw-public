/** @file vm1010.c
 *
 * @brief Driver for microphone VM1010
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2020 Irnas. All rights reserved.
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <voltage_divider.h>

#include "vm1010.h"

#define MIC_MODE_NORMAL        0
#define MIC_MODE_WAKE_ON_SOUND 1

#define VM1010_ADC_GAIN ADC_GAIN_1_6
#define BUFFER_SIZE     1

#define MIC_CHECK_INTERVAL 5000

LOG_MODULE_REGISTER(MIC_VM1010, 3);

/* We should have proper definition in DT to build this module! */
BUILD_ASSERT(DT_NODE_EXISTS(DT_NODELABEL(vm1010)), "vm1010 is not defined in DT!");
BUILD_ASSERT(DT_NODE_EXISTS(DT_NODELABEL(mic_mode)), "mic_mode is not defined in DT!");
BUILD_ASSERT(DT_NODE_EXISTS(DT_NODELABEL(mic_dout)), "mic_dout is not defined in DT!");

/* Device bindings */
const struct device *vm1010_dev = DEVICE_DT_GET(DT_NODELABEL(vm1010));

const struct gpio_dt_spec vm1010_mode_gpio = GPIO_DT_SPEC_GET(DT_NODELABEL(mic_mode), gpios);
const struct gpio_dt_spec vm1010_dout_gpio = GPIO_DT_SPEC_GET(DT_NODELABEL(mic_dout), gpios);

/* Private variables */
uint64_t last_vm1010_sample = 0; // Last time vm1010 was checked for samples
static int16_t buf[BUFFER_SIZE];

uint32_t output_ohm = 0;
uint32_t full_ohm = 0;
uint8_t vm1010_pin = 0;

int vm1010_init(void)
{
	int err = 0;
	if (vm1010_dev == NULL) {
		LOG_ERR("VM1010 device unknown");
		return -ENXIO;
	} else {
		if (!device_is_ready(vm1010_dev)) {
			LOG_ERR("VM1010 device defined, but not ready!");
			return -ENXIO;
		}
	}

	// MIC mode pin setup
	// Check device binding
	if (!device_is_ready(vm1010_mode_gpio.port)) {
		if (vm1010_mode_gpio.port) {
			LOG_ERR("Device %s is not ready", vm1010_mode_gpio.port->name);
			return -ENODEV;
		}
		LOG_ERR("GPIO device not defined in DT for VM1010 MODE pin!");
		return -EIO;
	}

	// Configure pin
	err = gpio_pin_configure_dt(&vm1010_mode_gpio, GPIO_OUTPUT_ACTIVE);
	if (err) {
		LOG_ERR("Failed to init mode pin");
		return err;
	}

	err = gpio_pin_set_dt(&vm1010_mode_gpio, MIC_MODE_WAKE_ON_SOUND);
	if (err) {
		LOG_ERR("Failed to init mode pin");
		return err;
	} else {
		LOG_INF("Init mic mode pin done. Wake on sound mode set.");
	}

	// MIC dout setup
	if (!device_is_ready(vm1010_dout_gpio.port)) {
		if (vm1010_dout_gpio.port) {
			LOG_ERR("Device %s is not ready", vm1010_dout_gpio.port->name);
			return -ENODEV;
		}
		LOG_ERR("GPIO device not defined in DT for VM1010 DOUT pin!");
		return -EIO;
	}

	// Configure pin
	err = gpio_pin_configure_dt(&vm1010_dout_gpio, GPIO_INPUT);
	if (err) {
		LOG_ERR("Failed to init dout pin");
		return err;
	} else {
		LOG_INF("Init mic dout pin done. ");
	}

	return err;
}

int vm1010_disable(void)
{

	if (!vm1010_mode_gpio.port) {
		LOG_ERR("Failed to initialize mode pin gpio");
		return -ENOENT;
	} else {
		gpio_pin_configure_dt(&vm1010_mode_gpio, GPIO_INPUT);
		LOG_INF("Disabling mic mode pin done.");
	}

	return 0;
}

int vm1010_sample(void)
{
	if (vm1010_dev == NULL) {
		LOG_ERR("VM1010 Device unknown");
		return -ENXIO;
	}

	// Enable measurements
	int err = 0;
	uint64_t start_time = 0;

	// Reset buffer
	memset(buf, 0, sizeof(buf));

	// Sample
	start_time = k_uptime_get();
	for (int i = 0; i < 100; i++) {
		buf[i] = voltage_divider_sample(vm1010_dev);
		LOG_INF("%d", buf[i]);
	}
	LOG_INF("Sampling dome, time: %d\n", (int)(k_uptime_get() - start_time));

	return err;
}

int vm1010_set_mode(uint8_t mode)
{
	int err = 0;

	if (!vm1010_mode_gpio.port) {
		LOG_ERR("Failed to initialize mode pin gpio");
		return -ENOENT;
	} else {
		err = gpio_pin_set_dt(&vm1010_mode_gpio, (int)mode);
	}

	return err;
}

int vm1010_get_detected(void)
{
	int detected = 0;
	if (vm1010_dout_gpio.port) {
		detected = gpio_pin_get_dt(&vm1010_dout_gpio);
	} else {
		detected = -ENOENT;
	}

	return detected;
}

int handle_microphone(void)
{
	if (k_uptime_get() - last_vm1010_sample > MIC_CHECK_INTERVAL) {

		last_vm1010_sample = k_uptime_get();
	}

	return 0;
}
