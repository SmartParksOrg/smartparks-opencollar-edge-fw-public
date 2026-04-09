/**
 * @file wifi_scan.c
 * @brief
 *
 * @copyright (c) 2023 Irnas. All rights reserved.
 *
 */

#include "settings_def.h"
#include "wifi_scan.h"
#include "wifi_scan_data.h"
#include "wifi_scan_printers.h"

#include "lr11xx_board.h"
#include "lr11xx_system.h"
#include "lr11xx_wifi.h"

#include <zephyr/kernel.h>
#include <zephyr/types.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(wifi_scan, CONFIG_LR_WIFI_SCAN_LOG_LEVEL);

/**
 * @brief LR11xx interrupt mask used by the application
 */
#define IRQ_MASK            (LR11XX_SYSTEM_IRQ_WIFI_SCAN_DONE)
#define WIFI_SCAN_TIMEOUT_S 10
#define CHANNEL_ALL_MASK    (0x3FFF)

#define WIFI_SCAN_DEFAULT_SCAN_MODE           CONFIG_LR_WIFI_SCAN_MODE
#define WIFI_SCAN_DEFAULT_RESULT_FORMAT       CONFIG_LR_WIFI_SCAN_RESULT_FORMAT
#define WIFI_SCAN_DEFAULT_SIGNAL_TYPE         CONFIG_LR_WIFI_SCAN_SIGNAL_TYPE
#define WIFI_SCAN_DEFAULT_CHANNEL_MASK        CONFIG_LR_WIFI_SCAN_CHANNEL_MASK
#define WIFI_SCAN_DEFAULT_MAX_RESULT          CONFIG_LR_WIFI_SCAN_MAX_RESULTS
#define WIFI_SCAN_DEFAULT_NB_SCAN_PER_CHANNEL CONFIG_LR_WIFI_SCAN_NB_SCAN_PER_CHANNEL
#define WIFI_SCAN_DEFAULT_TIMEOUT_PER_SCAN    CONFIG_LR_WIFI_SCAN_TIMEOUT_PER_SCAN_MS
#define WIFI_SCAN_DEFAULT_ABORT_ON_TIMEOUT    CONFIG_LR_WIFI_SCAN_ABORT_ON_TIMEOUT
#define WIFI_SCAN_DEFAULT_NUMBER_OF_SCANS     1

struct wifi_scan_configuration {
	/* Scan mode options:
	LR11XX_WIFI_SCAN_MODE_BEACON = 1
	LR11XX_WIFI_SCAN_MODE_BEACON_AND_PKT = 2
	LR11XX_WIFI_SCAN_MODE_FULL_BEACON = 4
	LR11XX_WIFI_SCAN_MODE_UNTIL_SSID = 5
	*/
	lr11xx_wifi_mode_t scan_mode;
	/* The possible value depends on the value provided in scan_mode and can be:
	LR11XX_WIFI_RESULT_FORMAT_BASIC_COMPLETE or LR11XX_WIFI_RESULT_FORMAT_BASIC_MAC_TYPE_CHANNEL
	if scan mode is:
		- LR11XX_WIFI_SCAN_MODE_BEACON or
		- LR11XX_WIFI_SCAN_MODE_BEACON_AND_PKT
	LR11XX_WIFI_RESULT_FORMAT_EXTENDED_FULL if scan mode is:
		- LR11XX_WIFI_SCAN_MODE_FULL_BEACON or
		- LR11XX_WIFI_SCAN_MODE_UNTIL_SSID
	*/
	lr11xx_wifi_result_format_t result_format;
	/* Type of signal:
	LR11XX_WIFI_TYPE_SCAN_B = 0x01
	LR11XX_WIFI_TYPE_SCAN_G = 0x02
	LR11XX_WIFI_TYPE_SCAN_N = 0x03
	LR11XX_WIFI_TYPE_SCAN_B_G_N = 0x04
	*/
	lr11xx_wifi_signal_type_scan_t signal_type;
	/* Channel mask */
	lr11xx_wifi_channel_mask_t channel_mask;
	/* Maximum number of results */
	uint8_t max_result;
	/* Number of scans per channel */
	uint8_t nb_scan_per_channel;
	/* Timeout per scan */
	uint16_t timeout_per_scan;
	/* Indicate if scan on one channel should stop on the first preamble search timeout detected
	 */
	bool abort_on_timeout;
	/* Number of scans to perform */
	uint8_t number_of_scans;
};

