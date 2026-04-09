/** @file operation_test.c
 *
 * @brief System test functions
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2023 Irnas. All rights reserved.
 */

#include "flash_ext_partitions.h"
#include "operation_test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/types.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>

#include <zephyr/drivers/flash.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/spi.h>

#include <zephyr/console/console.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/printk.h>

#include <voltage_divider.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/hci_vs.h>

/* LR11xx and lorawan */
#include "almanac.h"
#include "gnss.h"
#include "lorawan.h"
#include "lr11xx_board.h"
#include "lr11xx_lr_fhss.h"
#include "lr11xx_radio.h"
#include "lr11xx_system.h"
#include "lr11xx_system_types.h"

#ifdef CONFIG_LR_WIFI_SCAN
#include "wifi_scan.h"
#include "wifi_scan_data.h"
#endif /* CONFIG_LR_WIFI_SCAN */

/* lr s-band */
#ifdef CONFIG_LR_S_BAND
#include "lr_s_band.h"
#endif /* CONFIG_LR_S_BAND */

/* front-end module */
#ifdef CONFIG_RF_FRONT_END_MODULE
#include <rf_front_end_module.h>
#endif /* CONFIG_RF_FRONT_END_MODULE */

#ifdef CONFIG_SHELL_BACKEND_SERIAL
#include <zephyr/shell/shell_uart.h>
#endif // CONFIG_SHELL_BACKEND_SERIAL
#ifdef CONFIG_SHELL_BACKEND_RTT
#include <zephyr/shell/shell_rtt.h>
#endif // CONFIG_SHELL_BACKEND_RTT

#include <zephyr/pm/device.h>
#include <zephyr/pm/pm.h>

#include "bt_interface.h"
#include "common_functions.h"
#include "communication.h"
#include "flash_interface.h"
#include "generated_settings.h"
#include "global_time.h"
#include "gps.h"
#include "led.h"
#include "nvs_storage.h"
#include "uart_pm.h"

#ifdef CONFIG_ENABLE_MIC
#include "mic_interface.h"
#endif

#define STR(x)  #x
#define XSTR(x) STR(x)

// Define test commands
#define OPERATION_TEST_I2C_SCAN      I2C
#define OPERATION_TEST_I2C_SCAN_TEXT "scan I2C device addresses"

#if DT_NODE_EXISTS(DT_NODELABEL(lis2dw12_accel))
#define OPERATION_TEST_ACC      ACC
#define OPERATION_TEST_ACC_TEXT "test lis2dw12 accelerometer (I2C)"
#endif // DT_NODE_EXISTS(DT_NODELABEL(lis2dw12_accel))

#define OPERATION_TEST_TEMPERATURE      TMP
#define OPERATION_TEST_TEMPERATURE_TEXT "test temperature sensor (internal)"

#ifdef CONFIG_ENABLE_MIC
#define OPERATION_TEST_MIC      MIC
#define OPERATION_TEST_MIC_TEXT "test microphone"
#endif // CONFIG_ENABLE_MIC

#define OPERATION_TEST_BAT      BAT
#define OPERATION_TEST_BAT_TEXT "get battery voltage reading"

#if DT_NODE_EXISTS(DT_PATH(vcharge))
#define OPERATION_TEST_CHG      CHG
#define OPERATION_TEST_CHG_TEXT "get charging voltage reading"
#endif // DT_NODE_EXISTS(DT_PATH(vcharge))

#if DT_NODE_EXISTS(DT_NODELABEL(chg_disable))
#define OPERATION_TEST_CHG_EN       CHG_EN
#define OPERATION_TEST_CHG_EN_TEXT  "test charging enable"
#define OPERATION_TEST_CHG_DIS      CHG_DIS
#define OPERATION_TEST_CHG_DIS_TEXT "test charging enable"
#endif // DT_NODE_EXISTS(DT_NODELABEL(chg_disable))

#if DT_NODE_EXISTS(DT_NODELABEL(chg_status))
#define OPERATION_TEST_CHG_STAT      CHG_STAT
#define OPERATION_TEST_CHG_STAT_TEXT "get charging status pin reading"
#endif // DT_NODE_EXISTS(DT_NODELABEL(chg_status))

#if DT_NODE_EXISTS(DT_ALIAS(external_flash))
#define OPERATION_TEST_FLASH            FLASH
#define OPERATION_TEST_FLASH_TEXT       "test external flash chip (SPI)"
#define OPERATION_TEST_FLASH_ERASE      FLASH_ERASE
#define OPERATION_TEST_FLASH_ERASE_TEXT "external flash full erase"
#define OPERATION_TEST_FLASH_SIZE       FLASH_SIZE
#define OPERATION_TEST_FLASH_SIZE_TEXT  "print external flash size to console"
#endif // DT_NODE_EXISTS(DT_ALIAS(external_flash))

#define OPERATION_TEST_LORA      LORA
#define OPERATION_TEST_LORA_TEXT "read devEUI, appKey and modem version"

#define OPERATION_TEST_LORA_RX      LORA_RX
#define OPERATION_TEST_LORA_RX_TEXT "Test LoRa RX functionality"

#define OPERATION_TEST_LORA_TX      LORA_TX
#define OPERATION_TEST_LORA_TX_TEXT "Test LoRa TX functionality"

#ifdef CONFIG_LR_WIFI_SCAN
#define OPERATION_TEST_WIFI_LORA      WIFI_LORA
#define OPERATION_TEST_WIFI_LORA_TEXT "test LORA WiFi scanning"
#endif // CONFIG_LR_WIFI_SCAN

#define OPERATION_TEST_GPS_LORA      GPS_LORA
#define OPERATION_TEST_GPS_LORA_TEXT "test LORA GPS"

#ifdef CONFIG_LR_S_BAND
#define OPERATION_TEST_LR_S_BAND_CW      LR_S_BAND_CW
#define OPERATION_TEST_LR_S_BAND_CW_TEXT "test LR S-Band CW"
#endif // CONFIG_LR_S_BAND

#if DT_NODE_EXISTS(DT_NODELABEL(gps_pwr))
#define OPERATION_TEST_GPS          GPS
#define OPERATION_TEST_GPS_TEXT     "test Ublox GPS"
#define OPERATION_TEST_FIX_GPS      FIX_GPS
#define OPERATION_TEST_FIX_GPS_TEXT "test Ublox GPS fix"
#endif // DT_NODE_EXISTS(DT_NODELABEL(gps_pwr))

#define OPERATION_TEST_BT_SCAN      BT_SCAN
#define OPERATION_TEST_BT_SCAN_TEXT "test BT scan"

#define OPERATION_TEST_GET_FACTORY_NAME      GET_FACTORY_NAME
#define OPERATION_TEST_GET_FACTORY_NAME_TEXT "get device factory name"

#define OPERATION_TEST_GET_MAC      GET_MAC
#define OPERATION_TEST_GET_MAC_TEXT "get device MAC"

#define OPERATION_TEST_LED      LED
#define OPERATION_TEST_LED_TEXT "test led, specify color in the argument: R, G or B"
#define OPERATION_TEST_LED_R    "R"
#define OPERATION_TEST_LED_G    "G"
#define OPERATION_TEST_LED_B    "B"

#ifdef CONFIG_LOW_POWER
#define OPERATION_TEST_LOW_POWER      LOW_POWER
#define OPERATION_TEST_LOW_POWER_TEXT "Test low power performance"
#endif

#define OPERATION_TEST_SETTING      SETTING
#define OPERATION_TEST_SETTING_TEXT "Change setting value: setting_id, setting_len, setting_val[]"

#define OPERATION_TEST_GET_SETTING      GET_SETTING
#define OPERATION_TEST_GET_SETTING_TEXT "Get setting value by id"

#define OPERATION_TEST_EXIT      EXIT
#define OPERATION_TEST_EXIT_TEXT "Exit test loop"

#define TEST_OK  "OK"
#define TEST_ERR "ERR"

char buf[255];
bool testing_mode = true; // Run test loop until set to false
bool low_power_test =
	false; // After we exit test loop, check if set to true and perform low power test.

static void cmd_shell_print_output(const struct shell *shell, const char *syntax, bool test_res,
				   char *val, char *params)
{
	if (test_res) {
		shell_print(shell, "%s:%s|VALUE:%s!%s", syntax, TEST_OK, val, params);
	} else {
		shell_print(shell, "%s:%s|VALUE:!", syntax, TEST_ERR);
	}
}

#ifdef OPERATION_TEST_I2C_SCAN
/**
 * @brief Perform i2c scan for given peripheral.
 *
 * @param i2c_dev i2c dev
 * @param found list of found addresses
 * @return int nr of found addresses
 */
static int i2c_scan(const struct device *i2c_dev, uint8_t *found)
{
	int count = 0;
	int error = 0;
	// Start scan
	for (uint16_t i = 4; i <= 255; i++) {
		struct i2c_msg msgs[1];
		uint8_t dst = 1;
		/* Send the address to read from */
		msgs[0].buf = &dst;
		msgs[0].len = 1U;
		msgs[0].flags = I2C_MSG_WRITE | I2C_MSG_STOP;
		error = i2c_transfer(i2c_dev, &msgs[0], 1, i);
		if (error == 0) {
			found[count] = i;
			count++;
		}
	}

	return count;
}

