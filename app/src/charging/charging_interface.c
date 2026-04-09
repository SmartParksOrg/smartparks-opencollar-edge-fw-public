/** @file charging_interface.c
 *
 * @brief Interface to handle battery and charging
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2023 Irnas. All rights reserved.
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <voltage_divider.h>

#include "charging_interface.h"
#include "charging_voltage_adjustment.h"
#include "definitions.h"
#include "generated_settings.h"
#include "led.h"
#include "status.h"

#define CHARGING_MEASURE_INTERVAL_MS      CHARGING_READ_INTERVAL_MS
#define CHARGING_MEASURE_INTERVAL_FAST_MS CHARGING_READ_INTERVAL_FAST_MS

LOG_MODULE_REGISTER(CHARGING_INTERFACE, 3);

enum charging_status {
	CHG_INACTIVE = 0,
	CHG_ACTIVE = 1,
	CHG_ACTIVE_FULL = 2,
};

struct charging_status_interrupt_configuration_work {
	int gpio_int_level;
	struct k_work_delayable work;
};

/* Device bindings */

#if DT_NODE_EXISTS(DT_PATH(vcharge))
const struct device *charge_dev = DEVICE_DT_GET(DT_PATH(vcharge));
#else
const struct device *charge_dev;
#endif // DT_NODE_EXISTS(DT_PATH(vcharge))

// Charging disable pin
#if DT_NODE_EXISTS(DT_NODELABEL(chg_disable))
const struct gpio_dt_spec chg_disable_gpio = GPIO_DT_SPEC_GET(DT_NODELABEL(chg_disable), gpios);
#endif

// Charging status pin
#if DT_NODE_EXISTS(DT_NODELABEL(chg_status))
const struct gpio_dt_spec chg_status_gpio = GPIO_DT_SPEC_GET(DT_NODELABEL(chg_status), gpios);
static struct gpio_callback chg_status_cb;
#endif

/* Local variables */
uint64_t last_charging_sample = 0; // Last time charging was sampled

enum charging_status chg_status = CHG_INACTIVE;
static bool __unused prv_last_chg_status = false; /* Last logical status of charging pin */

#if DT_NODE_EXISTS(DT_NODELABEL(chg_status))

/**
 * @brief Configure interrupt on charging pin work handler
 *
 * @param[in] work work structure
 */
static void chg_status_interrupt_work_handler(struct k_work *work)
{
	struct charging_status_interrupt_configuration_work *cfg_work =
		CONTAINER_OF(work, struct charging_status_interrupt_configuration_work, work);

	gpio_pin_interrupt_configure_dt(&chg_status_gpio, cfg_work->gpio_int_level);
}

/* Define status interrupt configure work handler */
static struct charging_status_interrupt_configuration_work
	prv_chg_status_interrupt_configuration_work;

/*!
 *  @brief Chg interrupt function
 */
static void chg_status_int_work_handler(struct k_work *work)
{
	// Get logical value
	LOG_DBG("CHG handler triggered, logical value of pin: %d raw value: %d",
		gpio_pin_get_dt(&chg_status_gpio),
		gpio_pin_get_raw(chg_status_gpio.port, chg_status_gpio.pin));

	int _status = gpio_pin_get_dt(&chg_status_gpio);

	if (_status < 0) {
		LOG_ERR("Failed to obtain CHG status!");
		_status = 0;
		return;
	}

	if (chg_status == CHG_INACTIVE && _status == 1) {
		/* Start charging */
		chg_status = CHG_ACTIVE;
		LOG_INF("Started charging, configure interrupt to inactive!");
		led_turn_off(LED_ALL);
		led_blink_interval(THREAD_LP_SLEEP, LED_R);

		/* Set gpio int level and commit work */
		prv_chg_status_interrupt_configuration_work.gpio_int_level =
			GPIO_INT_LEVEL_INACTIVE;
		k_work_schedule(&prv_chg_status_interrupt_configuration_work.work, K_MSEC(1000));
		return;
	}

	if (chg_status == CHG_ACTIVE && _status == 1) {
		/* Continue charging */
		/* Set gpio int level and commit work */
		prv_chg_status_interrupt_configuration_work.gpio_int_level =
			GPIO_INT_LEVEL_INACTIVE;
		k_work_schedule(&prv_chg_status_interrupt_configuration_work.work, K_MSEC(1000));
		return;
	}

	if (_status == 0) {
		/* Full battery */
		if (Main_values.batt_mV->def_val > FULL_BATTERY_LEVEL) {
			/* Stop Charging */
			LOG_INF("Stopped charging! Battery full. batt: %d",
				Main_values.batt_mV->def_val);
			chg_status = CHG_ACTIVE_FULL;
			led_turn_on(LED_G);
			/* Set gpio int level and commit work */
			prv_chg_status_interrupt_configuration_work.gpio_int_level =
				GPIO_INT_LEVEL_ACTIVE;

		} else if (Main_values.charge_mV->def_val == 0) {
			/* Stop Charging - charger unplugged */
			LOG_INF("Stopped charging!");
			chg_status = CHG_INACTIVE;
			led_turn_off(LED_ALL);

			/* Set gpio int level and commit work */
			prv_chg_status_interrupt_configuration_work.gpio_int_level =
				GPIO_INT_LEVEL_ACTIVE;
		}

		k_work_schedule(&prv_chg_status_interrupt_configuration_work.work, K_MSEC(1000));
		return;
	}
}

