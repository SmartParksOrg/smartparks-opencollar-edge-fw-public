/** @file lp0_communication.c
 *
 * @brief Interface for lp0 communication operations
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2025 Irnas. All rights reserved.
 */

#include <stdlib.h>
#include <led.h>
#include <lorawan_tools.h>
#include <lp0_packetization.h>

#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include <lr11xx_board.h>
#include <lr11xx_lr_fhss.h>
#include <lr11xx_lr_fhss_types.h>
#include <lr11xx_radio.h>
#include <lr11xx_radio_types.h>
#include <lr11xx_regmem.h>
#include <lr11xx_system.h>

#include <lp0_communication.h>

LOG_MODULE_REGISTER(lp0_communication);

#define LR_SENDING_TIMEOUT_MS 5000

/* Output power - range [-17, +22] for sub-G, range [-18, 13] for 2.4G ( HF_PA ) */
#define LP0_FHSS_TX_OUTPUT_POWER_DBM 13
#define LP0_LR_TX_OUTPUT_POWER_DBM   22

#define LP0_FHSS_PA_RAMP_TIME LR11XX_RADIO_RAMP_208_US
#define LP0_LR_PA_RAMP_TIME   LR11XX_RADIO_RAMP_48_US

/* Packet parameters for LoRa packets */
#define LR_LORA_PREAMBLE_LENGTH 8

/* LP0 communication task */
enum lp0_task_type {
	LP0_TASK_IDLE,
	LP0_TASK_RX,
	LP0_TASK_RX_CONTINUOUS,
	LP0_TASK_TX,
};

static atomic_t prv_current_task = ATOMIC_INIT(LP0_TASK_IDLE);

/* Locally kept configuration data */
static struct lp0_payload_data prv_packet = {.frame_counter = 1};
static struct lp0_communication_params *prv_params;
static struct lp0_callbacks *prv_callbacks;

/* Locally kept configuration data pointer */
static struct lp0_cfg *prv_cfg;

/* Callback definitions */
static void prv_on_tx_done(const struct device *dev);
static void prv_on_rx_done(const struct device *dev);
static void prv_on_timeout(const struct device *dev);

/* Callbacks for lora_irq_handler
 * Note: These are not user callbacks. User callbacks are saved in the prv_callbacks structure.
 *
 * This structure will be expanded when other callback types are needed. Currently rx_done, tx_done
 * and timeout are sufficient.
 */
static struct lr11xx_irq_callbacks prv_lr11xx_callbacks = {
	.rx_done = prv_on_rx_done,
	.tx_done = prv_on_tx_done,
	.timeout = prv_on_timeout,
};

/**
 * @brief IRQ handler for TX done
 *
 * @param[in] dev - device pointer to lora chip
 */
static void prv_on_tx_done(const struct device *dev)
{
	lr11xx_irq_thread_stop_processing(dev);
	LOG_DBG("TX done");

	prv_packet.frame_counter++;

	if (prv_callbacks->tx_done == NULL) {
		return;
	}
	atomic_set(&prv_current_task, LP0_TASK_IDLE);
	int err = 0;
	prv_callbacks->tx_done(err);
}

/**
 * @brief IRQ handler for RX done
 *
 * @param[in] dev - device pointer to lora chip
 */
