/** @file vhf.c
 *
 * @brief VHF - very high frequency burst transmission functionality.
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2024 Irnas. All rights reserved.
 */

#include <vhf.h>

#include <zephyr/logging/log.h>

/* Include hal and ralf so that initialization can be done */
#include <ralf_lr11xx.h>
#include <smtc_modem_hal.h>

/* LoRaWAN */
#include "lorawan_tools.h"
#include "lr11xx_board.h"
#include "lr11xx_lr_fhss.h"
#include "lr11xx_radio.h"
#include "lr11xx_regmem.h"
#include "lr11xx_system.h"
#include "lr11xx_system_types.h"

/* Other */
#include "generated_settings.h"
#include "global_time.h"
#include "led.h"
#include "thread_watchdog.h"

LOG_MODULE_REGISTER(VHF);

/* Minimum VHF pulse period */
#define VHF_BURST_INTERVAL_MIN_SECONDS 1

/* IRQ mask used by the application */
#define IRQ_MASK                   (LR11XX_SYSTEM_IRQ_TX_DONE)
/* VHF output power (selected by antenna path) => LP (external VHF antenna) / HP (on-board LoRaWAN
 * antenna). Path selection is controlled by `vhf_external_path` user setting. Default: HP */
#define VHF_TX_OUTPUT_POWER_DBM_LP 15 /* [-17 15] */
#define VHF_TX_OUTPUT_POWER_DBM_HP 22 /* [15 22] */
/* VHF PA ramp time - 48 us Ramp Time (Default) */
#define VHF_PA_RAMP_TIME           LR11XX_RADIO_RAMP_48_US
/* VHF fallback mode - Standby RC (Default) */
#define VHF_FALLBACK_MODE          LR11XX_RADIO_FALLBACK_STDBY_RC

uint8_t vhf_get_current_time_interval(void)
{
	uint8_t interval = 1;
	/* Check if switching intervals is active */
	if (!Main_settings.vhf_multiple_intervals->def_val) {
		return 1;
	}
	// Get latest unix time
	uint8_t hours = (get_global_unix_time() / 3600) % 24;

	/* NOTE: If both intervals are set to the same hour, interval 1 is selected */
	if (hours == Main_settings.vhf_interval2_start->def_val) {
		interval = 2;
	}
	if (hours == Main_settings.vhf_interval1_start->def_val) {
		interval = 1;
		return interval;
	}

	/* Check if we are outside the two intervals (circadianly - in the same day). */
	if ((hours < Main_settings.vhf_interval1_start->def_val &&
	     hours < Main_settings.vhf_interval2_start->def_val) ||
	    (hours > Main_settings.vhf_interval1_start->def_val &&
	     hours > Main_settings.vhf_interval2_start->def_val)) {
		/* Select subsequent interval */
		if (Main_settings.vhf_interval1_start->def_val >
		    Main_settings.vhf_interval2_start->def_val) {
			interval = 1;
		} else {
			interval = 2;
		}
		return interval;
	}

	/* Check if we are in-between the two intervals (circadianly - in the same day) */
	if (hours > Main_settings.vhf_interval1_start->def_val &&
	    hours < Main_settings.vhf_interval2_start->def_val) {
		interval = 1;
	}
	if (hours < Main_settings.vhf_interval1_start->def_val &&
	    hours > Main_settings.vhf_interval2_start->def_val) {
		interval = 2;
	}
	LOG_DBG("Current hour: %d, we are in interval: %d", hours, interval);

	return interval;
}

int vhf_configure(const void *context)
{
	int ret = 0;

	uint32_t VHF_RF_FREQ_IN_HZ = Main_settings.vhf_tx_frequency_khz->def_val * 1000;

	/* Set packet type */
	ret = lr11xx_radio_set_pkt_type(context, LR11XX_RADIO_PKT_TYPE_LORA);
	if (ret) {
		LOG_ERR("Failed pkt pkt type.");
		return ret;
	}

	/* Set frequency */
	ret = lr11xx_radio_set_rf_freq(context, VHF_RF_FREQ_IN_HZ);
	if (ret) {
		LOG_ERR("Failed set RF freq.");
		return ret;
	}

	/* Get power config for given settings - RhinoEdge does not have a LP connector  */
#if defined(CONFIG_BOARD_RHINOPUCK35_NRF52840) || defined(CONFIG_BOARD_RHINOPUCK_NRF52840) ||      \
	defined(CONFIG_BOARD_RHINOEDGE_NRF52840)
	const lr11xx_board_pa_pwr_cfg_t *pa_pwr_cfg = lr11xx_board_get_pa_pwr_cfg(
		Main_settings.vhf_tx_frequency_khz->def_val * 1000, VHF_TX_OUTPUT_POWER_DBM_HP);
#else
	const lr11xx_board_pa_pwr_cfg_t *pa_pwr_cfg;
	if (Main_settings.vhf_external_path->def_val) {
		pa_pwr_cfg = lr11xx_board_get_pa_pwr_cfg(
			Main_settings.vhf_tx_frequency_khz->def_val * 1000,
			VHF_TX_OUTPUT_POWER_DBM_LP);
	} else {
		pa_pwr_cfg = lr11xx_board_get_pa_pwr_cfg(
			Main_settings.vhf_tx_frequency_khz->def_val * 1000,
			VHF_TX_OUTPUT_POWER_DBM_HP);
	}
#endif
	if (pa_pwr_cfg == NULL) {
		LOG_ERR("Invalid target frequency or power level");
		return ret;
	}

	/* Set power config */
	ret = lr11xx_radio_set_pa_cfg(context, &(pa_pwr_cfg->pa_config));
	if (ret) {
		LOG_ERR("Failed set PA config.");
		return ret;
	}

	/* Set TX params */
	ret = lr11xx_radio_set_tx_params(context, pa_pwr_cfg->power, VHF_PA_RAMP_TIME);
	if (ret) {
		LOG_ERR("Failed set TX params.");
		return ret;
	}

	return ret;
}

void vhf_send_burst()
{
	int ret = 0;
	const ralf_t modem_radio = RALF_LR11XX_INSTANTIATE(DEVICE_DT_GET(DT_NODELABEL(lr11xx)));

	ret = vhf_configure(modem_radio.ral.context);
	if (ret) {
		LOG_ERR("Failed to configure VHF: %d", ret);
		return;
	}

	/* LR - TX Continuous Wave */
	ret = lr11xx_radio_set_tx_cw(modem_radio.ral.context);
	if (ret) {
		LOG_ERR("Failed to set TX continuous wave.");
		return;
	}
	k_sleep(K_MSEC(Main_settings.vhf_single_pulse_duration_ms->def_val));
}

int vhf_get_interval_min_seconds(void)
{
	return VHF_BURST_INTERVAL_MIN_SECONDS;
}