/*!
 * @brief Perform i2c scan test on all supported i2c ports.
 *
 *
 * @return negative error code, 0 is successful.
 */
static int cmd_i2c_scan(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(shell, "ACK %s", XSTR(OPERATION_TEST_I2C_SCAN));

	int count0 = 0;
	uint8_t found0[256];

#if DT_NODE_HAS_STATUS(DT_NODELABEL(i2c0), okay)
	// Init device
	const struct device *i2c0_dev = DEVICE_DT_GET(DT_NODELABEL(i2c0));
	if (i2c0_dev) {
		count0 = i2c_scan(i2c0_dev, found0);
	}
#endif

	// Add additional parameters to string
	int offset = 0;
	offset += sprintf(buf + offset, "I2C0:");
	for (int i = 0; i < count0; i++) {
		offset += sprintf(buf + offset, "0x%x ", found0[i]);
	}

	// Convert value to string
	char val[3];
	sprintf(val, "%d", count0);

	cmd_shell_print_output(shell, XSTR(OPERATION_TEST_I2C_SCAN), true, val, buf);

	return 0;
}
#endif // OPERATION_TEST_I2C_SCAN

#ifdef OPERATION_TEST_ACC
/*!
 * @brief Test lis2 sensor.
 *
 * @return
 */
static int cmd_test_lis2dw12(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(shell, "ACK %s", XSTR(OPERATION_TEST_ACC));

	const struct device *lis2dw12_sensor = DEVICE_DT_GET(DT_NODELABEL(lis2dw12_accel));
	if (device_is_ready(lis2dw12_sensor)) {
		if (!sensor_sample_fetch(lis2dw12_sensor)) {
			struct sensor_value lis2dw12_accel_val[3];
			if (!sensor_channel_get(lis2dw12_sensor, SENSOR_CHAN_ACCEL_XYZ,
						lis2dw12_accel_val)) {
				sprintf(buf, "[%d.%02d, %d.%02d, %d.%02d]",
					lis2dw12_accel_val[0].val1,
					abs(lis2dw12_accel_val[0].val2 / 10000),
					lis2dw12_accel_val[1].val1,
					abs(lis2dw12_accel_val[1].val2 / 10000),
					lis2dw12_accel_val[2].val1,
					abs(lis2dw12_accel_val[2].val2 / 10000));

				cmd_shell_print_output(shell, XSTR(OPERATION_TEST_ACC), true, buf,
						       NULL);
				return 0;
			}
		}
	}

	cmd_shell_print_output(shell, XSTR(OPERATION_TEST_ACC), false, NULL, NULL);
	return 0;
}
#endif // OPERATION_TEST_ACC

#ifdef OPERATION_TEST_TEMPERATURE
/*!
 * @brief Test temperature sensor. Init and make measurement.
 *
 *
 * @return
 */
static int cmd_test_temp_sensor(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(shell, "ACK %s", XSTR(OPERATION_TEST_TEMPERATURE));

	const struct device *temp_dev = DEVICE_DT_GET_ANY(nordic_nrf_temp);
	if (device_is_ready(temp_dev)) {
		if (!sensor_sample_fetch(temp_dev)) {
			struct sensor_value t_mes;
			if (!sensor_channel_get(temp_dev, SENSOR_CHAN_DIE_TEMP, &t_mes)) {
				char val[8];
				sprintf(val, "%d.%02d", t_mes.val1, t_mes.val2 / 10000);
				cmd_shell_print_output(shell, XSTR(OPERATION_TEST_TEMPERATURE),
						       true, val, NULL);
				return 0;
			}
		}
	}
	cmd_shell_print_output(shell, XSTR(OPERATION_TEST_TEMPERATURE), false, NULL, NULL);
	return 0;
}
#endif // OPERATION_TEST_TEMPERATURE

#ifdef OPERATION_TEST_MIC
/*!
 * @brief Test mic. Init and disable.
 *
 *
 * @return
 */
static int cmd_test_mic(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(shell, "ACK %s", XSTR(OPERATION_TEST_MIC));

	// MIC
	int err = mic_init();
	if (!err) {
		cmd_shell_print_output(shell, XSTR(OPERATION_TEST_MIC), true, NULL, NULL);
	} else {
		cmd_shell_print_output(shell, XSTR(OPERATION_TEST_MIC), false, NULL, NULL);
	}

	return 0;
}
#endif // OPERATION_TEST_MIC

#ifdef OPERATION_TEST_BAT
/*!
 * @brief Test battery voltage.
 *
 * @return
 */
static int cmd_test_battery(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(shell, "ACK %s", XSTR(OPERATION_TEST_BAT));
	const struct device *battery_dev = DEVICE_DT_GET(DT_PATH(vbatt));
	if (device_is_ready(battery_dev)) {
		int batt_mV = voltage_divider_sample(battery_dev);
		if (batt_mV < Main_values.batt_mV->max && batt_mV > Main_values.batt_mV->min) {
			sprintf(buf, "%d", batt_mV);
			cmd_shell_print_output(shell, XSTR(OPERATION_TEST_BAT), true, buf, NULL);
			return 0;
		}
	}

	cmd_shell_print_output(shell, XSTR(OPERATION_TEST_BAT), false, NULL, NULL);
	return 0;
}
#endif // OPERATION_TEST_BAT

#ifdef OPERATION_TEST_CHG
/*!
 * @brief Test charging.
 *
 *
 * @return
 */
static int cmd_test_charging(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(shell, "ACK %s", XSTR(OPERATION_TEST_CHG));
	const struct device *charge_dev = DEVICE_DT_GET(DT_PATH(vcharge));
	if (device_is_ready(charge_dev)) {
		int charge_mV = voltage_divider_sample(charge_dev);
		if (charge_mV >= 0) {
			sprintf(buf, "%d", charge_mV);
			cmd_shell_print_output(shell, XSTR(OPERATION_TEST_CHG), true, buf, NULL);
			return 0;
		}
	}

	cmd_shell_print_output(shell, XSTR(OPERATION_TEST_CHG), false, NULL, NULL);
	return 0;
}
#endif // OPERATION_TEST_CHG

#ifdef OPERATION_TEST_CHG_EN
/*!
 * @brief Test charging enable.
 *
 * @return
 */
static int cmd_test_enable_charging(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(shell, "ACK %s", XSTR(OPERATION_TEST_CHG_EN));
	const struct gpio_dt_spec chg_disable_gpio =
		GPIO_DT_SPEC_GET(DT_NODELABEL(chg_disable), gpios);

	if (device_is_ready(chg_disable_gpio.port)) {
		int err = gpio_pin_configure_dt(&chg_disable_gpio, GPIO_OUTPUT_ACTIVE);
		if (!err) {
			cmd_shell_print_output(shell, XSTR(OPERATION_TEST_CHG_EN), true, NULL,
					       NULL);
		} else {
			cmd_shell_print_output(shell, XSTR(OPERATION_TEST_CHG_EN), false, NULL,
					       NULL);
		}
	}

	return 0;
}
#endif // OPERATION_TEST_CHG_EN

#ifdef OPERATION_TEST_CHG_DIS
/*!
 * @brief Test charging disable.
 *
 * @return
 */
static int cmd_test_disable_charging(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(shell, "ACK %s", XSTR(OPERATION_TEST_CHG_DIS));
	const struct gpio_dt_spec chg_disable_gpio =
		GPIO_DT_SPEC_GET(DT_NODELABEL(chg_disable), gpios);

	if (device_is_ready(chg_disable_gpio.port)) {
		int err = gpio_pin_configure_dt(&chg_disable_gpio, GPIO_OUTPUT_INACTIVE);
		if (!err) {
			cmd_shell_print_output(shell, XSTR(OPERATION_TEST_CHG_DIS), true, NULL,
					       NULL);
		} else {
			cmd_shell_print_output(shell, XSTR(OPERATION_TEST_CHG_DIS), false, NULL,
					       NULL);
		}
	}

	return 0;
}
#endif // OPERATION_TEST_CHG_DIS

#ifdef OPERATION_TEST_CHG_STAT
/**
 * @brief Measure status pin, if present on the HW.
 *
 */
static int cmd_test_status_charging(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(shell, "ACK %s", XSTR(OPERATION_TEST_CHG_STAT));

	const struct gpio_dt_spec chg_status_gpio_tmp =
		GPIO_DT_SPEC_GET(DT_NODELABEL(chg_status), gpios);
	int stat = gpio_pin_get_dt(&chg_status_gpio_tmp);
	if (stat < 0) {
		cmd_shell_print_output(shell, XSTR(OPERATION_TEST_CHG_STAT), false, NULL, NULL);
	} else {
		sprintf(buf, "%d", stat);
		cmd_shell_print_output(shell, XSTR(OPERATION_TEST_CHG_STAT), true, buf, NULL);
	}
	return 0;
}
#endif // OPERATION_TEST_CHG_STAT