static void prv_on_rx_done(const struct device *dev)
{
	lr11xx_irq_thread_stop_processing(dev);

	uint8_t buffer_rx[255];
	uint8_t buffer_rx_raw[255];
	size_t size = 0;

	lr11xx_radio_rx_buffer_status_t rx_buffer_status;
	lr11xx_radio_pkt_status_lora_t pkt_status_lora;
	lr11xx_radio_pkt_status_gfsk_t pkt_status_gfsk;

	lr11xx_radio_get_rx_buffer_status(dev, &rx_buffer_status);
	lr11xx_regmem_read_buffer8(dev, buffer_rx_raw, rx_buffer_status.buffer_start_pointer,
				   rx_buffer_status.pld_len_in_bytes);
	size = rx_buffer_status.pld_len_in_bytes;

	LOG_HEXDUMP_DBG(buffer_rx_raw, size, "Received packet content: ");

	LOG_INF("Packet status:");
	if (prv_cfg->pkt_type == LR11XX_RADIO_PKT_TYPE_LORA) {
		lr11xx_radio_get_lora_pkt_status(dev, &pkt_status_lora);
		LOG_INF("  - RSSI packet = %i dBm", pkt_status_lora.rssi_pkt_in_dbm);
		LOG_INF("  - Signal RSSI packet = %i dBm", pkt_status_lora.signal_rssi_pkt_in_dbm);
		LOG_INF("  - SNR packet = %i dB", pkt_status_lora.snr_pkt_in_db);
	} else if (prv_cfg->pkt_type == LR11XX_RADIO_PKT_TYPE_GFSK) {
		lr11xx_radio_get_gfsk_pkt_status(dev, &pkt_status_gfsk);
		LOG_INF("  - RSSI average = %i dBm", pkt_status_gfsk.rssi_avg_in_dbm);
		LOG_INF("  - RSSI sync = %i dBm", pkt_status_gfsk.rssi_sync_in_dbm);
	}

	bool lorawan_header = false;

	if (buffer_rx_raw[0] == (LP0_CUSTOM_HEADER_SYNC_BYTES >> 8) &&
	    buffer_rx_raw[1] == (LP0_CUSTOM_HEADER_SYNC_BYTES & 0xFF)) {
		LOG_DBG("LP0 sync word detected");
		lorawan_header = false;
	} else {
		LOG_DBG("LP0 sync word NOT detected, assuming LoRaWAN header");
		lorawan_header = true;
	}

	size_t decoded_len = sizeof(buffer_rx_raw);

	uint8_t port = 0;
	uint32_t frame_counter = 0;

	int err = lp0_depacketize_message(buffer_rx_raw, size, &port, prv_cfg, buffer_rx,
					  &decoded_len, &frame_counter, lorawan_header);

	if (decoded_len > 0) {
		LOG_HEXDUMP_DBG(buffer_rx, decoded_len, "Decoded payload");
	}

	atomic_set(&prv_current_task, LP0_TASK_IDLE);

	/* If a callback is not registered no need to submit work */
	if (prv_callbacks->rx_done == NULL) {
		LOG_DBG("No RX done callback registered");
		return;
	}

	if (prv_cfg->pkt_type == LR11XX_RADIO_PKT_TYPE_LORA) {
		prv_callbacks->rx_done(err, &port, buffer_rx, &decoded_len, buffer_rx_raw, &size,
				       prv_cfg->pkt_type, &pkt_status_lora);
	} else if (prv_cfg->pkt_type == LR11XX_RADIO_PKT_TYPE_GFSK) {
		prv_callbacks->rx_done(err, &port, buffer_rx, &decoded_len, buffer_rx_raw, &size,
				       prv_cfg->pkt_type, &pkt_status_gfsk);
	} else {
		prv_callbacks->rx_done(err, &port, buffer_rx, &decoded_len, buffer_rx_raw, &size,
				       prv_cfg->pkt_type, NULL);
	}
}

/**
 * @brief IRQ handler for RX timeout
 *
 * @param[in] dev - device pointer to lora chip
 */
static void prv_on_timeout(const struct device *dev)
{
	lr11xx_irq_thread_stop_processing(dev);

	LOG_DBG("Timeout IRQ received");

	/* Find out if TX or RX timeout */
	switch (atomic_get(&prv_current_task)) {
	case LP0_TASK_TX:
		atomic_set(&prv_current_task, LP0_TASK_IDLE);
		prv_callbacks->tx_done(-ETIMEDOUT);
		LOG_DBG("TX timeout");
		break;
	case LP0_TASK_RX:
		atomic_set(&prv_current_task, LP0_TASK_IDLE);
		prv_callbacks->rx_done(-ETIMEDOUT, 0, NULL, 0, 0, 0, 0, NULL);
		LOG_DBG("RX timeout");
		break;
	default:
		atomic_set(&prv_current_task, LP0_TASK_IDLE);
		LOG_ERR("TIMEOUT: Unhandled task type");
		break;
	}
}

/**
 * @brief Set modulation and packet parameters for given payload length.
 *
 * @param[in] context - device pointer to LoRa chip
 * @param[in] params - lp0 params
 * @return int 0 if ok, negative error code if error
 */
static int prv_configure_lora_mod_pkt_and_sync_params(const struct device *context,
						      struct lp0_communication_params *params,
						      struct lp0_cfg *cfg)
{
	int ret = lr11xx_radio_set_lora_mod_params(context, &params->lr_standard.mod_params);
	if (ret) {
		LOG_ERR("Failed set lora mod params.");
		return ret;
	}

	ret = lr11xx_radio_set_lora_pkt_params(context, &params->lr_standard.pkt_params);
	if (ret) {
		LOG_ERR("Failed set lora pkt params.");
		return ret;
	}
	ret = lr11xx_radio_set_lora_sync_word(context, cfg->lora_syncword);
	if (ret) {
		LOG_ERR("Failed set lora sync word.");
		return ret;
	}

