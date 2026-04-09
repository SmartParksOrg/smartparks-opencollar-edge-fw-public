/**
 * @file main.c
 * @brief Basic LR11xx TX example
 *
 * @copyright Copyright (c) 2023
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/types.h>

#include "apps_common.h"
#include "lorawan_tools.h"
#include "lr11xx_board.h"
#include "lr11xx_radio.h"
#include "lr11xx_regmem.h"
#include "lr11xx_system.h"
#include "lr11xx_system_types.h"

#ifdef CONFIG_RF_FRONT_END_MODULE
#include <rf_front_end_module.h>
#endif

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main);

#define GPIO_DT_SPEC_IS_EMPTY(spec) ((spec).port == NULL)

/**
 * @brief LR11xx interrupt mask used by the application
 */
#define IRQ_MASK                                                                                   \
	(LR11XX_SYSTEM_IRQ_TX_DONE | LR11XX_SYSTEM_IRQ_TIMEOUT | LR11XX_SYSTEM_IRQ_HEADER_ERROR |  \
	 LR11XX_SYSTEM_IRQ_CRC_ERROR | LR11XX_SYSTEM_IRQ_FSK_LEN_ERROR)

/* Radio parameters */
#define LR_PACKET_TYPE          LR11XX_RADIO_PKT_TYPE_LORA
#define LR_RF_FREQ_IN_HZ        868000000U
/* range [-17, +22] for sub-G, range [-18, 13] for 2.4G ( HF_PA ) */
#define LR_TX_OUTPUT_POWER_DBM  14
#define LR_PA_RAMP_TIME         LR11XX_RADIO_RAMP_48_US
#define LR_FALLBACK_MODE        LR11XX_RADIO_FALLBACK_STDBY_RC
#define LR_ENABLE_RX_BOOST_MODE false

/* Max buffer length */
#define MAX_PAYLOAD_LENGTH 255

/* Modulation parameters for LoRa packets */
#define LR_LORA_SPREADING_FACTOR LR11XX_RADIO_LORA_SF9
#define LR_LORA_BANDWIDTH        LR11XX_RADIO_LORA_BW_125
#define LR_LORA_CODING_RATE      LR11XX_RADIO_LORA_CR_4_5

/* Packet parameters for LoRa packets */
#define LR_LORA_PREAMBLE_LENGTH 8
#define LR_LORA_PKT_LEN_MODE    LR11XX_RADIO_LORA_PKT_EXPLICIT
#define LR_LORA_IQ              LR11XX_RADIO_LORA_IQ_STANDARD
#define LR_LORA_CRC             LR11XX_RADIO_LORA_CRC_ON

/* LoRa sync word */
#define LR_LORA_SYNCWORD 0x34 // 0x12 Private Network, 0x34 Public Network

/* lr11xx context */
const struct device *context = DEVICE_DT_GET_ONE(irnas_lr11xx);

/* For now use hardcoded values - sband-test-1 */

uint8_t app_key[16] = {0xEC, 0x7F, 0x38, 0x0E, 0x7A, 0xDF, 0xB2, 0xE5,
		       0xC9, 0xBB, 0xDE, 0x5A, 0xC2, 0x16, 0x94, 0xA8};
uint8_t network_key[16] = {0xDB, 0x7E, 0x27, 0x26, 0xCF, 0xF5, 0x8C, 0x13,
			   0x3A, 0x07, 0xB5, 0xA1, 0xB4, 0x00, 0xE1, 0xD0};
uint8_t dev_addr[4] = {0x26, 0x0B, 0xF6, 0xEF};