#ifdef OPERATION_TEST_FLASH
/*!
 * @brief Test external flash. Check init and run test read write of dummy message.
 *
 *
 * @return
 */
static int cmd_test_external_flash(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(shell, "ACK %s", XSTR(OPERATION_TEST_FLASH));

	// Init
	bool flash_status = get_flash_status();
	sprintf(buf, "%d", flash_status);
	if (!flash_status) {
		cmd_shell_print_output(shell, XSTR(OPERATION_TEST_FLASH), false, buf, NULL);
		return 0;
	}

	// Read write test
	int err = test_flash();
	if (err) {
		cmd_shell_print_output(shell, XSTR(OPERATION_TEST_FLASH), false, buf, NULL);
		return 0;
	}

	cmd_shell_print_output(shell, XSTR(OPERATION_TEST_FLASH), true, buf, NULL);
	return 0;
}
#endif // OPERATION_TEST_FLASH

#ifdef OPERATION_TEST_FLASH_ERASE
/*!
 * @brief Clear flash data.
 *
 *
 * @return
 */
static int cmd_test_external_flash_erase(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(shell, "ACK %s", XSTR(OPERATION_TEST_FLASH_ERASE));

	int err = clear_flash_data();
	if (err) {
		cmd_shell_print_output(shell, XSTR(OPERATION_TEST_FLASH_ERASE), false, NULL, NULL);
	} else {
		cmd_shell_print_output(shell, XSTR(OPERATION_TEST_FLASH_ERASE), true, NULL, NULL);
	}

	return 0;
}
#endif // OPERATION_TEST_FLASH_ERASE

#ifdef OPERATION_TEST_FLASH_SIZE
static int cmd_test_external_flash_size(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(shell, "ACK %s", XSTR(OPERATION_TEST_FLASH_SIZE));

	bool flash_status = get_flash_status();
	sprintf(buf, "%d", flash_status);
	if (!flash_status) {
		cmd_shell_print_output(shell, XSTR(OPERATION_TEST_FLASH), false, buf, NULL);
		return 0;
	}

	long size = flash_ext_get_size();
	if (size < 0) {
		cmd_shell_print_output(shell, XSTR(OPERATION_TEST_FLASH_SIZE), false, NULL, NULL);
	} else {
		sprintf(buf, "%ld", size);
		cmd_shell_print_output(shell, XSTR(OPERATION_TEST_FLASH_SIZE), true, buf, NULL);
	}

	return 0;
}
#endif /* OPERATION_TEST_FLASH_SIZE */

static int prv_lr11xx_system_init(const struct device *context)
{
	int ret = 0;
	const struct lr11xx_hal_context_cfg_t *config = context->config;

	ret = lr11xx_system_reset((void *)context);
	if (ret) {
		return ret;
	}

	const lr11xx_system_reg_mode_t regulator = config->reg_mode;
	ret = lr11xx_system_set_reg_mode((void *)context, regulator);
	if (ret) {
		return ret;
	}

	const lr11xx_system_rfswitch_cfg_t rf_switch_setup = config->rf_switch_cfg;
	ret = lr11xx_system_set_dio_as_rf_switch(context, &rf_switch_setup);
	if (ret) {
		return ret;
	}

	const struct lr11xx_hal_context_tcxo_cfg_t tcxo_cfg = config->tcxo_cfg;
	if (tcxo_cfg.has_tcxo == true) {
		const uint32_t timeout_rtc_step =
			lr11xx_radio_convert_time_in_ms_to_rtc_step(tcxo_cfg.timeout_ms);
		ret = lr11xx_system_set_tcxo_mode(context, tcxo_cfg.supply, timeout_rtc_step);
		if (ret) {
			return ret;
		}
	}

	// Configure the Low Frequency Clock
	const struct lr11xx_hal_context_lf_clck_cfg_t lf_clk_cfg = config->lf_clck_cfg;
	ret = lr11xx_system_cfg_lfclk(context, lf_clk_cfg.lf_clk_cfg, lf_clk_cfg.wait_32k_ready);
	if (ret) {
		return ret;
	}

	ret = lr11xx_system_clear_errors(context);
	if (ret) {
		return ret;
	}

	ret = lr11xx_system_calibrate(context, 0x3F);
	if (ret) {
		return ret;
	}

	uint16_t errors;
	ret = lr11xx_system_get_errors(context, &errors);
	if (ret) {
		return ret;
	}

	ret = lr11xx_system_clear_errors(context);
	if (ret) {
		return ret;
	}

	ret = lr11xx_system_clear_irq_status(context, LR11XX_SYSTEM_IRQ_ALL_MASK);
	if (ret) {
		return ret;
	}

	return 0;
}

static void prv_lr11xx_system_deinit(const struct device *context)
{
	lr11xx_system_reset((void *)context);

	lr11xx_system_sleep_cfg_t cfg = {.is_rtc_timeout = false, .is_warm_start = false};

	lr11xx_system_set_sleep(context, cfg, 100000);
}

#ifdef OPERATION_TEST_LORA
/*!
 * @brief Test LoRa module.
 * Test will check if LoRa module is initialized, read its FW and modem version, device EUI and App
 * Key.
 *
 * @return
 */
static int cmd_test_lora(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(shell, "ACK %s", XSTR(OPERATION_TEST_LORA));

	uint8_t dev_eui[8];
	int err;

	/* Bind to lr11xx device and read lr fw version */
	const struct device *context = DEVICE_DT_GET_ONE(irnas_lr11xx);

	lr11xx_system_version_t version_trx = {0x00};
	err = lr11xx_system_get_version(context, &version_trx);
	if (err) {
		prv_lr11xx_system_deinit(context);
		cmd_shell_print_output(shell, XSTR(OPERATION_TEST_LORA), false, NULL, NULL);
		return 0;
	}

	err = lr11xx_system_read_uid(context, dev_eui);
	if (err) {
		prv_lr11xx_system_deinit(context);
		cmd_shell_print_output(shell, XSTR(OPERATION_TEST_LORA), false, NULL, NULL);
		return 0;
	}

	prv_lr11xx_system_deinit(context);

	uint8_t val[10];
	sprintf(val, "0x%04X", version_trx.fw);
	int offset = 0;
	offset += sprintf(buf + offset, "DEV_EUI:");
	for (int i = 0; i < sizeof(dev_eui); i++) {
		offset += sprintf(buf + offset, "%02X ", dev_eui[i]);
	}
	offset += sprintf(buf + offset, "|APP_KEY:");
	for (int i = 0; i < 16; i++) {
		offset += sprintf(buf + offset, "%02X ", Main_settings.app_key->def_val[i]);
	}

	cmd_shell_print_output(shell, XSTR(OPERATION_TEST_LORA), true, val, buf);

	return 0;
}
#endif // OPERATION_TEST_LORA

#ifdef OPERATION_TEST_LORA_RX
static int prv_radio_lora_init_rx(const void *context, uint32_t frequency_hz)
{
	int ret = 0;

	ret = lr11xx_radio_cfg_rx_boosted(context, true);

	return ret;
}

/**
 * @brief read RSSI value from radio
 *
 * @param context LR11xx device context
 * @param[in] frequency frequency to read RSSI from
 * @param[out] rssi address to store negative rssi in dbm
 * @return int -EBUSY if error, 0 if success
 */
static int lr_read_rssi(const struct device *context, uint32_t frequency, int *rssi)
{
	if (lr11xx_radio_set_rf_freq(context, frequency)) {
		return -EBUSY;
	}

	int8_t rssi_in_dbm = 0;

	if (lr11xx_radio_get_rssi_inst(context, &rssi_in_dbm)) {
		return -EBUSY;
	}
	*rssi = rssi_in_dbm;

	return 0;
}

/**
 * @brief Test LoRa RX functionality on 868 MHz and 2.4 GHz bands.
 *
 * The test consists of configuring the LoRa module to receive packets on both 868 MHz and 2.4 GHz
 * frequencies.
 *
 * @return
 */
