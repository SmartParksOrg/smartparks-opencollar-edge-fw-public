/**
 * @file lr_s_band.c
 * @brief
 *
 * @copyright (c) 2023 Irnas. All rights reserved.
 *
 */

#include "lr_s_band.h"
#include <lorawan_tools.h>
#include <rf_front_end_module.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/types.h>

#include "generated_settings.h"
#include "lr11xx_board.h"
#include "lr11xx_lr_fhss.h"
#include "lr11xx_radio.h"
#include "lr11xx_regmem.h"
#include "lr11xx_system.h"
#include "lr11xx_system_types.h"
#include "uart_pm.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(lr_s_band);

/* IRQ mask used by the application */
#define IRQ_MASK                 (LR11XX_SYSTEM_IRQ_TX_DONE)
/* Output power - range [-17, +22] for sub-G, range [-18, 13] for 2.4G ( HF_PA ) */
#define FHSS_TX_OUTPUT_POWER_DBM 6
#define LR_TX_OUTPUT_POWER_DBM   6

#define FHSS_PA_RAMP_TIME LR11XX_RADIO_RAMP_208_US
#define LR_PA_RAMP_TIME   LR11XX_RADIO_RAMP_48_US

/* Max buffer length */
#define MAX_PAYLOAD_LENGTH 255

/* LoRa sync word */
#define LR_LORA_SYNCWORD 0x12 // 0x12 Private Network, 0x34 Public Network

/* Packet parameters for LoRa packets */
#define LR_LORA_PREAMBLE_LENGTH 8

/* FHSS parameters */
static const uint8_t sync_word[] = {0x2c, 0x0f, 0x79, 0x95};

static const lr11xx_lr_fhss_params_t prv_params = {
	.lr_fhss_params = {.sync_word = sync_word,
			   .modulation_type = LR_FHSS_V1_MODULATION_TYPE_GMSK_488,
			   .cr = LR_FHSS_V1_CR_1_3,
			   .grid = LR_FHSS_V1_GRID_3906_HZ,
			   .bw = LR_FHSS_V1_BW_136719_HZ,
			   .enable_hopping = true,
			   .header_count = 3},
	.device_offset = 0};

/* Payload data */

struct payload_data {
	uint8_t buffer_tx[MAX_PAYLOAD_LENGTH];
	size_t buffer_len;

	uint32_t frame_counter;

	uint8_t network_key[16];
	uint8_t app_key[16];
	uint8_t dev_addr[4];
};

static struct payload_data prv_data = {.frame_counter = 1,
				       .buffer_len = sizeof(prv_data.buffer_tx)};

/* External TX done handler */
lr_s_band_tx_done_handler_t prv_lr_s_band_tx_done_handler = NULL;

static volatile bool irq_fired = false;
static volatile bool tx_done = false;

/**
 * @brief Reset TX buffer.
 *
 */
static void prv_payload_data_reset(void)
{
	memset(prv_data.buffer_tx, 0, MAX_PAYLOAD_LENGTH);
	prv_data.buffer_len = sizeof(prv_data.buffer_tx);
}

/* IRQ callbacks */

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

static void prv_on_tx_done(void)
{
	LOG_INF("TX done");
	tx_done = true;
	prv_data.frame_counter++;
}

/**
 * @brief Attach and enable interrupt for TX done event
 *
 * @param[in] context
 */
static void prv_enable_irq(const void *context)
{
	lr11xx_board_attach_interrupt(context, prv_radio_on_dio_irq);
	lr11xx_board_enable_interrupt(context);
}

/**
 * @brief Process events on IRQ fired.
 *
 * @param[in] context
 * @param[in] irq_filter_mask
 */
static void prv_irq_process(const void *context, lr11xx_system_irq_mask_t irq_filter_mask)
{
	if (irq_fired) {
		irq_fired = false;

		lr11xx_system_irq_mask_t irq_regs;
		lr11xx_system_get_and_clear_irq_status(context, &irq_regs);

		LOG_DBG("Interrupt flags = 0x%08X", irq_regs);

		irq_regs &= irq_filter_mask;

		LOG_DBG("Interrupt flags (after filtering) = 0x%08X", irq_regs);

		if ((irq_regs & LR11XX_SYSTEM_IRQ_TX_DONE) == LR11XX_SYSTEM_IRQ_TX_DONE) {
			LOG_INF("TX done");
			prv_on_tx_done();
		}
	}
}

/**
 * @brief IRQ setup
 *
 * @param[in] context
 * @return int
 */
static int prv_irq_setup(const void *context)
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
	tx_done = false;

	/* Attach new callbacks */
	prv_enable_irq(context);

	return 0;
}