	return ret;
}

/**
 * @brief Initialize radio without FHSS. (Lora messages without frequency hopping)
 *
 * @param[in] context device pointer to LoRa chip
 * @param[in] freq_hz frequency in Hz
 *
 * @return int 0 if ok, negative error code if error
 */
static int prv_radio_init_without_fhss(const struct device *context, uint32_t freq_hz)
{
	int ret;

	/* Set packet type */
	ret = lr11xx_radio_set_pkt_type(context, LR11XX_RADIO_PKT_TYPE_LORA);
	if (ret) {
		LOG_ERR("Failed pkt pkt type.");
	}

	/* Set frequency */
	ret = lr11xx_radio_set_rf_freq(context, freq_hz);
	if (ret) {
		LOG_ERR("Failed set RF freq.");
		return ret;
	}

	/* Set RSSI calibration */
	ret = lr11xx_radio_set_rssi_calibration(context,
						lr11xx_board_get_rssi_calibration_table(freq_hz));
	if (ret) {
		LOG_ERR("Failed set RSSI calibration.");
		return ret;
	}

	/* Get power config for given settings */
	const lr11xx_board_pa_pwr_cfg_t *pa_pwr_cfg =
		lr11xx_board_get_pa_pwr_cfg(freq_hz, LP0_LR_TX_OUTPUT_POWER_DBM);
	/* LUKATODO:TODO-FUTURE: when s band is transferred to lp0, add option to select power level
	 */
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
	ret = lr11xx_radio_set_tx_params(context, pa_pwr_cfg->power, LP0_LR_PA_RAMP_TIME);
	if (ret) {
		LOG_ERR("Failed set TX params.");
		return ret;
	}

	ret = lr11xx_radio_set_rx_tx_fallback_mode(context, LR11XX_RADIO_FALLBACK_STDBY_RC);
	if (ret) {
		LOG_ERR("Failed set fallback mode.");
	}

	ret = lr11xx_radio_cfg_rx_boosted(context, true);
	if (ret) {
		LOG_ERR("Failed set rx boosted.");
	}

	return 0;
}

/**
 * @brief Implement radio FHSS init.
 *
 * @param[in] context device pointer to LoRa chip
 * @param[in] freq_hz frequency in Hz
 *
 * @return int 0 if ok, negative error code if error
 */
static int prv_radio_init_with_fhss(const struct device *context, uint32_t freq_hz)
{
	int ret;

	/* Init FHSS mode - set packet type */
	ret = lr11xx_lr_fhss_init(context);
	if (ret) {
		LOG_ERR("Failed to init FHSS mode. err: %d", ret);
		return ret;
	}

	/* Set frequency */
	ret = lr11xx_radio_set_rf_freq(context, freq_hz);
	if (ret) {
		LOG_ERR("Failed set RF freq.");
		return ret;
	}

	/* Set RSSI calibration */
	ret = lr11xx_radio_set_rssi_calibration(context,
						lr11xx_board_get_rssi_calibration_table(freq_hz));
	if (ret) {
		LOG_ERR("Failed set RSSI calibration.");
		return ret;
	}

	/* Get power config for given settings */
	const lr11xx_board_pa_pwr_cfg_t *pa_pwr_cfg =
		lr11xx_board_get_pa_pwr_cfg(freq_hz, LP0_FHSS_TX_OUTPUT_POWER_DBM);
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
	ret = lr11xx_radio_set_tx_params(context, pa_pwr_cfg->power, LP0_FHSS_PA_RAMP_TIME);
	if (ret) {
		LOG_ERR("Failed set TX params.");
		return ret;
	}

	return 0;
}