static int cmd_test_lora_rx(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	/* Get frequency from first argument */
	uint32_t frequency_hz;
	if (argc == 2) {
		frequency_hz = ((uint32_t)atoi(argv[1])) * 1000; /* Convert kHz to Hz */
		shell_print(shell, "ACK %s %u Hz", XSTR(OPERATION_TEST_LORA_RX), frequency_hz);
	} else {
		frequency_hz = 868000000; /* Default frequency */
		shell_print(shell, "ACK %s %u Hz", XSTR(OPERATION_TEST_LORA_RX), frequency_hz);
		shell_print(shell, "Incorrect parameter count. Setting frequency to 868 MHz");
	}

	/* Bind to lr11xx device and read lr fw version */
	const struct device *context = DEVICE_DT_GET_ONE(irnas_lr11xx);

	/* We have to re-initialize the LR11xx system, otherwise we see a drop in detected RSSI.
	 * Initializing via prv_lr11xx_system_init() produces a drop of 50 dBm on input*/
	lr_start();
	k_sleep(K_MSEC(200));
	lorawan_suspend();

	prv_radio_lora_init_rx(context, frequency_hz);

	/* Set RF front end to RX LNA if present */
#ifdef CONFIG_RF_FRONT_END_MODULE
#if DT_NODE_HAS_STATUS(DT_ALIAS(serial_uart), okay)
	const struct device *serial_uart_dev = DEVICE_DT_GET(DT_ALIAS(serial_uart));
	uart_pm_disable(serial_uart_dev->name);
#endif /* DT_NODE_HAS_STATUS(DT_ALIAS(serial_uart), okay) */

	rf_front_end_module_init();
	rf_front_end_module_set_mode(RF_FRONT_END_MODE_RX_LNA);
#endif /* CONFIG_RF_FRONT_END_MODULE */

	/* Perform scan */
	if (lr11xx_radio_set_rf_freq(context, frequency_hz)) {
		return -EIO;
	}
	if (lr11xx_system_set_standby(context, LR11XX_SYSTEM_STANDBY_CFG_RC)) {
		return -EIO;
	}

	uint16_t lora_rx_timeout_ms = 5000; /* RX timeout in ms */

	if (lr11xx_radio_set_rx(context, lora_rx_timeout_ms)) {
		return -EIO;
	}

	k_sleep(K_SECONDS(1)); /* Wait for RX to settle */

	int low_rssi_limit = -130;

	for (int i = 0; i < lora_rx_timeout_ms / 1000 - 1; i++) {
		int res = low_rssi_limit;
		if (lr_read_rssi(context, frequency_hz, &res)) {
			return -EIO;
		}
		shell_print(shell, "RSSI reading %d dBm", res);
		k_sleep(K_SECONDS(1));
	}

#ifdef CONFIG_RF_FRONT_END_MODULE
	rf_front_end_module_set_mode(RF_FRONT_END_MODE_SLEEP);

#if DT_NODE_HAS_STATUS(DT_ALIAS(serial_uart), okay)
	uart_pm_enable(serial_uart_dev->name);
#endif /* DT_NODE_HAS_STATUS(DT_ALIAS(serial_uart), okay) */
#endif /* CONFIG_RF_FRONT_END_MODULE */

	/* Deinitialize LR11xx system */
	prv_lr11xx_system_deinit(context);

	/* End test */
	cmd_shell_print_output(shell, XSTR(OPERATION_TEST_LORA_RX), true, 0, buf);

	return 0;
}
#endif /* OPERATION_TEST_LORA_RX */

#ifdef OPERATION_TEST_LORA_TX
/**
 * @brief Initialize LR11xx radio in LoRa mode.
 *
 * @param context LR11xx device context
 * @param frequency_hz Frequency in Hz

 */
static int prv_radio_lora_init_tx(const void *context, uint32_t frequency_hz)
{
	int ret = 0;

	/* Set packet type */
	ret = lr11xx_radio_set_pkt_type(context, LR11XX_RADIO_PKT_TYPE_LORA);
	if (ret) {
		return ret;
	}

	/* Set frequency */
	ret = lr11xx_radio_set_rf_freq(context, frequency_hz);
	if (ret) {
		return ret;
	}

	/* Set RSSI calibration */
	ret = lr11xx_radio_set_rssi_calibration(
		context, lr11xx_board_get_rssi_calibration_table(frequency_hz));
	if (ret) {
		return ret;
	}

	/* Set maximum output power based on frequency */
	int8_t tx_power_output = 0;
	if (frequency_hz < 1000000000) {
		tx_power_output = 22; /* range [-17 22] */
	} else {
		tx_power_output = 13; /* range [-18 13] */
	}

	/* Get power config for given settings */
	const lr11xx_board_pa_pwr_cfg_t *pa_pwr_cfg =
		lr11xx_board_get_pa_pwr_cfg(frequency_hz, tx_power_output);
	if (pa_pwr_cfg == NULL) {
		return -EIO;
	}

	/* Set power config */
	ret = lr11xx_radio_set_pa_cfg(context, &(pa_pwr_cfg->pa_config));
	if (ret) {
		return ret;
	}

	/* Set TX params */
	ret = lr11xx_radio_set_tx_params(context, pa_pwr_cfg->power, LR11XX_RADIO_RAMP_48_US);
	if (ret) {
		return ret;
	}

	return ret;
}

/**
 * @brief Test LoRa TX functionality via continuous wave.
 *
 * @return 0 on success, negative error code on failure
 */
static int cmd_test_lora_tx(const struct shell *shell, size_t argc, char **argv)
{

	/* Get frequency from first argument */
	uint32_t frequency_hz;
	if (argc == 2) {
		frequency_hz = ((uint32_t)atoi(argv[1])) * 1000; /* Convert kHz to Hz */
		shell_print(shell, "ACK %s %u Hz", XSTR(OPERATION_TEST_LORA_TX), frequency_hz);
	} else {
		frequency_hz = 868000000; /* Default frequency */
		shell_print(shell, "ACK %s %u Hz", XSTR(OPERATION_TEST_LORA_TX), frequency_hz);
		shell_print(shell, "Incorrect parameter count. Setting frequency to 868 MHz");
	}

	/* Bind to lr11xx device and read lr fw version */
	const struct device *context = DEVICE_DT_GET_ONE(irnas_lr11xx);

	lr_start();
	k_sleep(K_MSEC(200));
	lorawan_suspend();

	int err = prv_lr11xx_system_init(context);
	if (err) {
		prv_lr11xx_system_deinit(context);
		cmd_shell_print_output(shell, XSTR(OPERATION_TEST_LORA_TX), false, NULL, NULL);
		return err;
	}

	err = prv_radio_lora_init_tx(context, frequency_hz);
	if (err) {
		prv_lr11xx_system_deinit(context);
		cmd_shell_print_output(shell, XSTR(OPERATION_TEST_LORA_TX), false, NULL, NULL);
		return err;
	}

#ifdef CONFIG_RF_FRONT_END_MODULE
	if (frequency_hz >= 1000000000U) {
		rf_front_end_module_init();
		rf_front_end_module_set_mode(RF_FRONT_END_MODE_TX);
	}
#endif /* CONFIG_RF_FRONT_END_MODULE */

	/* CW test */
	err = lr11xx_radio_set_tx_cw(context);
	if (err) {
		cmd_shell_print_output(shell, XSTR(OPERATION_TEST_LORA_TX), false, NULL, NULL);
	} else {

		/* Transmit CW for 5 seconds */
		int64_t start = k_uptime_get();
		while (k_uptime_get() - start < 5000) {
			k_sleep(K_MSEC(1));
		}
	}
#ifdef CONFIG_RF_FRONT_END_MODULE
	if (frequency_hz >= 1000000000U) {
		rf_front_end_module_set_mode(RF_FRONT_END_MODE_SLEEP);
	}
#endif /* CONFIG_RF_FRONT_END_MODULE */

	/* Deinitialize LR11xx system */
	prv_lr11xx_system_deinit(context);

	/* End test */
	cmd_shell_print_output(shell, XSTR(OPERATION_TEST_LORA_TX), true, 0, buf);

	return 0;
}
#endif /* OPERATION_TEST_LORA_TX */

#ifdef OPERATION_TEST_GPS_LORA
static int lr_gnss_err = 0;
static bool lr_gnss_done = false;

/**
 * @brief External handler for GNSS scan done event.
 *
 * @param[in] err - gnss scan error code
 */
static void prv_handle_gnss_done(int err)
{
	lr_gnss_err = err;
	lr_gnss_done = true;
}

/*!
 * @brief Test obtain LoRa GPS and print number of satellites and payload.
 *
 *
 * @return
 */
