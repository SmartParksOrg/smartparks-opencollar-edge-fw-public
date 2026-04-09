/**
 * @file rf-front-end-module.c
 * @brief
 *
 * @copyright (c) 2023 Irnas. All rights reserved.
 *
 */

#include <rf_front_end_module.h>

#include <stdio.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(rf_front_end_module, CONFIG_RF_FRONT_END_MODULE_LOG_LEVEL);

/* Pins must be defined in devicetree for module to work */
const struct gpio_dt_spec cps_gpio = GPIO_DT_SPEC_GET(DT_NODELABEL(front_end_cps), gpios);
const struct gpio_dt_spec csd_gpio = GPIO_DT_SPEC_GET(DT_NODELABEL(front_end_csd), gpios);
const struct gpio_dt_spec ctx_gpio = GPIO_DT_SPEC_GET(DT_NODELABEL(front_end_ctx), gpios);
const struct gpio_dt_spec ant_sel_gpio = GPIO_DT_SPEC_GET(DT_NODELABEL(front_end_ant_sel), gpios);

static enum rf_front_end_mode prv_mode = RF_FRONT_END_MODE_SLEEP;

/**
 * @brief Configure front-end module gpios as outputs and set them to inactive
 *
 * @return int 0 if successful, negative errno code if failure.
 */
static int prv_configure_gpios(void)
{
	int err = 0;

	err = gpio_pin_configure_dt(&cps_gpio, GPIO_OUTPUT_INACTIVE);
	if (err) {
		LOG_ERR("Failed to configure cps gpio");
		return err;
	}

	err = gpio_pin_configure_dt(&csd_gpio, GPIO_OUTPUT_INACTIVE);
	if (err) {
		LOG_ERR("Failed to configure csd gpio");
		return err;
	}

	err = gpio_pin_configure_dt(&ctx_gpio, GPIO_OUTPUT_INACTIVE);
	if (err) {
		LOG_ERR("Failed to configure ctx gpio");
		return err;
	}

	err = gpio_pin_configure_dt(&ant_sel_gpio, GPIO_OUTPUT_INACTIVE);
	if (err) {
		LOG_ERR("Failed to configure ant sel gpio");
		return err;
	}

	return err;
}

/**
 * @brief Set module to sleep mode. This is done by setting all gpios to inactive
 *
 * @return int 0 if successful, negative errno code if failure.
 */
static int prv_set_sleep_mode(void)
{
	int err = 0;

	err = gpio_pin_set_dt(&cps_gpio, 0);
	if (err) {
		LOG_ERR("Failed to configure cps gpio");
		return err;
	}

	err = gpio_pin_set_dt(&csd_gpio, 0);
	if (err) {
		LOG_ERR("Failed to configure csd gpio");
		return err;
	}

	err = gpio_pin_set_dt(&ctx_gpio, 0);
	if (err) {
		LOG_ERR("Failed to configure ctx gpio");
		return err;
	}

#ifdef CONFIG_BOARD_COLLAREDGE_NRF52840
	if (strcmp(CONFIG_BOARD_REVISION, "1.1.0") == 0 ||
	    strcmp(CONFIG_BOARD_REVISION, "1.4.0") == 0 ||
	    strcmp(CONFIG_BOARD_REVISION, "1.5.0") == 0) {
		err = gpio_pin_set_dt(&ant_sel_gpio, 0);
		if (err) {
			LOG_ERR("Failed to configure ctx gpio");
			return err;
		}
	}
#elif CONFIG_BOARD_FREEEDGE_NRF52840
	/* Select ANT2 path (2.4GHz) */
	err = gpio_pin_set_dt(&ant_sel_gpio, 0);
	if (err) {
		LOG_ERR("Failed to configure ctx gpio");
		return err;
	}
#elif CONFIG_BOARD_RANGEREDGE_NRF52840 || CONFIG_BOARD_RANGEREDGE_AIRQ_NRF52840
	if (strcmp(CONFIG_BOARD_REVISION, "1.8.0") == 0) {
		/* Select ANT2 path (2.4GHz) */
		err = gpio_pin_set_dt(&ant_sel_gpio, 0);
		if (err) {
			LOG_ERR("Failed to configure ctx gpio");
			return err;
		}
	}
#endif

	/* Wait for the LNA to stabilize (min 800 ns as per the datasheet (Rise and fall times)) */
	k_msleep(1);

	return err;
}