/* For now use hardcoded values - sband-test-2 */
/*
uint8_t app_key[16] = {0xEA, 0x56, 0x07, 0xB0, 0xF0, 0xF2, 0x38, 0x86,
		       0x5C, 0xED, 0xC5, 0xE9, 0xA0, 0x23, 0x8D, 0x76};
uint8_t network_key[16] = {0x13, 0x55, 0xD3, 0x36, 0xD1, 0x67, 0xF3, 0xC5,
			   0xCB, 0xB3, 0x2B, 0xDA, 0x5C, 0x68, 0x13, 0x07};
uint8_t dev_addr[4] = {0x26, 0x0B, 0x06, 0x29};
*/

/* Payload to send */
#define LR_PAYLOAD_LENGTH 4
uint8_t payload[LR_PAYLOAD_LENGTH] = {0x01, 0x02, 0x03, 0x04};

/* Encrypted message buffer */
static uint8_t buffer_tx[MAX_PAYLOAD_LENGTH];

/* Send done flag */
static bool send_done = false;

static void prv_radio_init(const void *context)
{
	int ret;

	/* Set packet type */
	ret = lr11xx_radio_set_pkt_type(context, LR_PACKET_TYPE);
	if (ret) {
		LOG_ERR("Failed pkt pkt type.");
	}

	/* Set frequency */
	ret = lr11xx_radio_set_rf_freq(context, LR_RF_FREQ_IN_HZ);
	if (ret) {
		LOG_ERR("Failed set RF freq.");
	}

	/* Set RSSI calibration */
	ret = lr11xx_radio_set_rssi_calibration(
		context, lr11xx_board_get_rssi_calibration_table(LR_RF_FREQ_IN_HZ));
	if (ret) {
		LOG_ERR("Failed set RSSI calibration.");
	}

	/* Get power config for given settings */
	const lr11xx_board_pa_pwr_cfg_t *pa_pwr_cfg =
		lr11xx_board_get_pa_pwr_cfg(LR_RF_FREQ_IN_HZ, LR_TX_OUTPUT_POWER_DBM);
	if (pa_pwr_cfg == NULL) {
		LOG_ERR("Invalid target frequency or power level");
		while (true) {
		}
	}

	/* Set power config */
	ret = lr11xx_radio_set_pa_cfg(context, &(pa_pwr_cfg->pa_config));
	if (ret) {
		LOG_ERR("Failed set PA config.");
	}

	/* Set TX params */
	ret = lr11xx_radio_set_tx_params(context, pa_pwr_cfg->power, LR_PA_RAMP_TIME);
	if (ret) {
		LOG_ERR("Failed set RSSI calibration.");
	}

	ret = lr11xx_radio_set_rx_tx_fallback_mode(context, LR_FALLBACK_MODE);
	if (ret) {
		LOG_ERR("Failed set fallback mode.");
	}

	ret = lr11xx_radio_cfg_rx_boosted(context, LR_ENABLE_RX_BOOST_MODE);
	if (ret) {
		LOG_ERR("Failed set rx boosted.");
	}
}