static int cmd_test_lora_gps(const struct shell *shell, size_t argc, char **argv)
{
	// Check for user argument
	uint32_t timestamp = 0;
	if (argc == 2) {
		timestamp = atoi(argv[1]);
	}

	shell_print(shell, "ACK %s", XSTR(OPERATION_TEST_GPS_LORA));

	/* Bind to lr11xx device and read lr fw version */
	const struct device *context = DEVICE_DT_GET_ONE(irnas_lr11xx);

	int err = prv_lr11xx_system_init(context);
	if (err) {
		prv_lr11xx_system_deinit(context);
		cmd_shell_print_output(shell, XSTR(OPERATION_TEST_GPS_LORA), false, NULL, NULL);
		return 0;
	}

	/* Update Almanac */
	almanac_update(context);

	/* Register handler for GNSS scan done event */
	gnss_results_handler_register(prv_handle_gnss_done);

	uint8_t payload[255];
	uint8_t payload_size;
	/* Get time */
	uint32_t ref_time = get_global_gps_time();
	if (timestamp > 0) {
		ref_time = unix_to_gps(timestamp);
	}

	lr_gnss_done = false;

	/* Convert lat and lon to reference position struct */
	lr11xx_gnss_solver_assistance_position_t assistance_position;
	assistance_position.latitude = ((float)Main_values.gps_lat->def_val) / 10000000;
	assistance_position.longitude = ((float)Main_values.gps_lon->def_val) / 10000000;

	/* Save assistance position */
	gnss_scan_set_ref_position(assistance_position);

	/* Save reference gps time */
	gnss_scan_set_ref_gps_time(ref_time);

	/* Preform autonomous or assisted scan */
	// gnss_scan_assisted(context, 10, LR11XX_GNSS_GPS_MASK | LR11XX_GNSS_BEIDOU_MASK);
	gnss_scan_autonomous(context, 10, LR11XX_GNSS_GPS_MASK | LR11XX_GNSS_BEIDOU_MASK);

	/* We need to block other functions until we do not get gnss cb */
	while (!lr_gnss_done) {
		k_sleep(K_MSEC(100));
	}

	if (lr_gnss_err) {
		cmd_shell_print_output(shell, XSTR(OPERATION_TEST_GPS_LORA), false, NULL, NULL);
	} else {

		uint8_t n_sat;
		payload_size = gnss_get_last_sat_data(payload, &n_sat);
		payload_size = gnss_get_last_nav_data(payload);

		int offset = 0;
		offset += sprintf(buf, "SAT:%d|FIX:", n_sat);
		for (uint8_t i = 0; i < payload_size; i++) {
			offset += sprintf(buf + offset, "%02X ", payload[i]);
		}

		cmd_shell_print_output(shell, XSTR(OPERATION_TEST_GPS_LORA), true, NULL, buf);
	}

	prv_lr11xx_system_deinit(context);

	return 0;
}
#endif // OPERATION_TEST_GPS_LORA

#ifdef OPERATION_TEST_WIFI_LORA

static bool lr_wifi_scan_done = false;
static int lr_wifi_scan_err = 0;

static void prv_handle_wifi_scan_done(int err)
{
	lr_wifi_scan_err = err;
	if (!err) {
		/* Invoke storing of WiFi scan results */
		wifi_scan_store_results(get_global_unix_time());
	}

	lr_wifi_scan_done = true;
}

/*!
 * @brief Test WiFi scan via LR. Display number of results, MAC addresses and rssi.
 *
 *
 * @return
 */
static int cmd_test_lora_wifi_scan(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(shell, "ACK %s", XSTR(OPERATION_TEST_WIFI_LORA));

	/* Bind to lr11xx device and read lr fw version */
	const struct device *context = DEVICE_DT_GET_ONE(irnas_lr11xx);

	int err = prv_lr11xx_system_init(context);
	if (err) {
		prv_lr11xx_system_deinit(context);
		cmd_shell_print_output(shell, XSTR(OPERATION_TEST_WIFI_LORA), false, NULL, NULL);
		return 0;
	}

	wifi_scan_results_handler_register(prv_handle_wifi_scan_done);

	/* Clear results */
	wifi_scan_data_clear_results();

	/* Set RF front end to RX LNA if present */
#ifdef CONFIG_RF_FRONT_END_MODULE
	rf_front_end_module_init();
	rf_front_end_module_set_mode(RF_FRONT_END_MODE_RX_LNA);
#endif /* CONFIG_RF_FRONT_END_MODULE */

	/* Perform scan */
	wifi_scan_default(context);

#ifdef CONFIG_RF_FRONT_END_MODULE
	rf_front_end_module_set_mode(RF_FRONT_END_MODE_SLEEP);
#endif /* CONFIG_RF_FRONT_END_MODULE */

	if (lr_wifi_scan_err < 0) {
		cmd_shell_print_output(shell, XSTR(OPERATION_TEST_WIFI_LORA), false, NULL, NULL);
	} else {
		// Get results
		struct wifi_scan_results *res = wifi_scan_data_get_results();
		char val[4];
		sprintf(val, "%d", res->nb_results);

		int offset = 0;
		offset += sprintf(buf + offset, "RES:");
		if (res->nb_results > 0) {
			offset += sprintf(buf + offset, "[");
			for (uint8_t i = 0; i < res->nb_results; i++) {
				offset += sprintf(
					buf + offset, "[%02X:%02X:%02X:%02X:%02X:%02X, %d],",
					res->results[i].mac_address[0],
					res->results[i].mac_address[1],
					res->results[i].mac_address[2],
					res->results[i].mac_address[3],
					res->results[i].mac_address[4],
					res->results[i].mac_address[5], res->results[i].rssi);
			}
			offset += sprintf(buf + offset, "]");
		}

		cmd_shell_print_output(shell, XSTR(OPERATION_TEST_WIFI_LORA), true, val, buf);
	}

	/* Reset */
	prv_lr11xx_system_deinit(context);

	return 0;
}
#endif // OPERATION_TEST_WIFI_LORA

#ifdef OPERATION_TEST_LR_S_BAND_CW

#define FHSS_RF_FREQ_IN_HZ       2008450000
#define FHSS_TX_OUTPUT_POWER_DBM 13
#define FHSS_PA_RAMP_TIME        LR11XX_RADIO_RAMP_208_US

static int prv_radio_fhss_init(const void *context)
{
	int ret;

	/* Init FHSS mode - set packet type */
	ret = lr11xx_lr_fhss_init(context);
	if (ret) {
		return ret;
	}

	/* Set frequency */
	ret = lr11xx_radio_set_rf_freq(context, FHSS_RF_FREQ_IN_HZ);
	if (ret) {
		return ret;
	}

	/* Set RSSI calibration */
	ret = lr11xx_radio_set_rssi_calibration(
		context, lr11xx_board_get_rssi_calibration_table(FHSS_RF_FREQ_IN_HZ));
	if (ret) {
		return ret;
	}

	/* Get power config for given settings */
	const lr11xx_board_pa_pwr_cfg_t *pa_pwr_cfg =
		lr11xx_board_get_pa_pwr_cfg(FHSS_RF_FREQ_IN_HZ, FHSS_TX_OUTPUT_POWER_DBM);
	if (pa_pwr_cfg == NULL) {
		return -EIO;
	}

	/* Set power config */
	ret = lr11xx_radio_set_pa_cfg(context, &(pa_pwr_cfg->pa_config));
	if (ret) {
		return ret;
	}

	/* Set TX params */
	ret = lr11xx_radio_set_tx_params(context, pa_pwr_cfg->power, FHSS_PA_RAMP_TIME);
	if (ret) {
		return ret;
	}

	return 0;
}

static int cmd_test_lora_s_band_cw(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(shell, "ACK %s", XSTR(OPERATION_TEST_LR_S_BAND_CW));

	/* Bind to lr11xx device and read lr fw version */
	const struct device *context = DEVICE_DT_GET_ONE(irnas_lr11xx);

	int err = prv_lr11xx_system_init(context);
	if (err) {
		prv_lr11xx_system_deinit(context);
		cmd_shell_print_output(shell, XSTR(OPERATION_TEST_LR_S_BAND_CW), false, NULL, NULL);
		return 0;
	}

	err = prv_radio_fhss_init(context);
	if (err) {
		prv_lr11xx_system_deinit(context);
		cmd_shell_print_output(shell, XSTR(OPERATION_TEST_LR_S_BAND_CW), false, NULL, NULL);
		return 0;
	}

#ifdef CONFIG_RF_FRONT_END_MODULE
	rf_front_end_module_init();
	rf_front_end_module_set_mode(RF_FRONT_END_MODE_TX);
#endif /* CONFIG_RF_FRONT_END_MODULE */

	/* CW test */
	err = lr11xx_radio_set_tx_cw(context);
	if (err) {
		cmd_shell_print_output(shell, XSTR(OPERATION_TEST_LR_S_BAND_CW), false, NULL, NULL);
	} else {

		int64_t start = k_uptime_get();
		while (k_uptime_get() - start < 5000) {
			k_sleep(K_MSEC(1));
		}

		/* Reset module */
		prv_lr11xx_system_deinit(context);
	}
#ifdef CONFIG_RF_FRONT_END_MODULE
	rf_front_end_module_set_mode(RF_FRONT_END_MODE_SLEEP);
#endif /* CONFIG_RF_FRONT_END_MODULE */

	if (!err) {
		cmd_shell_print_output(shell, XSTR(OPERATION_TEST_LR_S_BAND_CW), true, NULL, NULL);
	}
	return 0;
}

#endif // OPERATION_TEST_LR_S_BAND_CW

#ifdef OPERATION_TEST_GPS
/*!
 * @brief Test Ublox module
 * Check if module is available. Then turn power on for 5 seconds.
 *
 *
 * @return
 */
static int cmd_test_ublox_gps(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(shell, "ACK %s", XSTR(OPERATION_TEST_GPS));

	// Check if module is operational
	bool gps_en = gps_get_enabled();
	if (!gps_en) {
		cmd_shell_print_output(shell, XSTR(OPERATION_TEST_GPS), false, NULL, NULL);
		return 0;
	}
	// Turn on GPS module for 5 seconds
	gps_power(1);
	uint64_t start = k_uptime_get();
	while ((uint32_t)((k_uptime_get() - start) / 1000) < 5) {
		k_sleep(K_SECONDS(1));
	}
	gps_power(0);
	cmd_shell_print_output(shell, XSTR(OPERATION_TEST_GPS), true, NULL, NULL);

	return 0;
}
#endif // OPERATION_TEST_GPS