int lp0_lr11xx_system_init(const struct device *context)
{
	int ret = 0;

	const struct lr11xx_hal_context_cfg_t *config = context->config;

	LOG_DBG("Reset lr11xx chip");
	ret = lr11xx_system_reset((void *)context);
	if (ret) {
		LOG_ERR("System reset failed.");
	}

	// Configure the regulator
	const lr11xx_system_reg_mode_t regulator = config->reg_mode;
	ret = lr11xx_system_set_reg_mode((void *)context, regulator);
	if (ret) {
		LOG_ERR("Failed to config regulator.");
	}

	// Configure RF switch
	const lr11xx_system_rfswitch_cfg_t rf_switch_setup = config->rf_switch_cfg;
	ret = lr11xx_system_set_dio_as_rf_switch(context, &rf_switch_setup);
	if (ret) {
		LOG_ERR("Failed to config rf switch.");
	}

	// Configure the 32MHz TCXO if it is available on the board
	const struct lr11xx_hal_context_tcxo_cfg_t tcxo_cfg = config->tcxo_cfg;
	if (tcxo_cfg.has_tcxo == true) {
		const uint32_t timeout_rtc_step =
			lr11xx_radio_convert_time_in_ms_to_rtc_step(tcxo_cfg.timeout_ms);
		ret = lr11xx_system_set_tcxo_mode(context, tcxo_cfg.supply, timeout_rtc_step);
		if (ret) {
			LOG_ERR("Failed to configure TCXO.");
		}
	}

	// Configure the Low Frequency Clock
	const struct lr11xx_hal_context_lf_clck_cfg_t lf_clk_cfg = config->lf_clck_cfg;
	ret = lr11xx_system_cfg_lfclk(context, lf_clk_cfg.lf_clk_cfg, lf_clk_cfg.wait_32k_ready);
	if (ret) {
		LOG_ERR("Failed to configure Configure the Low Frequency Clock.");
	}

	ret = lr11xx_system_clear_errors(context);
	if (ret) {
		LOG_ERR("Failed to clear errors.");
	}

	ret = lr11xx_system_calibrate(context, 0x3F);
	if (ret) {
		LOG_ERR("Failed to calibrate.");
	}

	uint16_t errors;
	ret = lr11xx_system_get_errors(context, &errors);
	if (ret) {
		LOG_ERR("Failed to get errors.");
	}

	ret = lr11xx_system_clear_errors(context);
	if (ret) {
		LOG_ERR("Failed to clear errors.");
	}

	ret = lr11xx_system_clear_irq_status(context, LR11XX_SYSTEM_IRQ_ALL_MASK);
	if (ret) {
		LOG_ERR("Failed to clear irq status.");
	}

	return ret;
}

int lp0_init(const struct device *context, struct lp0_cfg *cfg, struct lp0_callbacks *callbacks)
{
	ARG_UNUSED(context);

	if (cfg == NULL || callbacks == NULL) {
		LOG_ERR("Invalid NULL pointer provided for configuration or callbacks.");
		return -EINVAL;
	}

	/* Set params */
	prv_cfg = cfg;
	prv_callbacks = callbacks;

	return 0;
}

int lp0_update_callbacks(struct lp0_callbacks *callbacks)
{
	if (callbacks == NULL) {
		LOG_ERR("Invalid NULL pointer provided for callbacks.");
		return -EINVAL;
	}

	/* Set params */
	prv_callbacks = callbacks;

	return 0;
}

int lp0_configure(const struct device *context, bool fhss, uint32_t freq_hz,
		  struct lp0_communication_params *params)
{
	int ret = 0;
	if (context == NULL || params == NULL) {
		LOG_ERR("Invalid NULL pointer provided for context or params.");
		return -EINVAL;
	}

	/* Packet params */
	prv_params = params;

	if (fhss) {
		/* FHSS init */
		ret = prv_radio_init_with_fhss(context, freq_hz);
		if (ret) {
			LOG_ERR("Failed to init FHSS mode.");
			return ret;
		}

	} else {
		/* LR init */
		ret = prv_radio_init_without_fhss(context, freq_hz);
		if (ret) {
			LOG_ERR("Failed to init LR mode.");
			return ret;
		}
	}
	return ret;
}