/**
 * @brief Implement radio FHSS init.
 *
 * @param[in] context
 * @param[in] params lr11xx_lr_fhss_params_t params
 * @return int
 */
static int prv_radio_init_without_fhss(const void *context, const lr11xx_lr_fhss_params_t *params)
{
	int ret;

	/* Set packet type */
	ret = lr11xx_radio_set_pkt_type(context, LR11XX_RADIO_PKT_TYPE_LORA);
	if (ret) {
		LOG_ERR("Failed pkt pkt type.");
	}

	/* Set frequency */
	ret = lr11xx_radio_set_rf_freq(context, Main_settings.s_band_rf_frequency_hz->def_val);
	if (ret) {
		LOG_ERR("Failed set RF freq.");
		return ret;
	}

	/* Set RSSI calibration */
	ret = lr11xx_radio_set_rssi_calibration(
		context, lr11xx_board_get_rssi_calibration_table(
				 Main_settings.s_band_rf_frequency_hz->def_val));
	if (ret) {
		LOG_ERR("Failed set RSSI calibration.");
		return ret;
	}

	/* Get power config for given settings */
	const lr11xx_board_pa_pwr_cfg_t *pa_pwr_cfg = lr11xx_board_get_pa_pwr_cfg(
		Main_settings.s_band_rf_frequency_hz->def_val, LR_TX_OUTPUT_POWER_DBM);
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
	ret = lr11xx_radio_set_tx_params(context, pa_pwr_cfg->power, LR_PA_RAMP_TIME);
	if (ret) {
		LOG_ERR("Failed set TX params.");
		return ret;
	}

	return 0;
}

/**
 * @brief Implement radio FHSS init.
 *
 * @param[in] context
 * @param[in] params lr11xx_lr_fhss_params_t params
 * @return int
 */
static int prv_radio_init_with_fhss(const void *context, const lr11xx_lr_fhss_params_t *params)
{
	int ret;

	/* Init FHSS mode - set packet type */
	ret = lr11xx_lr_fhss_init(context);
	if (ret) {
		LOG_ERR("Failed to init FHSS mode.");
		return ret;
	}

	/* Set frequency */
	ret = lr11xx_radio_set_rf_freq(context, Main_settings.s_band_rf_frequency_hz->def_val);
	if (ret) {
		LOG_ERR("Failed set RF freq.");
		return ret;
	}

	/* Set RSSI calibration */
	ret = lr11xx_radio_set_rssi_calibration(
		context, lr11xx_board_get_rssi_calibration_table(
				 Main_settings.s_band_rf_frequency_hz->def_val));
	if (ret) {
		LOG_ERR("Failed set RSSI calibration.");
		return ret;
	}

	/* Get power config for given settings */
	const lr11xx_board_pa_pwr_cfg_t *pa_pwr_cfg = lr11xx_board_get_pa_pwr_cfg(
		Main_settings.s_band_rf_frequency_hz->def_val, FHSS_TX_OUTPUT_POWER_DBM);
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
	ret = lr11xx_radio_set_tx_params(context, pa_pwr_cfg->power, FHSS_PA_RAMP_TIME);
	if (ret) {
		LOG_ERR("Failed set TX params.");
		return ret;
	}

	return 0;
}

static int prv_set_packet_params(const void *context, uint8_t payload_len)
{
	int ret;

	const lr11xx_radio_mod_params_lora_t lora_mod_params = {
		.sf = LR11XX_RADIO_LORA_SF7,
		.bw = LR11XX_RADIO_LORA_BW_125,
		.cr = LR11XX_RADIO_LORA_CR_4_5,
		.ldro = 0,
	};

	const lr11xx_radio_pkt_params_lora_t lora_pkt_params = {
		.preamble_len_in_symb = LR_LORA_PREAMBLE_LENGTH,
		.header_type = LR11XX_RADIO_LORA_PKT_EXPLICIT,
		.pld_len_in_bytes = payload_len,
		.crc = LR11XX_RADIO_LORA_CRC_ON,

		/* Must be set to STANDARD on both ends when performing an UPLINK */
		.iq = LR11XX_RADIO_LORA_IQ_STANDARD,
	};

	ret = lr11xx_radio_set_lora_mod_params(context, &lora_mod_params);
	if (ret) {
		LOG_ERR("Failed set lora mod params.");
	}
	ret = lr11xx_radio_set_lora_pkt_params(context, &lora_pkt_params);
	if (ret) {
		LOG_ERR("Failed set lora pkt params.");
	}
	ret = lr11xx_radio_set_lora_sync_word(context, LR_LORA_SYNCWORD);
	if (ret) {
		LOG_ERR("Failed set lora sync word.");
	}

	return ret;
}