#ifdef OPERATION_TEST_FIX_GPS
/**
 * @brief Obtain GPS fix and  display results.
 *
 */
static int cmd_test_ublox_fix(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(shell, "ACK %s", XSTR(OPERATION_TEST_FIX_GPS));

	// Set on time after cold fix to 0
	Main_settings.ublox_leave_on->def_val = 0;
	// Get new fix
	int err = gps_get_fix();
	if (err) {
		cmd_shell_print_output(shell, XSTR(OPERATION_TEST_FIX_GPS), false, NULL, NULL);
	} else {

		/* Fix status */
		bool fix = false;
		uint8_t hot_retry = 0;
		uint8_t cold_retry = 0;
		uint16_t ttf = 0;
		gps_get_last_fix_status(&fix, &hot_retry, &cold_retry, &ttf);

		/* Fix data */
		struct gps_ublox_position_data position;
		gps_get_last_fix_data(&position);

		char val[5];
		sprintf(val, "%d", fix);
		int offset = 0;
		offset += sprintf(buf + offset, "TTF:%d|POS:[%d %d %d]|SIV:%d|", ttf,
				  position.latitude, position.longitude, position.altitude,
				  position.SIV);

		// compose fix payload
		uint8_t payload[256];
		payload[0] = Main_messages.msg_ublox_location->id;
		payload[1] = Main_messages.msg_ublox_location->length;
		payload[2] = fix;
		payload[3] = hot_retry;
		payload[4] = cold_retry;
		payload[5] = ttf >> 8;
		payload[6] = ttf;
		/* Latitude */
		memcpy(&payload[7], &position.latitude, sizeof(position.latitude));
		/* Longitude */
		memcpy(&payload[11], &position.longitude, sizeof(position.longitude));
		/* Altitude */
		memcpy(&payload[15], &position.altitude, sizeof(position.altitude));
		/* Fix type */
		payload[19] = position.fix_type;
		/* SIV */
		payload[20] = position.SIV;
		/* Scaled accuracy */
		memcpy(&payload[21], &position.scaled_accuracy, sizeof(position.scaled_accuracy));
		/* PDOP */
		payload[23] = position.PDOP;

		/* Fix time */
		uint32_t time;
		gps_get_last_fix_time(&time);
		memcpy(&payload[24], &time, sizeof(time));

		/* Active tracking */
		payload[28] = gps_get_active_tracking();
		if (gps_get_active_tracking()) {
			memcpy(&payload[29], &position.scaled_cog, sizeof(position.scaled_cog));
			payload[31] = position.scaled_sog;
		}

		int payload_size = Main_messages.msg_ublox_location->length + MESSAGE_HEAD_LEN;
		offset += sprintf(buf + offset, "FIX:");
		for (uint8_t i = 0; i < payload_size; i++) {
			offset += sprintf(buf + offset, "%02X ", payload[i]);
		}
		offset += sprintf(buf + offset, "|");

		// Compose satellite data payload
		err = gps_get_sat_data(payload + 2, sizeof(payload) - 2);
		if (err >= 0) {
			payload_size = (uint8_t)err;
			payload[0] = Main_messages.msg_ublox_satellites->id;
			payload[1] = payload_size;
			payload_size += MESSAGE_HEAD_LEN;

			offset += sprintf(buf + offset, "SAT:");
			for (uint8_t i = 0; i < payload_size; i++) {
				offset += sprintf(buf + offset, "%02X ", payload[i]);
			}
		}
		cmd_shell_print_output(shell, XSTR(OPERATION_TEST_FIX_GPS), true, val, buf);
	}

	return 0;
}
#endif // OPERATION_TEST_FIX_GPS

#ifdef OPERATION_TEST_BT_SCAN
const struct bt_le_scan_param test_scan_param = {
	.type = BT_HCI_LE_SCAN_PASSIVE,
	.options = BT_LE_SCAN_OPT_FILTER_DUPLICATE,
	.interval = 32, //(N * 0.625 ms)
	.window = 20,   //(N * 0.625 ms)
};

struct test_bt_scan_res {
	int8_t rssi;
	bt_addr_t mac_address;
};

struct test_bt_scan_res bt_res[20];
uint8_t test_bt_scan_n = 0;

static void test_scan_cb(const bt_addr_le_t *addr, int8_t rssi, uint8_t adv_type,
			 struct net_buf_simple *buf)
{
	if (test_bt_scan_n < 20) {
		bt_res[test_bt_scan_n].mac_address = addr->a;
		bt_res[test_bt_scan_n].rssi = rssi;
	}
	test_bt_scan_n++;
}

/*!
 * @brief Test BT scan.
 *
 *
 * @return
 */
static int cmd_test_bt_scan(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(shell, "ACK %s", XSTR(OPERATION_TEST_BT_SCAN));
	// Start scan
	int err = bt_le_scan_start(&test_scan_param, test_scan_cb);
	if (err) {
		cmd_shell_print_output(shell, XSTR(OPERATION_TEST_BT_SCAN), false, NULL, NULL);
		return 0;
	}
	k_sleep(K_MSEC(Main_settings.ble_scan_duration->def_val));
	err = bt_le_scan_stop();

	char val[3];
	sprintf(val, "%d", test_bt_scan_n);
	int offset = 0;
	offset += sprintf(buf + offset, "RES:");
	if (test_bt_scan_n > 0) {
		offset += sprintf(buf + offset, "[");
		for (uint8_t i = 0; i < MIN(20, test_bt_scan_n); i++) {
			offset +=
				sprintf(buf + offset, "[%02X:%02X:%02X:%02X:%02X:%02X, %d],",
					bt_res[i].mac_address.val[0], bt_res[i].mac_address.val[1],
					bt_res[i].mac_address.val[2], bt_res[i].mac_address.val[3],
					bt_res[i].mac_address.val[4], bt_res[i].mac_address.val[5],
					bt_res[i].rssi);
		}
		offset += sprintf(buf + offset, "]");
	}

	cmd_shell_print_output(shell, XSTR(OPERATION_TEST_BT_SCAN), true, val, buf);
	return 0;
}
#endif // OPERATION_TEST_BT_SCAN

#ifdef OPERATION_TEST_GET_FACTORY_NAME
/**
 * @brief Get factory name in UICR
 *
 */
static int cmd_get_factory_name(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(shell, "ACK %s", XSTR(OPERATION_TEST_GET_FACTORY_NAME));

	char name[8];
	read_factory_name(name);

	cmd_shell_print_output(shell, XSTR(OPERATION_TEST_GET_FACTORY_NAME), true, name, NULL);
	return 0;
}
#endif // OPERATION_TEST_GET_FACTORY_NAME

#ifdef OPERATION_TEST_GET_MAC
/**
 * @brief Get tracker MAC address.
 *
 */
static int cmd_get_mac(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(shell, "ACK %s", XSTR(OPERATION_TEST_GET_MAC));

	uint8_t mac[6];
	get_mac(mac, 6);
	sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X", mac[5], mac[4], mac[3], mac[2], mac[1],
		mac[0]);

	cmd_shell_print_output(shell, XSTR(OPERATION_TEST_GET_MAC), true, buf, NULL);
	return 0;
}
#endif // OPERATION_TEST_GET_MAC