/* Define work handler */
K_WORK_DELAYABLE_DEFINE(chg_status_int_work, chg_status_int_work_handler);

/*!
 *  @brief CHG interrupt function definition
 */
static void chg_status_interrupt_cb_fun(const struct device *dev, struct gpio_callback *cb,
					uint32_t pin)
{
	// Disable cb
	gpio_pin_interrupt_configure_dt(&chg_status_gpio, GPIO_INT_DISABLE);
	// Submit work
	k_work_schedule(&chg_status_int_work, K_MSEC(50));
}

/**
 * @brief Configure interrupt on charging pin if applicable
 *
 * @return int
 */
static int charging_interface_status_interrupt_setup(void)
{
	int err = 0;

	// Check device binding
	if (!device_is_ready(chg_status_gpio.port)) {
		if (chg_status_gpio.port) {
			LOG_ERR("Device %s is not ready", chg_status_gpio.port->name);
			return -ENODEV;
		}
		LOG_ERR("GPIO device not defined in DT for CHG STATUS pin!");
		return -EIO;
	}

	// Configure pin
	err = gpio_pin_configure_dt(&chg_status_gpio, GPIO_INPUT);
	if (err) {
		LOG_ERR("CHG status pin configure failed!");
		return err;
	}

	LOG_DBG("CHG STATUS pin configure done");
	LOG_DBG("Initial state, logical value of pin: %d raw value: %d",
		gpio_pin_get_dt(&chg_status_gpio),
		gpio_pin_get_raw(chg_status_gpio.port, chg_status_gpio.pin));

	// Init CB
	gpio_init_callback(&chg_status_cb, chg_status_interrupt_cb_fun, BIT(chg_status_gpio.pin));
	err = gpio_add_callback(chg_status_gpio.port, &chg_status_cb);
	if (err) {
		LOG_ERR("Could not set gpio callback for CHG");
		return err;
	}

	chg_status = CHG_INACTIVE;

	err = gpio_pin_interrupt_configure_dt(&chg_status_gpio, GPIO_INT_LEVEL_ACTIVE);

	k_work_init_delayable(&prv_chg_status_interrupt_configuration_work.work,
			      chg_status_interrupt_work_handler);

	return err;
}
#endif // DT_NODE_EXISTS(DT_NODELABEL(chg_status))

#if DT_NODE_EXISTS(DT_NODELABEL(chg_disable))
/**
 * @brief Init charging disable pin if supported.
 * Initialize it to output inactive.
 *
 * @return int
 */
static int charging_interface_init_disable_pin(void)
{
	if (!device_is_ready(chg_disable_gpio.port)) {
		LOG_ERR("Device %s is not ready", chg_disable_gpio.port->name);
		return -EIO;
	}
	LOG_INF("GPIO device get binding done for CHG disable pin");

	int ret = gpio_pin_configure_dt(&chg_disable_gpio, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		LOG_ERR("CHG disable pin configure failed!\n");
		return -EIO;
	}
	LOG_INF("CHG disable pin configure done\n");

	return 0;
}
#endif // DT_NODE_EXISTS(DT_NODELABEL(chg_disable))

/* PUBLIC FUNCTIONS */