int lp0_send_message(const struct device *context, uint8_t *payload, uint8_t len, uint8_t port,
		     bool fhss, uint32_t freq_hz, bool confirmed, bool lorawan_header,
		     lr11xx_radio_lora_iq_t iq_setting)
{
	int ret = 0;

	if (atomic_get(&prv_current_task) != LP0_TASK_IDLE) {
		return -EBUSY;
	}

	atomic_set(&prv_current_task, LP0_TASK_TX);

	/* If device is sending standard, it can only be an uplink, if the device is sending
	 * inverted it can only be a downlink */
	enum lora_link_direction direction = iq_setting == LR11XX_RADIO_LORA_IQ_STANDARD
						     ? LORA_DIRECTION_UPLINK
						     : LORA_DIRECTION_DOWNLINK;

	/* Packetize payload */
	ret = lp0_packetize_message(payload, len, port, fhss, &prv_packet, prv_params, prv_cfg,
				    confirmed, lorawan_header, direction);
	if (ret) {
		LOG_ERR("Failed to packetize message.");
		goto report;
	}

	/* Attach IRQ */
	lr11xx_irq_thread_start_processing(context, &prv_lr11xx_callbacks, prv_cfg->irq_mask);

	/* Update LoRa packet parameters */
	if (!fhss) {
		prv_params->lr_standard.pkt_params.iq = iq_setting;
		prv_params->lr_standard.pkt_params.pld_len_in_bytes = prv_packet.buffer_len;
		ret = prv_configure_lora_mod_pkt_and_sync_params(context, prv_params, prv_cfg);
		if (ret) {
			LOG_ERR("Failed to set lora packet params.");
		}
	}

	/* Build frame / Write to buffer */
	if (fhss) {
		/* Get hopping sequence */
		const uint16_t hop_sequence_id = rand() % lr11xx_lr_fhss_get_hop_sequence_count(
								  &prv_params->lr_fhss.fhss_params);

		/* Write buffer */
		LOG_DBG("Write TX buffer %d seq id, hopping: %d", hop_sequence_id,
			prv_params->lr_fhss.fhss_params.lr_fhss_params.enable_hopping);
		ret = lr11xx_lr_fhss_build_frame(context, &prv_params->lr_fhss.fhss_params,
						 hop_sequence_id, prv_packet.buffer_tx,
						 prv_packet.buffer_len);
		if (ret) {
			LOG_ERR("Failed to build frame.");
			goto report;
		}
	} else {
		LOG_HEXDUMP_DBG(prv_packet.buffer_tx, prv_packet.buffer_len, "Write buffer");
		/* Write buffer */
		ret = lr11xx_regmem_write_buffer8(context, prv_packet.buffer_tx,
						  prv_packet.buffer_len);
		if (ret) {
			LOG_ERR("Failed to build frame.");
			goto report;
		}
	}

	LOG_HEXDUMP_DBG(payload, len, "Sending payload:");

	/* Set in TX mode */
	// LOG_INF("Set TX mode");
	LOG_INF("Set TX mode: %d", freq_hz);
	ret = lr11xx_radio_set_tx(context, LR_SENDING_TIMEOUT_MS);
	if (ret) {
		LOG_ERR("Failed to set TX mode.");
		goto report;
	}

report:
	return ret;
}

int lp0_start_message_receive(const struct device *context, uint32_t freq_hz,
			      lr11xx_radio_lora_iq_t iq_setting, uint32_t timeout_ms)
{
	int ret = 0;
	led_turn_on(LED_C);

	if (atomic_get(&prv_current_task) != LP0_TASK_IDLE) {
		return -EBUSY;
	}

	if (timeout_ms == 0xFFFFFF) {
		atomic_set(&prv_current_task, LP0_TASK_RX_CONTINUOUS);
	} else {
		atomic_set(&prv_current_task, LP0_TASK_RX);
	}

	/* Attach IRQ */
	lr11xx_irq_thread_start_processing(context, &prv_lr11xx_callbacks, prv_cfg->irq_mask);

	prv_params->lr_standard.pkt_params.iq = iq_setting;
	prv_params->lr_standard.pkt_params.pld_len_in_bytes = 0; /* Must be 0, otherwise RX fails */

	ret = prv_configure_lora_mod_pkt_and_sync_params(context, prv_params, prv_cfg);
	if (ret) {
		LOG_ERR("Failed to configure lora mod and pkt params.");
		return ret;
	}

	if (lr11xx_radio_set_rf_freq(context, freq_hz)) {
		LOG_ERR("Failed to set RF frequency.");
		return -EBUSY;
	}
	LOG_INF("Set RX mode: %d", freq_hz);
	// LOG_INF("Set RX mode");

	if (timeout_ms == 0xFFFFFF) {
		ret = lr11xx_radio_set_rx_with_timeout_in_rtc_step(context, timeout_ms);
	} else {
		ret = lr11xx_radio_set_rx(context, timeout_ms);
	}

	if (ret) {
		LOG_ERR("Failed to set RX mode (err: %d)", ret);
	}

	k_sleep(K_MSEC(1));
	return ret;
}

int lp0_stop_continuous_message_receive(const struct device *context)
{
	lr11xx_irq_thread_stop_processing(context);

	lr11xx_status_t status = lr11xx_system_set_standby(context, LR11XX_SYSTEM_STANDBY_CFG_RC);
	if (status != LR11XX_STATUS_OK) {
		LOG_ERR("Failed to set standby: %d", status);
		return -EIO;
	}

	atomic_set(&prv_current_task, LP0_TASK_IDLE);

	return 0;
}