#ifdef OPERATION_TEST_LED
static int cmd_test_led(const struct shell *shell, size_t argc, char **argv)
{
	// Check for number of arguments
	if (argc != 2) {
		shell_print(shell, "NACK %s", XSTR(OPERATION_TEST_LED));
		return 0;
	} else {
		if (strncmp(argv[1], OPERATION_TEST_LED_R, sizeof(OPERATION_TEST_LED_R) - 1) == 0) {
			shell_print(shell, "ACK %s %s", XSTR(OPERATION_TEST_LED), argv[1]);
#if DT_NODE_EXISTS(DT_NODELABEL(led_r))
			struct gpio_dt_spec led_spec = GPIO_DT_SPEC_GET(DT_NODELABEL(led_r), gpios);
			if (device_is_ready(led_spec.port)) {
				gpio_pin_configure_dt(&led_spec, GPIO_OUTPUT_INACTIVE);
				gpio_pin_set_dt(&led_spec, 1);
				k_sleep(K_SECONDS(1));
				gpio_pin_set_dt(&led_spec, 0);
				cmd_shell_print_output(shell, XSTR(OPERATION_TEST_LED), true, NULL,
						       NULL);
				return 1;
			}

#endif // DT_NODE_EXISTS(DT_NODELABEL(led_r))
		} else if (strncmp(argv[1], OPERATION_TEST_LED_G,
				   sizeof(OPERATION_TEST_LED_G) - 1) == 0) {
			shell_print(shell, "ACK %s %s", XSTR(OPERATION_TEST_LED), argv[1]);
#if DT_NODE_EXISTS(DT_NODELABEL(led_g))
			const struct gpio_dt_spec led_spec =
				GPIO_DT_SPEC_GET(DT_NODELABEL(led_g), gpios);
			if (device_is_ready(led_spec.port)) {
				gpio_pin_configure_dt(&led_spec, GPIO_OUTPUT_INACTIVE);
				gpio_pin_set_dt(&led_spec, 1);
				k_sleep(K_SECONDS(1));
				gpio_pin_set_dt(&led_spec, 0);
				cmd_shell_print_output(shell, XSTR(OPERATION_TEST_LED), true, NULL,
						       NULL);
				return 1;
			}
#endif // DT_NODE_EXISTS(DT_NODELABEL(led_g))
		} else if (strncmp(argv[1], OPERATION_TEST_LED_B,
				   sizeof(OPERATION_TEST_LED_B) - 1) == 0) {
			shell_print(shell, "ACK %s %s", XSTR(OPERATION_TEST_LED), argv[1]);
#if DT_NODE_EXISTS(DT_NODELABEL(led_b))
			const struct gpio_dt_spec led_spec =
				GPIO_DT_SPEC_GET(DT_NODELABEL(led_b), gpios);
			if (device_is_ready(led_spec.port)) {
				gpio_pin_configure_dt(&led_spec, GPIO_OUTPUT_INACTIVE);
				gpio_pin_set_dt(&led_spec, 1);
				k_sleep(K_SECONDS(1));
				gpio_pin_set_dt(&led_spec, 0);
				cmd_shell_print_output(shell, XSTR(OPERATION_TEST_LED), true, NULL,
						       NULL);
				return 1;
			}
#endif // DT_NODE_EXISTS(DT_NODELABEL(led_b))
		} else {
			shell_print(shell, "NACK %s %s", XSTR(OPERATION_TEST_LED), argv[1]);
			return 0;
		}
	}

	cmd_shell_print_output(shell, XSTR(OPERATION_TEST_LED), false, NULL, NULL);
	return 1;
}
#endif

#ifdef OPERATION_TEST_LOW_POWER
/**
 * @brief Test low power performance of device.
 *
 */
static int cmd_test_low_power(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(shell, "ACK %s", XSTR(OPERATION_TEST_LOW_POWER));
	k_sleep(K_SECONDS(2));

	// Perform low power test after exiting the test loop.
	// For some reason we cannot disable shell inside command function.
	low_power_test = true;
	testing_mode = false;

	return 0;
}
#endif // OPERATION_TEST_LOW_POWER

#ifdef OPERATION_TEST_SETTING
/**
 * @brief Store new settings in NVS. Tracker needs to be rebooted for them to take place.
 *
 */
static int cmd_test_change_setting(const struct shell *shell, size_t argc, char **argv)
{
	shell_print(shell, "ACK %s", XSTR(OPERATION_TEST_SETTING));

	if (argc <= 1) {
		cmd_shell_print_output(shell, XSTR(OPERATION_TEST_SETTING), false, NULL, NULL);
		return 0;
	}
	// Read new setting
	// Parse
	if (argv[1] == NULL) {
		cmd_shell_print_output(shell, XSTR(OPERATION_TEST_SETTING), false, NULL, NULL);
		return 0;
	}

	char tmp[2];
	uint8_t i = 0;
	// ID
	tmp[0] = argv[1][i];
	tmp[1] = argv[1][i + 1];
	i += 2;
	uint8_t setting_id = (uint8_t)strtol(tmp, NULL, 16);

	if (!check_setting_id(setting_id)) {
		sprintf(buf, "INVALID_ID");
		cmd_shell_print_output(shell, XSTR(OPERATION_TEST_SETTING), false, buf, NULL);
		return 0;
	}

	// LEN
	tmp[0] = argv[1][i];
	tmp[1] = argv[1][i + 1];
	i += 2;
	uint8_t setting_len = (uint8_t)strtol(tmp, NULL, 16);
	if (setting_len != get_setting_len(setting_id)) {
		sprintf(buf, "INVALID_LEN");
		cmd_shell_print_output(shell, XSTR(OPERATION_TEST_SETTING), false, buf, NULL);
		return 0;
	}

	// Store setting
	uint8_t new_data[setting_len];
	for (uint8_t j = 0; j < setting_len; j++) {
		tmp[0] = argv[1][i];
		tmp[1] = argv[1][i + 1];
		i += 2;
		new_data[j] = (uint8_t)strtol(tmp, NULL, 16);
	}

	// Setting update
	set_setting_value_by_id(setting_id, new_data, setting_len);
	// NVS update
	nvs_storage_write((uint16_t)setting_id, new_data, setting_len);

	// Read back
	uint8_t read_data[setting_len];
	nvs_storage_read((uint16_t)setting_id, read_data, setting_len);

	int offset = 0;
	offset += sprintf(buf + offset, "ID:%02X|VAL:", setting_id);
	for (uint8_t j = 0; j < setting_len; j++) {
		offset += sprintf(buf + offset, "%02X ", read_data[j]);
	}

	cmd_shell_print_output(shell, XSTR(OPERATION_TEST_SETTING), true, NULL, buf);

	return 0;
}
#endif // OPERATION_TEST_SETTING

#ifdef OPERATION_TEST_GET_SETTING
/**
 * @brief Get setting value with port.
 *
 */
static int cmd_test_get_setting(const struct shell *shell, size_t argc, char **argv)
{
	shell_print(shell, "ACK %s", XSTR(OPERATION_TEST_GET_SETTING));

	if (argc <= 1) {
		cmd_shell_print_output(shell, XSTR(OPERATION_TEST_GET_SETTING), false, NULL, NULL);
		return 0;
	}
	// Read new setting
	// Parse
	if (argv[1] == NULL) {
		cmd_shell_print_output(shell, XSTR(OPERATION_TEST_GET_SETTING), false, NULL, NULL);
		return 0;
	}

	char tmp[2];
	uint8_t i = 0;
	// ID
	tmp[0] = argv[1][i];
	tmp[1] = argv[1][i + 1];
	i += 2;
	uint8_t setting_id = (uint8_t)strtol(tmp, NULL, 16);
	if (!check_setting_id(setting_id)) {
		sprintf(buf, "INVALID_ID");
		cmd_shell_print_output(shell, XSTR(OPERATION_TEST_GET_SETTING), false, buf, NULL);
		return 0;
	}

	// Get predicted length
	int len_p = get_setting_len(setting_id);
	uint8_t setting_buf[len_p];

	// Setting get
	int len = get_setting_by_id(setting_id, setting_buf);
	if (len != len_p) {
		sprintf(buf, "INVALID_LEN");
		cmd_shell_print_output(shell, XSTR(OPERATION_TEST_GET_SETTING), false, buf, NULL);
		return 0;
	}

	int offset = 0;
	offset += sprintf(buf + offset, "ID:%02X|VAL:", setting_id);
	offset += sprintf(buf + offset, "%02X ", PORT_SETTINGS);
	offset += sprintf(buf + offset, "%02X ", setting_id);
	offset += sprintf(buf + offset, "%02X ", len);
	for (uint8_t j = 0; j < len; j++) {
		offset += sprintf(buf + offset, "%02X ", setting_buf[j]);
	}

	cmd_shell_print_output(shell, XSTR(OPERATION_TEST_GET_SETTING), true, NULL, buf);

	return 0;
}
#endif // OPERATION_TEST_GET_SETTING

#ifdef OPERATION_TEST_EXIT
/**
 * @brief Set testing mode flag to false and exit testing procedure.
 *
 */
static int cmd_test_exit(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(shell, "ACK %s", XSTR(OPERATION_TEST_EXIT));

	testing_mode = false;

	return 0;
}
#endif // OPERATION_TEST_EXIT

/**
 * @brief Implement low power test.
 * Disable all threads, shell and uart.
 *
 */
void test_low_power(void)
{
	lorawan_suspend();
	disable_flash();
	stop_bt_service();
	// Disable shell
#ifdef CONFIG_SHELL_BACKEND_RTT
	shell_stop(shell_backend_rtt_get_ptr());
	shell_uninit(shell_backend_rtt_get_ptr(), NULL);
#endif // CONFIG_SHELL_BACKEND_RTT
#ifdef CONFIG_SHELL_BACKEND_SERIAL
	shell_stop(shell_backend_uart_get_ptr());
	shell_uninit(shell_backend_uart_get_ptr(), NULL);
#endif // CONFIG_SHELL_BACKEND_SERIAL

#ifdef CONFIG_RF_FRONT_END_MODULE
	rf_front_end_module_set_mode(RF_FRONT_END_MODE_SLEEP);
#endif /* CONFIG_RF_FRONT_END_MODULE */

	// Disable uart
#if DT_NODE_HAS_STATUS(DT_ALIAS(serial_uart), okay)
	const struct device *serial_uart_dev = DEVICE_DT_GET(DT_ALIAS(serial_uart));
	uart_pm_disable(serial_uart_dev->name);
#endif // DT_NODE_HAS_STATUS(DT_ALIAS(serial_uart), okay)

	k_sleep(K_SECONDS(100));
	sys_reboot(0);
}