int charging_interface_init(void)
{
	int err = 0;

	/* Charging */
	if (charge_dev == NULL) {
		LOG_WRN("Charging device not supported!");
	} else {
		if (!device_is_ready(charge_dev)) {
			LOG_ERR("Charging device defined, but not ready!");
			return -ENXIO;
		}
	}

#if DT_NODE_EXISTS(DT_NODELABEL(chg_disable))
	err = charging_interface_init_disable_pin();
#endif // DT_NODE_EXISTS(DT_NODELABEL(chg_disable))

#if DT_NODE_EXISTS(DT_NODELABEL(chg_status))
	err = charging_interface_status_interrupt_setup();
#endif // DT_NODE_EXISTS(DT_NODELABEL(chg_status))

	return err;
}

int charging_enable(void)
{
	int ret = 0;
#if DT_NODE_EXISTS(DT_NODELABEL(chg_disable))
	ret = gpio_pin_set_dt(&chg_disable_gpio, 0);
	if (ret < 0) {
		LOG_ERR("CHG disable pin configure failed!\n");
		return -EIO;
	}
	// Enable interrupt
#if DT_NODE_EXISTS(DT_NODELABEL(chg_status))
	chg_status = CHG_INACTIVE;
	ret = gpio_pin_interrupt_configure_dt(&chg_status_gpio, GPIO_INT_LEVEL_ACTIVE);
#endif // DT_NODE_EXISTS(DT_NODELABEL(chg_status))
	LOG_DBG("Enabled charging!");
#endif // DT_NODE_EXISTS(DT_NODELABEL(chg_disable))
	return ret;
}

int charging_disable(void)
{
	int ret = 0;
#if DT_NODE_EXISTS(DT_NODELABEL(chg_disable))
	ret = gpio_pin_set_dt(&chg_disable_gpio, 1);
	if (ret < 0) {
		LOG_ERR("CHG disable pin configure failed!\n");
		return -EIO;
	}
#if DT_NODE_EXISTS(DT_NODELABEL(chg_status))
	chg_status = CHG_INACTIVE;
	ret = gpio_pin_interrupt_configure_dt(&chg_status_gpio, GPIO_INT_DISABLE);
	led_turn_off(LED_ALL);
#endif // DT_NODE_EXISTS(DT_NODELABEL(chg_status))
	LOG_DBG("Disabled charging!");
#endif // DT_NODE_EXISTS(DT_NODELABEL(chg_disable))
	return ret;
}

int charging_interface_measure(int *val)
{
	if (charge_dev == NULL) {
		LOG_ERR("Charging Device unknown");
		return -ENXIO;
	}

	int err = 0;
	int chg_mV = voltage_divider_sample(charge_dev);
	err = charging_voltage_adjust(&chg_mV);

	// Check if in boundaries and store to values structure
	if (chg_mV < 0) {
		LOG_ERR("Failed to read charging voltage, error: %d.", chg_mV);
		err = chg_mV;
	} else {
		if (chg_mV < Main_values.charge_mV->min || chg_mV > Main_values.charge_mV->max) {
			LOG_INF("Charging measurement out of boundaries, measured: %d mV, set it "
				"to 0.",
				chg_mV);
			chg_mV = 0;
		}
		LOG_DBG("Charging: %d mV", chg_mV);
		*val = chg_mV;
	}

	return err;
}