static struct wifi_scan_configuration prv_cfg = {
	.scan_mode = WIFI_SCAN_DEFAULT_SCAN_MODE,
	.result_format = WIFI_SCAN_DEFAULT_RESULT_FORMAT,
	.signal_type = WIFI_SCAN_DEFAULT_SIGNAL_TYPE,
	.channel_mask = WIFI_SCAN_DEFAULT_CHANNEL_MASK,
	.max_result = WIFI_SCAN_DEFAULT_MAX_RESULT,
	.nb_scan_per_channel = WIFI_SCAN_DEFAULT_NB_SCAN_PER_CHANNEL,
	.timeout_per_scan = WIFI_SCAN_DEFAULT_TIMEOUT_PER_SCAN,
	.abort_on_timeout = WIFI_SCAN_DEFAULT_ABORT_ON_TIMEOUT,
	.number_of_scans = WIFI_SCAN_DEFAULT_NUMBER_OF_SCANS,
};

struct wifi_scan_data {

	uint8_t nb_results;

	lr11xx_wifi_result_format_t result_format;

	lr11xx_wifi_basic_complete_result_t basic_complete_results[LR11XX_WIFI_MAX_RESULTS];
	lr11xx_wifi_basic_mac_type_channel_result_t
		basic_mac_type_channel_results[LR11XX_WIFI_MAX_RESULTS];
	lr11xx_wifi_extended_full_result_t extended_full_results[LR11XX_WIFI_MAX_RESULTS];
};

static struct wifi_scan_data prv_data;

/* External results handler */
wifi_scan_results_handler_t prv_wifi_scan_results_handler = NULL;

static volatile bool irq_fired = false;
static volatile bool scan_done = false;

/**
 * @brief Reset results data structure.
 *
 */
static void prv_wifi_scan_reset_data(void)
{
	prv_data.nb_results = 0;
	memset(prv_data.basic_complete_results, 0, sizeof(prv_data.basic_complete_results));
	memset(prv_data.basic_mac_type_channel_results, 0,
	       sizeof(prv_data.basic_mac_type_channel_results));
	memset(prv_data.extended_full_results, 0, sizeof(prv_data.extended_full_results));
}

/**
 * @brief Read and print cumulative timings of the last scan.
 *
 * @param[in] context
 * @return int 0 or negative error code
 */
static int prv_wifi_read_timings(const void *context)
{
	int ret = 0;

	lr11xx_wifi_cumulative_timings_t cumulative_timings = {0};
	ret = lr11xx_wifi_read_cumulative_timing(context, &cumulative_timings);
	if (ret) {
		LOG_ERR("Failed to read cumulative timing.");
		return ret;
	}

	LOG_DBG("Cumulative timings:");
	LOG_DBG("  -> Demodulation: %u us", cumulative_timings.demodulation_us);
	LOG_DBG("  -> Capture: %u us", cumulative_timings.rx_capture_us);
	LOG_DBG("  -> Correlation: %u us", cumulative_timings.rx_correlation_us);
	LOG_DBG("  -> Detection: %u us", cumulative_timings.rx_detection_us);
	LOG_DBG("  => Total : %u us\n",
		cumulative_timings.demodulation_us + cumulative_timings.rx_capture_us +
			cumulative_timings.rx_correlation_us + cumulative_timings.rx_detection_us);

	return ret;
}