/**
 * @brief Set module to bypass mode. This is done by setting cps and csd gpios to inactive and ctx
 * gpio to active.
 *
 * @return int 0 if successful, negative errno code if failure.
 */
static int prv_set_bypass_mode(void)
{
	int err = 0;

	err = gpio_pin_set_dt(&cps_gpio, 0);
	if (err) {
		LOG_ERR("Failed to configure cps gpio");
		return err;
	}

	err = gpio_pin_set_dt(&csd_gpio, 1);
	if (err) {
		LOG_ERR("Failed to configure csd gpio");
		return err;
	}

	err = gpio_pin_set_dt(&ctx_gpio, 0);
	if (err) {
		LOG_ERR("Failed to configure ctx gpio");
		return err;
	}

	/* Wait for the LNA to stabilize (min 800 ns as per the datasheet (Rise and fall times)) */
	k_msleep(1);

	return err;
}

/**
 * @brief Set module to rx lna mode. This is done by setting cps and ctx gpios to active and csd
 *
 * @return int 0 if successful, negative errno code if failure.
 */
static int prv_set_rx_lna_mode(void)
{
	int err = 0;

	err = gpio_pin_set_dt(&cps_gpio, 1);
	if (err) {
		LOG_ERR("Failed to configure cps gpio");
		return err;
	}

	err = gpio_pin_set_dt(&csd_gpio, 1);
	if (err) {
		LOG_ERR("Failed to configure csd gpio");
		return err;
	}

	err = gpio_pin_set_dt(&ctx_gpio, 0);
	if (err) {
		LOG_ERR("Failed to configure ctx gpio");
		return err;
	}

#ifdef CONFIG_BOARD_COLLAREDGE_NRF52840
	if (strcmp(CONFIG_BOARD_REVISION, "1.4.0") == 0 ||
	    strcmp(CONFIG_BOARD_REVISION, "1.5.0") == 0) {
		/* Select ANT1 path (2.4GHz) */
		err = gpio_pin_set_dt(&ant_sel_gpio, 0);
		if (err) {
			LOG_ERR("Failed to configure ctx gpio");
			return err;
		}
	}
#elif CONFIG_BOARD_RANGEREDGE_NRF52840 || CONFIG_BOARD_RANGEREDGE_AIRQ_NRF52840
	if (strcmp(CONFIG_BOARD_REVISION, "1.8.0") == 0) {
		/* Select ANT2 path (2.4GHz) */
		err = gpio_pin_set_dt(&ant_sel_gpio, 1);
		if (err) {
			LOG_ERR("Failed to configure ctx gpio");
			return err;
		}
	}
#endif

	/* Wait for the LNA to stabilize (min 800 ns as per the datasheet (Rise and fall times)) */
	k_msleep(1);

	return err;
}

/**
 * @brief Set module to tx mode. This is done by setting cps and ctx gpios to active and csd gpio
 * to inactive.
 *
 * @return int 0 if successful, negative errno code if failure.
 */