static int prv_set_packet_prams(uint8_t payload_len)
{
	int ret;

	const lr11xx_radio_mod_params_lora_t lora_mod_params = {
		.sf = LR_LORA_SPREADING_FACTOR,
		.bw = LR_LORA_BANDWIDTH,
		.cr = LR_LORA_CODING_RATE,
		.ldro = apps_common_compute_lora_ldro(LR_LORA_SPREADING_FACTOR, LR_LORA_BANDWIDTH),
	};

	const lr11xx_radio_pkt_params_lora_t lora_pkt_params = {
		.preamble_len_in_symb = LR_LORA_PREAMBLE_LENGTH,
		.header_type = LR_LORA_PKT_LEN_MODE,
		.pld_len_in_bytes = payload_len,
		.crc = LR_LORA_CRC,
		.iq = LR_LORA_IQ,
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

#ifdef CONFIG_RF_FRONT_END_MODULE
/**
 * @brief Configure RF Front End Module.
 *
 * This function configures the RF Front End Module (FEM) based on the provided frequency.
 */
static void prv_configure_rf_front_end_module(uint32_t rf_freq_in_hz)
{
	/* Configure gpios */
	rf_front_end_module_init();

	/* Set rf front end module mode */
	if (rf_freq_in_hz >= 1000000000U) {
		rf_front_end_module_set_mode(RF_FRONT_END_MODE_TX);
		// rf_front_end_module_set_ant_path(RF_FRONT_END_ANTENNA_PATH_2);
	}
}
#endif /* CONFIG_RF_FRONT_END_MODULE */

/**
 * @brief Main application entry point.
 */
int main(void)
{
	LOG_INF("===== LR11xx TX example =====");

	k_sleep(K_SECONDS(5));

	/* INIT */

	apps_common_lr11xx_system_init(context);

	apps_common_lr11xx_fetch_and_print_version(context);

	prv_radio_init(context);

#ifdef CONFIG_RF_FRONT_END_MODULE
	/* RF Front End Module */
	prv_configure_rf_front_end_module(LR_RF_FREQ_IN_HZ);
#endif /* CONFIG_RF_FRONT_END_MODULE */

	int ret = 0;

	/* CW test */
	ret = lr11xx_radio_set_tx_cw(context);
	if (ret) {
		LOG_ERR("Failed to set TX continuous wave.");
	}

	while (1) {
		k_sleep(K_MSEC(1));
	}
	/* End CW test */

	LOG_INF("Set dio irq mask");
	ret = lr11xx_system_set_dio_irq_params(context, IRQ_MASK, 0);
	if (ret) {
		LOG_ERR("Failed to set dio irq params.");
	}

	LOG_INF("Clear irq status");
	ret = lr11xx_system_clear_irq_status(context, LR11XX_SYSTEM_IRQ_ALL_MASK);
	if (ret) {
		LOG_ERR("Failed to set dio irq params.");
	}

	apps_common_lr11xx_enable_irq(context);

	uint32_t frame_counter = 0;

	struct lorawan_auth_info auth_info;
	memcpy(auth_info.AppSKey, app_key, sizeof(auth_info.AppSKey));
	memcpy(auth_info.NwkSKey, network_key, sizeof(auth_info.NwkSKey));
	memcpy(auth_info.DevAddr, dev_addr, sizeof(auth_info.DevAddr));

	while (true) {
		/* Increment, reset variables */
		frame_counter++;
		send_done = false;
		size_t buffer_len = sizeof(buffer_tx);

		memcpy(payload, &frame_counter, sizeof(frame_counter));

		/* SETUP TX BUFFER */
		ret = lorawan_tools_build(payload, LR_PAYLOAD_LENGTH, 4, &auth_info, frame_counter,
					  buffer_tx, &buffer_len, false);
		if (ret) {
			LOG_ERR("Failed to build lorawan package.");
		}

		LOG_INF("Generated buffer of length %d", buffer_len);
		for (uint8_t i = 0; i < buffer_len; i++) {
			printk("%02X ", buffer_tx[i]);
		}

		ret = prv_set_packet_prams(buffer_len);
		if (ret) {
			LOG_ERR("Failed to set packet params.");
		}

		/* SEND */

		/* Write buffer */
		LOG_INF("Write TX buffer");
		ret = lr11xx_regmem_write_buffer8(context, buffer_tx, buffer_len);
		if (ret) {
			LOG_ERR("Failed to write buffer.");
		}

		/* Set in TX mode */
		LOG_INF("Set TX mode");
		ret = lr11xx_radio_set_tx(context, 0);
		if (ret) {
			LOG_ERR("Failed to set TX mode.");
		}

		/* Wait for send to be done */
		while (!send_done) {
			apps_common_lr11xx_irq_process(context, IRQ_MASK);
			k_sleep(K_MSEC(1));
		}

		LOG_INF("Sleeping for 10 seconds");
		k_sleep(K_SECONDS(10));
	}
}

void on_tx_done(void)
{
	LOG_INF("TX done");
	send_done = true;
}