int charging_interface_handler(void)
{
	/* Charging */
	if (charge_dev == NULL) {
#if DT_NODE_EXISTS(DT_NODELABEL(chg_status))
		uint64_t read_interval = 1000; /* Once per second */
		if (read_interval > 0 && k_uptime_get() - last_charging_sample > read_interval) {
			last_charging_sample = k_uptime_get();
			int _status = gpio_pin_get_dt(&chg_status_gpio);
			if (_status < 0) {
				LOG_ERR("Failed to obtain charging status!");
				_status = 0;
				Main_values.charge_mV->def_val = 0;
				chg_status = CHG_INACTIVE;
				led_turn_off(LED_ALL);
				return -EIO;
			} else if (_status == 0) {
				if (prv_last_chg_status == false) {
					Main_values.charge_mV->def_val = 0;
					chg_status = CHG_INACTIVE;
					led_turn_off(LED_ALL);
					status_update(); /* Update status message */
				}
				/* Skip one sample for debouncing at low charging voltages
				 */
			} else if (_status == 1) {
				if (prv_last_chg_status == true) {
					/* We don't know the actual charging voltage, so we report
					 * the first value that is detectable as charging */
					Main_values.charge_mV->def_val = CHARGING_THRESHOLD_V + 100;
					chg_status = CHG_ACTIVE;
					led_blink_interval(THREAD_LP_SLEEP, LED_R);
					status_update(); /* Update status message */
				}
				/* Skip one sample for debouncing at low charging voltages
				 */
			}
			prv_last_chg_status = _status;
		}
#else
		LOG_DBG("Charging device not supported!");
#endif /* DT_NODE_EXISTS(DT_NODELABEL(chg_status)) */
	} else {
		/* Determine measurement interval base on if we have charging status pin or not */
		uint64_t read_interval = 0;
		int charge_mV = 0;

		/* If status pin is not defined, set sampling to default interval */
#if !DT_NODE_EXISTS(DT_NODELABEL(chg_status))
		read_interval = CHARGING_MEASURE_INTERVAL_MS;
#endif // DT_NODE_EXISTS(DT_NODELABEL(chg_status))

		/* Check if we are charging and set faster read interval */
		if (chg_status) {
			read_interval = CHARGING_MEASURE_INTERVAL_FAST_MS;
		} else {
			Main_values.charge_mV->def_val = 0;
		}

		/* If time, read charging value */
		if (read_interval > 0 && k_uptime_get() - last_charging_sample > read_interval) {

			/* Read charging value */
			charging_interface_measure(&charge_mV);
			last_charging_sample = k_uptime_get();
		} else {
#if DT_NODE_EXISTS(DT_NODELABEL(chg_status))
			/* Check if chg_status is CHG_INACTIVE but charger charging */
			int _status = gpio_pin_get_dt(&chg_status_gpio);
			if (chg_status == CHG_INACTIVE && _status == 1) {
				prv_chg_status_interrupt_configuration_work.gpio_int_level =
					GPIO_INT_LEVEL_INACTIVE;
				k_work_schedule(&prv_chg_status_interrupt_configuration_work.work,
						K_MSEC(1000));
			}
#endif /* DT_NODE_EXISTS(DT_NODELABEL(chg_status)) */
			return 0;
		}

		/* Check if we are charging */
		if (charge_mV < Main_values.charge_mV->max && charge_mV > CHARGING_THRESHOLD_V) {
			Main_values.charge_mV->def_val = charge_mV;
			// Check if battery full
			if (Main_values.batt_mV->def_val >= FULL_BATTERY_LEVEL) {
#if !DT_NODE_EXISTS(DT_NODELABEL(chg_status))
				chg_status = CHG_ACTIVE_FULL;
#endif //! DT_NODE_EXISTS(DT_NODELABEL(chg_status))
				led_turn_on(LED_G);
			} else {
#if !DT_NODE_EXISTS(DT_NODELABEL(chg_status))
				chg_status = CHG_ACTIVE;
#endif //! DT_NODE_EXISTS(DT_NODELABEL(chg_status))
				if (led_get_state(LED_G)) {
					led_turn_off(LED_G);
				}
				led_blink_interval(THREAD_LP_SLEEP, LED_R);
			}
		} else {
			Main_values.charge_mV->def_val = 0;
			chg_status = CHG_INACTIVE;
			led_turn_off(LED_ALL);
		}
#if DT_NODE_EXISTS(DT_NODELABEL(chg_status))
		/* Voltage dropped - start charging again */
		if (chg_status == CHG_ACTIVE_FULL &&
		    Main_values.batt_mV->def_val < FULL_BATTERY_LEVEL) {
			chg_status = CHG_ACTIVE;
			/* Set gpio int level and commit work */
			prv_chg_status_interrupt_configuration_work.gpio_int_level =
				GPIO_INT_LEVEL_INACTIVE;
			k_work_schedule(&prv_chg_status_interrupt_configuration_work.work,
					K_MSEC(1000));
		}
		/* Reconnecting charger when battery is already full */
		if (chg_status == CHG_ACTIVE_FULL && charge_mV == 0) {
			prv_chg_status_interrupt_configuration_work.gpio_int_level =
				GPIO_INT_LEVEL_ACTIVE;
			k_work_schedule(&prv_chg_status_interrupt_configuration_work.work,
					K_MSEC(1000));
		}
#endif //! DT_NODE_EXISTS(DT_NODELABEL(chg_status))
		LOG_INF("Charging status: %d, measured: %d mV", chg_status,
			Main_values.charge_mV->def_val);
	}

	return 0;
}