static int prv_set_tx_mode(void)
{
	int err = 0;

	err = gpio_pin_set_dt(&csd_gpio, 1);
	if (err) {
		LOG_ERR("Failed to configure csd gpio");
		return err;
	}

	err = gpio_pin_set_dt(&ctx_gpio, 1);
	if (err) {
		LOG_ERR("Failed to configure ctx gpio");
		return err;
	}

#ifdef CONFIG_BOARD_COLLAREDGE_NRF52840
	if (strcmp(CONFIG_BOARD_REVISION, "1.1.0") == 0) {
		/* Select ANT1 path (2.0GHz) */
		err = gpio_pin_set_dt(&ant_sel_gpio, 1);
		if (err) {
			LOG_ERR("Failed to configure ctx gpio");
			return err;
		}
	} else if (strcmp(CONFIG_BOARD_REVISION, "1.4.0") == 0 ||
		   strcmp(CONFIG_BOARD_REVISION, "1.5.0") == 0) {
		/* Select ANT1 path (2.4GHz) */
		err = gpio_pin_set_dt(&ant_sel_gpio, 0);
		if (err) {
			LOG_ERR("Failed to configure ctx gpio");
			return err;
		}
	}
#elif CONFIG_BOARD_FREEEDGE_NRF52840
	if (strcmp(CONFIG_BOARD_REVISION, "1.3.0") == 0 ||
	    strcmp(CONFIG_BOARD_REVISION, "1.6.0") == 0) {
		/* Select ANT2 path (2.4GHz) */
		err = gpio_pin_set_dt(&ant_sel_gpio, 1);
		if (err) {
			LOG_ERR("Failed to configure ctx gpio");
			return err;
		}
	}
#elif CONFIG_BOARD_RANGEREDGE_NRF52840 || CONFIG_BOARD_RANGEREDGE_AIRQ_NRF52840
	if (strcmp(CONFIG_BOARD_REVISION, "1.8.0") == 0) {
		/* Select ANT2 path (2.4GHz) */
		err = gpio_pin_set_dt(&ant_sel_gpio, 1);
		if (err) {
			LOG_ERR("Failed to configure ctx gpio");
			return err;
		}
	}
#endif

	/* Wait for the LNA to stabilize (min 800 ns as per the datasheet (Rise and fall times)) */
	k_msleep(1);

	return err;
}

int rf_front_end_module_init(void)
{
	int err = 0;

	err = prv_configure_gpios();
	if (err) {
		LOG_ERR("Failed to configure gpios");
		return err;
	}

	err = prv_set_sleep_mode();
	if (err) {
		LOG_ERR("Failed to set sleep mode");
		return err;
	}

	prv_mode = RF_FRONT_END_MODE_SLEEP;

	return err;
}

int rf_front_end_module_set_mode(enum rf_front_end_mode mode)
{
	int err = 0;
	switch (mode) {
	case RF_FRONT_END_MODE_SLEEP: {
		err = prv_set_sleep_mode();
		if (!err) {
			prv_mode = RF_FRONT_END_MODE_SLEEP;
		}
		break;
	}
	case RF_FRONT_END_MODE_BYPASS: {
		err = prv_set_bypass_mode();
		if (!err) {
			prv_mode = RF_FRONT_END_MODE_BYPASS;
		}
		break;
	}
	case RF_FRONT_END_MODE_RX_LNA: {
		err = prv_set_rx_lna_mode();
		if (!err) {
			prv_mode = RF_FRONT_END_MODE_RX_LNA;
		}
		break;
	}
	case RF_FRONT_END_MODE_TX: {
		err = prv_set_tx_mode();
		if (!err) {
			prv_mode = RF_FRONT_END_MODE_TX;
		}
		break;
	}
	default: {
		LOG_ERR("Invalid mode");
		return -EINVAL;
	}
	}

	return err;
}

enum rf_front_end_mode rf_front_end_module_get_mode(void)
{
	return prv_mode;
}

int rf_front_end_module_set_ant_path(enum rf_front_end_antenna_path antenna_path)
{
	int err = gpio_pin_set_dt(&ant_sel_gpio, antenna_path);
	if (err) {
		LOG_ERR("Failed to set antenna path");
		return err;
	}

	/* Wait for the module to stabilize (min 400 ns as per the datasheet (Antenna 1 to antenna 2
	 * switching time)) */
	k_msleep(1);

	return err;
}