static int prv_wifi_scan_get_results(const void *context)
{
	/* Reset data structure to hold data */
	prv_wifi_scan_reset_data();

	int ret = prv_wifi_read_timings(context);
	if (ret) {
		LOG_ERR("Failed to read timings");
		return ret;
	}

	prv_data.result_format = prv_cfg.result_format;

	ret = lr11xx_wifi_get_nb_results(context, &prv_data.nb_results);
	if (ret) {
		LOG_ERR("Failed to obtain number of results.");
		return ret;
	}

	switch (prv_data.result_format) {
	case LR11XX_WIFI_RESULT_FORMAT_BASIC_COMPLETE: {
		ret = lr11xx_wifi_read_basic_complete_results(context, 0, prv_data.nb_results,
							      prv_data.basic_complete_results);
		if (!ret) {
			wifi_scan_print_basic_complete_results(prv_data.basic_complete_results,
							       prv_data.nb_results);
		}
		break;
	}
	case LR11XX_WIFI_RESULT_FORMAT_BASIC_MAC_TYPE_CHANNEL: {
		ret = lr11xx_wifi_read_basic_mac_type_channel_results(
			context, 0, prv_data.nb_results, prv_data.basic_mac_type_channel_results);
		if (!ret) {
			wifi_scan_print_basic_basic_mac_type_channel_results(
				prv_data.basic_mac_type_channel_results, prv_data.nb_results);
		}
		break;
	}
	case LR11XX_WIFI_RESULT_FORMAT_EXTENDED_FULL: {
		ret = lr11xx_wifi_read_extended_full_results(context, 0, prv_data.nb_results,
							     prv_data.extended_full_results);
		if (!ret) {
			wifi_scan_print_extended_full_results(prv_data.extended_full_results,
							      prv_data.nb_results);
		}
		break;
	}
	}

	if (ret) {
		LOG_ERR("Failed to read results!");
	}

	return ret;
}

/**
 * @brief IRQ callback for radio dio irq
 *
 * @param[in] dev
 */
static void prv_radio_on_dio_irq(const struct device *dev)
{
	irq_fired = true;
	LOG_DBG("Irq fired");
}

/**
 * @brief Attach and enable interrupt for wifi scan done event
 *
 * @param[in] context
 */
static void prv_wifi_scan_enable_irq(const void *context)
{
	lr11xx_board_attach_interrupt(context, prv_radio_on_dio_irq);
	lr11xx_board_enable_interrupt(context);
}

static void prv_on_wifi_scan_done(void)
{
	LOG_INF("WiFi scan done");
	scan_done = true;
}

static void prv_wifi_scan_irq_process(const void *context, lr11xx_system_irq_mask_t irq_filter_mask)
{
	if (irq_fired) {
		irq_fired = false;

		lr11xx_system_irq_mask_t irq_regs;
		lr11xx_system_get_and_clear_irq_status(context, &irq_regs);

		LOG_DBG("Interrupt flags = 0x%08X", irq_regs);

		irq_regs &= irq_filter_mask;

		LOG_DBG("Interrupt flags (after filtering) = 0x%08X", irq_regs);

		if ((irq_regs & LR11XX_SYSTEM_IRQ_WIFI_SCAN_DONE) ==
		    LR11XX_SYSTEM_IRQ_WIFI_SCAN_DONE) {
			LOG_INF("WiFi scan done");
			prv_on_wifi_scan_done();
		}
	}
}

/**
 * @brief Reset config struct to default values
 *
 */
static void prv_wifi_scan_reset_cfg(void)
{
	prv_cfg.scan_mode = WIFI_SCAN_DEFAULT_SCAN_MODE;
	prv_cfg.result_format = WIFI_SCAN_DEFAULT_RESULT_FORMAT;
	prv_cfg.signal_type = WIFI_SCAN_DEFAULT_SIGNAL_TYPE;
	prv_cfg.channel_mask = WIFI_SCAN_DEFAULT_CHANNEL_MASK;
	prv_cfg.max_result = WIFI_SCAN_DEFAULT_MAX_RESULT;
	prv_cfg.nb_scan_per_channel = WIFI_SCAN_DEFAULT_NB_SCAN_PER_CHANNEL;
	prv_cfg.timeout_per_scan = WIFI_SCAN_DEFAULT_TIMEOUT_PER_SCAN;
	prv_cfg.abort_on_timeout = WIFI_SCAN_DEFAULT_ABORT_ON_TIMEOUT;
	prv_cfg.number_of_scans = WIFI_SCAN_DEFAULT_NUMBER_OF_SCANS;
}