int lr_s_band_send_message_fhss(const void *context, uint8_t *payload, uint8_t len, uint8_t port)
{
	int ret = 0;

	/* FHSS init */
	ret = prv_radio_init_with_fhss(context, &prv_params);
	if (ret) {
		LOG_ERR("Failed to init FHSS mode.");
		goto report;
	}

	/* IRQ setup */
	ret = prv_irq_setup(context);
	if (ret) {
		LOG_ERR("Failed to setup IRQ.");
		goto report;
	}

	/* Reset TX buffer */
	prv_payload_data_reset();

	struct lorawan_auth_info lora_auth_info;
	memcpy(lora_auth_info.NwkSKey, prv_data.network_key, sizeof(lora_auth_info.NwkSKey));
	memcpy(lora_auth_info.AppSKey, prv_data.app_key, sizeof(lora_auth_info.AppSKey));
	memcpy(lora_auth_info.DevAddr, prv_data.dev_addr, sizeof(lora_auth_info.DevAddr));

	/* Build lorawan compatible package */
	ret = lorawan_tools_build(payload, len, port, &lora_auth_info, prv_data.frame_counter,
				  prv_data.buffer_tx, &prv_data.buffer_len, false,
				  LORA_DIRECTION_UPLINK);
	if (ret) {
		LOG_ERR("Failed to build lorawan package.");
		goto report;
	}

#ifdef CONFIG_RF_FRONT_END_MODULE
/*
The front_end_ctx pin overlaps with the UART TX pin (uart0) on boards which use the rf front end
module and serial-uart. For these instances we do not know the current state of the pin so we
re-initialize it to OUTPUT for the front end module to work properly. This is only needed for the
UART_TX/rf_front_end_ctx pin. This is only needed on the production version, as the serial-uart
module is disabled to decrease power consumption.
*/
#if !defined(CONFIG_DEBUG_MODE) && !defined(CONFIG_PROVISIONING_MODE)
#if DT_NODE_HAS_STATUS(DT_ALIAS(serial_uart), okay)
	ret = rf_front_end_module_init();
	if (ret) {
		LOG_ERR("Failed to init RF front end module.");
		goto report;
	}
#endif /* DT_NODE_HAS_STATUS(DT_ALIAS(serial_uart), okay) */
#endif /* !CONFIG_DEBUG_MODE && !CONFIG_PROVISIONING_MODE */

	ret = rf_front_end_module_set_mode(RF_FRONT_END_MODE_TX);
	if (ret) {
		LOG_ERR("Failed to set RF front end module to TX mode.");
		goto report;
	}
#endif /* CONFIG_RF_FRONT_END_MODULE */

	/* Get hopping sequence */
	const uint16_t hop_sequence_id =
		rand() % lr11xx_lr_fhss_get_hop_sequence_count(&prv_params);

	/* Write buffer */
	LOG_INF("Write TX buffer");
	ret = lr11xx_lr_fhss_build_frame(context, &prv_params, hop_sequence_id, prv_data.buffer_tx,
					 prv_data.buffer_len);
	if (ret) {
		LOG_ERR("Failed to build frame.");
		goto report;
	}

	/* Set in TX mode */
	LOG_INF("Set TX mode");
	ret = lr11xx_radio_set_tx(context, 0);
	if (ret) {
		LOG_ERR("Failed to set TX mode.");
		goto report;
	}

	/* Wait for send to be done */
	int64_t start = k_uptime_get();

	while (!tx_done && k_uptime_get() - start < LR_S_BAND_TX_TIMEOUT_S * MSEC_PER_SEC) {
		prv_irq_process(context, IRQ_MASK);
		k_sleep(K_MSEC(10));
	}

	if (!tx_done) {
		ret = -ETIMEDOUT;
		goto report;
	}

report:
	/* Call handler */
	if (prv_lr_s_band_tx_done_handler) {
		prv_lr_s_band_tx_done_handler(ret);
	}

	/* Set RF front-end module to sleep */
#ifdef CONFIG_RF_FRONT_END_MODULE
	ret = rf_front_end_module_set_mode(RF_FRONT_END_MODE_SLEEP);
	if (ret) {
		LOG_ERR("Failed to set RF front end module to sleep mode.");
	}
#endif /* CONFIG_RF_FRONT_END_MODULE */

	return ret;
}

void lr_s_band_tx_done_handler_register(lr_s_band_tx_done_handler_t handler)
{
	prv_lr_s_band_tx_done_handler = handler;
}