void operation_test_setup(void)
{
#ifdef CONFIG_SHELL_BACKEND_RTT
	shell_print(shell_backend_rtt_get_ptr(), "Start shell test RTT.");
	shell_echo_set(shell_backend_rtt_get_ptr(), false);
#endif // CONFIG_SHELL_BACKEND_RTT
#ifdef CONFIG_SHELL_BACKEND_SERIAL
	shell_print(shell_backend_uart_get_ptr(), "Start shell test serial.");
	shell_echo_set(shell_backend_uart_get_ptr(), false);
#endif // CONFIG_SHELL_BACKEND_SERIAL

	/* Suspend smtc engine */
	lorawan_suspend();
	k_sleep(K_SECONDS(1));

	while (testing_mode) {
		k_sleep(K_MSEC(100));
	}

	// If selected, perform last test
	if (low_power_test) {
		test_low_power();
	}

	return;
}

#ifdef OPERATION_TEST_I2C_SCAN
SHELL_CMD_REGISTER(OPERATION_TEST_I2C_SCAN, NULL, OPERATION_TEST_I2C_SCAN_TEXT, cmd_i2c_scan);
#endif // OPERATION_TEST_I2C_SCAN

#ifdef OPERATION_TEST_ACC
SHELL_CMD_REGISTER(OPERATION_TEST_ACC, NULL, OPERATION_TEST_ACC_TEXT, cmd_test_lis2dw12);
#endif // OPERATION_TEST_ACC

#ifdef OPERATION_TEST_TEMPERATURE
SHELL_CMD_REGISTER(OPERATION_TEST_TEMPERATURE, NULL, OPERATION_TEST_TEMPERATURE_TEXT,
		   cmd_test_temp_sensor);
#endif // OPERATION_TEST_TEMPERATURE

#ifdef OPERATION_TEST_MIC
SHELL_CMD_REGISTER(OPERATION_TEST_MIC, NULL, OPERATION_TEST_MIC_TEXT, cmd_test_mic);
#endif // OPERATION_TEST_MIC

#ifdef OPERATION_TEST_BAT
SHELL_CMD_REGISTER(OPERATION_TEST_BAT, NULL, OPERATION_TEST_BAT_TEXT, cmd_test_battery);
#endif // OPERATION_TEST_BAT

#ifdef OPERATION_TEST_CHG
SHELL_CMD_REGISTER(OPERATION_TEST_CHG, NULL, OPERATION_TEST_CHG_TEXT, cmd_test_charging);
#endif // OPERATION_TEST_CHG

#ifdef OPERATION_TEST_CHG_EN
SHELL_CMD_REGISTER(OPERATION_TEST_CHG_EN, NULL, OPERATION_TEST_CHG_EN_TEXT,
		   cmd_test_enable_charging);
#endif // OPERATION_TEST_CHG_EN

#ifdef OPERATION_TEST_CHG_DIS
SHELL_CMD_REGISTER(OPERATION_TEST_CHG_DIS, NULL, OPERATION_TEST_CHG_DIS_TEXT,
		   cmd_test_disable_charging);
#endif // OPERATION_TEST_CHG_DIS

#ifdef OPERATION_TEST_CHG_STAT
SHELL_CMD_REGISTER(OPERATION_TEST_CHG_STAT, NULL, OPERATION_TEST_CHG_STAT_TEXT,
		   cmd_test_status_charging);
#endif // OPERATION_TEST_CHG_DIS

#ifdef OPERATION_TEST_FLASH
SHELL_CMD_REGISTER(OPERATION_TEST_FLASH, NULL, OPERATION_TEST_FLASH_TEXT, cmd_test_external_flash);
#endif // OPERATION_TEST_FLASH

#ifdef OPERATION_TEST_FLASH_ERASE
SHELL_CMD_REGISTER(OPERATION_TEST_FLASH_ERASE, NULL, OPERATION_TEST_FLASH_ERASE_TEXT,
		   cmd_test_external_flash_erase);
#endif // OPERATION_TEST_FLASH_ERASE

#ifdef OPERATION_TEST_FLASH_SIZE
SHELL_CMD_REGISTER(OPERATION_TEST_FLASH_SIZE, NULL, OPERATION_TEST_FLASH_SIZE_TEXT,
		   cmd_test_external_flash_size);
#endif // OPERATION_TEST_FLASH_SIZE

#ifdef OPERATION_TEST_LORA
SHELL_CMD_REGISTER(OPERATION_TEST_LORA, NULL, OPERATION_TEST_LORA_TEXT, cmd_test_lora);
#endif // OPERATION_TEST_LORA

#ifdef OPERATION_TEST_LORA_RX
SHELL_CMD_REGISTER(OPERATION_TEST_LORA_RX, NULL, OPERATION_TEST_LORA_RX_TEXT, cmd_test_lora_rx);
#endif // OPERATION_TEST_LORA_RX

#ifdef OPERATION_TEST_LORA_TX
SHELL_CMD_REGISTER(OPERATION_TEST_LORA_TX, NULL, OPERATION_TEST_LORA_TX_TEXT, cmd_test_lora_tx);
#endif // OPERATION_TEST_LORA_TX

#ifdef OPERATION_TEST_WIFI_LORA
SHELL_CMD_REGISTER(OPERATION_TEST_WIFI_LORA, NULL, OPERATION_TEST_WIFI_LORA_TEXT,
		   cmd_test_lora_wifi_scan);
#endif // OPERATION_TEST_WIFI_LORA

#ifdef OPERATION_TEST_LR_S_BAND_CW
SHELL_CMD_REGISTER(OPERATION_TEST_LR_S_BAND_CW, NULL, OPERATION_TEST_LR_S_BAND_CW_TEXT,
		   cmd_test_lora_s_band_cw);
#endif // OPERATION_TEST_LR_S_BAND_CW

#ifdef OPERATION_TEST_GPS_LORA
SHELL_CMD_REGISTER(OPERATION_TEST_GPS_LORA, NULL, OPERATION_TEST_GPS_LORA_TEXT, cmd_test_lora_gps);
#endif // OPERATION_TEST_GPS_LORA

#ifdef OPERATION_TEST_GPS
SHELL_CMD_REGISTER(OPERATION_TEST_GPS, NULL, OPERATION_TEST_GPS_TEXT, cmd_test_ublox_gps);
#endif // OPERATION_TEST_GPS

#ifdef OPERATION_TEST_FIX_GPS
SHELL_CMD_REGISTER(OPERATION_TEST_FIX_GPS, NULL, OPERATION_TEST_FIX_GPS_TEXT, cmd_test_ublox_fix);
#endif // OPERATION_TEST_FIX_GPS

#ifdef OPERATION_TEST_BT_SCAN
SHELL_CMD_REGISTER(OPERATION_TEST_BT_SCAN, NULL, OPERATION_TEST_BT_SCAN_TEXT, cmd_test_bt_scan);
#endif // OPERATION_TEST_BT_SCAN

#ifdef OPERATION_TEST_GET_FACTORY_NAME
SHELL_CMD_REGISTER(OPERATION_TEST_GET_FACTORY_NAME, NULL, OPERATION_TEST_GET_FACTORY_NAME_TEXT,
		   cmd_get_factory_name);
#endif // OPERATION_TEST_GET_FACTORY_NAME

#ifdef OPERATION_TEST_GET_MAC
SHELL_CMD_REGISTER(OPERATION_TEST_GET_MAC, NULL, OPERATION_TEST_GET_MAC_TEXT, cmd_get_mac);
#endif // OPERATION_TEST_GET_MAC

#ifdef OPERATION_TEST_LED
SHELL_CMD_REGISTER(OPERATION_TEST_LED, NULL, OPERATION_TEST_LED_TEXT, cmd_test_led);
#endif // OPERATION_TEST_LED

#ifdef OPERATION_TEST_LOW_POWER
SHELL_CMD_REGISTER(OPERATION_TEST_LOW_POWER, NULL, OPERATION_TEST_LOW_POWER_TEXT,
		   cmd_test_low_power);
#endif // OPERATION_TEST_LOW_POWER

#ifdef OPERATION_TEST_SETTING
SHELL_CMD_REGISTER(OPERATION_TEST_SETTING, NULL, OPERATION_TEST_SETTING_TEXT,
		   cmd_test_change_setting);
#endif // OPERATION_TEST_SETTING

#ifdef OPERATION_TEST_GET_SETTING
SHELL_CMD_REGISTER(OPERATION_TEST_GET_SETTING, NULL, OPERATION_TEST_GET_SETTING_TEXT,
		   cmd_test_get_setting);
#endif // OPERATION_TEST_GET_SETTING

#ifdef OPERATION_TEST_EXIT
SHELL_CMD_REGISTER(OPERATION_TEST_EXIT, NULL, OPERATION_TEST_EXIT_TEXT, cmd_test_exit);
#endif // OPERATION_TEST_EXIT