static int prv_wifi_scan_setup(const void *context)
{
	LOG_INF("Set dio irq mask");
	int ret = lr11xx_system_set_dio_irq_params(context, IRQ_MASK, 0);
	if (ret) {
		LOG_ERR("Failed to set dio irq params.");
		return ret;
	}

	LOG_INF("Clear irq status");
	ret = lr11xx_system_clear_irq_status(context, LR11XX_SYSTEM_IRQ_ALL_MASK);
	if (ret) {
		LOG_ERR("Failed to set dio irq params.");
		return ret;
	}

	irq_fired = false;
	scan_done = false;

	/* Attach new callback for wifi scan */
	prv_wifi_scan_enable_irq(context);

	return 0;
}

static int prv_wifi_scan(const void *context)
{
	int ret = lr11xx_wifi_reset_cumulative_timing(context);
	if (ret) {
		LOG_ERR("Failed to reset cumulative timing.");
		return ret;
	}

	ret = lr11xx_wifi_scan(context, prv_cfg.signal_type, prv_cfg.channel_mask,
			       prv_cfg.scan_mode, prv_cfg.max_result, prv_cfg.nb_scan_per_channel,
			       prv_cfg.timeout_per_scan, prv_cfg.abort_on_timeout);
	if (ret) {
		LOG_ERR("WiFi scan fail!");
	}

	return ret;
}

void wifi_scan_default(const void *context)
{
	/* Reset config struct */
	prv_wifi_scan_reset_cfg();

	/* WiFi scan IRQ setup */
	LOG_INF("Setup wifi scan irq");
	int ret = prv_wifi_scan_setup(context);
	if (ret) {
		LOG_ERR("lr11xx setup for wifi scan failed");
		goto exit;
	}

	/* Reset config to default values */

	/* Check that scan mode is compatible with result format */
	if (!lr11xx_wifi_are_scan_mode_result_format_compatible(prv_cfg.scan_mode,
								prv_cfg.result_format)) {
		LOG_ERR("Scan mode and result format are not compatible");
		goto exit;
	}

	/* Perform desired number of scans */
	uint8_t scan_counter = 0;

	while (scan_counter < prv_cfg.number_of_scans) {

		ret = prv_wifi_scan(context);
		if (ret) {
			LOG_ERR("WiFi scan failed");
			goto exit;
		}

		int64_t start = k_uptime_get();

		k_sleep(K_SECONDS(2));

		while (!scan_done && k_uptime_get() - start < WIFI_SCAN_TIMEOUT_S * MSEC_PER_SEC) {
			prv_wifi_scan_irq_process(context, IRQ_MASK);
			k_sleep(K_MSEC(10));
		}

		if (!scan_done) {
			ret = -ETIMEDOUT;
			goto exit;
		}

		prv_wifi_scan_get_results(context);

		scan_counter++;
	}
exit:
	prv_wifi_scan_results_handler(ret);
}

void wifi_scan_store_results(uint32_t timestamp)
{
	if (!Main_settings.wifi_scan_report_zero_connections_found->def_val &&
	    prv_data.nb_results == 0) {
		LOG_WRN("No wifi connections found. Skipping report. (Zero connection reports are "
			"turned off.)");
		return;
	}
	switch (prv_data.result_format) {
	case LR11XX_WIFI_RESULT_FORMAT_BASIC_COMPLETE: {
		wifi_scan_data_add_basic_complete_results(prv_data.basic_complete_results,
							  prv_data.nb_results, timestamp);
		break;
	}
	case LR11XX_WIFI_RESULT_FORMAT_BASIC_MAC_TYPE_CHANNEL: {
		wifi_scan_data_add_basic_basic_mac_type_channel_results(
			prv_data.basic_mac_type_channel_results, prv_data.nb_results, timestamp);
		break;
	}
	case LR11XX_WIFI_RESULT_FORMAT_EXTENDED_FULL: {
		wifi_scan_data_add_extended_full_results(prv_data.extended_full_results,
							 prv_data.nb_results, timestamp);
		break;
	}
	}
}

void wifi_scan_results_handler_register(wifi_scan_results_handler_t handler)
{
	prv_wifi_scan_results_handler = handler;
}