void lr_s_band_set_keys(uint8_t network_key[16], uint8_t app_key[16], uint8_t dev_addr[4])
{
	memcpy(prv_data.network_key, network_key, sizeof(prv_data.network_key));
	memcpy(prv_data.app_key, app_key, sizeof(prv_data.app_key));
	memcpy(prv_data.dev_addr, dev_addr, sizeof(prv_data.dev_addr));
}

int lr_s_band_send_message_lr(const void *context, uint8_t *payload, uint8_t len, uint8_t port)
{
	int ret = 0;

	/* FHSS init */
	ret = prv_radio_init_without_fhss(context, &prv_params);
	if (ret) {
		LOG_ERR("Failed to init FHSS mode.");
		goto report;
	}

	/* IRQ setup */
	ret = prv_irq_setup(context);
	if (ret) {
		LOG_ERR("Failed to setup IRQ.");
		goto report;
	}

	/* Reset TX buffer */
	prv_payload_data_reset();

	struct lorawan_auth_info lora_auth_info;
	memcpy(lora_auth_info.NwkSKey, prv_data.network_key, sizeof(lora_auth_info.NwkSKey));
	memcpy(lora_auth_info.AppSKey, prv_data.app_key, sizeof(lora_auth_info.AppSKey));
	memcpy(lora_auth_info.DevAddr, prv_data.dev_addr, sizeof(lora_auth_info.DevAddr));

	/* Build lorawan compatible package */
	ret = lorawan_tools_build(payload, len, port, &lora_auth_info, prv_data.frame_counter,
				  prv_data.buffer_tx, &prv_data.buffer_len, false,
				  LORA_DIRECTION_UPLINK);
	if (ret) {
		LOG_ERR("Failed to build lorawan package.");
		goto report;
	}

	ret = prv_set_packet_params(context, prv_data.buffer_len);
	if (ret) {
		LOG_ERR("Failed to set packet params.");
		goto report;
	}

#ifdef CONFIG_RF_FRONT_END_MODULE
/*
The front_end_ctx pin overlaps with the UART TX pin (uart0) on boards which use the rf front end
module and serial-uart. For these instances we do not know the current state of the pin so we
re-initialize it to OUTPUT for the front end module to work properly. This is only needed for the
UART_TX/rf_front_end_ctx pin. This is only needed on the production version, as the serial-uart
module is disabled to decrease power consumption.
*/
#if !defined(CONFIG_DEBUG_MODE) && !defined(CONFIG_PROVISIONING_MODE)
#if DT_NODE_HAS_STATUS(DT_ALIAS(serial_uart), okay)
	ret = rf_front_end_module_init();
	if (ret) {
		LOG_ERR("Failed to init RF front end module.");
		goto report;
	}
#endif /* DT_NODE_HAS_STATUS(DT_ALIAS(serial_uart), okay) */
#endif /* !CONFIG_DEBUG_MODE && !CONFIG_PROVISIONING_MODE */

	ret = rf_front_end_module_set_mode(RF_FRONT_END_MODE_TX);
	if (ret) {
		LOG_ERR("Failed to set RF front end module to TX mode.");
		goto report;
	}
#endif /* CONFIG_RF_FRONT_END_MODULE */

	/* Write buffer */
	ret = lr11xx_regmem_write_buffer8(context, prv_data.buffer_tx, prv_data.buffer_len);
	if (ret) {
		LOG_ERR("Failed to build frame.");
		goto report;
	}

	/* Set in TX mode */
	LOG_INF("Set TX mode");
	ret = lr11xx_radio_set_tx(context, 0);
	if (ret) {
		LOG_ERR("Failed to set TX mode.");
		goto report;
	}

	/* Wait for send to be done */
	int64_t start = k_uptime_get();

	while (!tx_done && k_uptime_get() - start < LR_S_BAND_TX_TIMEOUT_S * MSEC_PER_SEC) {
		prv_irq_process(context, IRQ_MASK);
		k_sleep(K_MSEC(10));
	}

	if (!tx_done) {
		ret = -ETIMEDOUT;
		goto report;
	}

report:
	/* Call handler */
	if (prv_lr_s_band_tx_done_handler) {
		prv_lr_s_band_tx_done_handler(ret);
	}

	/* Set RF front-end module to sleep */
#ifdef CONFIG_RF_FRONT_END_MODULE
	ret = rf_front_end_module_set_mode(RF_FRONT_END_MODE_SLEEP);
	if (ret) {
		LOG_ERR("Failed to set RF front end module to sleep mode.");
	}
#endif /* CONFIG_RF_FRONT_END_MODULE */

	return ret;
}
