/**
 * @file main.c
 * @brief Basic LR11xx TX FHSS example
 *
 * @copyright Copyright (c) 2023
 *
 */

#include "transmit_rule.h"
#include <rf_front_end_module.h>

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
#include "lr11xx_lr_fhss.h"
#include "lr11xx_radio.h"
#include "lr11xx_regmem.h"
#include "lr11xx_system.h"
#include "lr11xx_system_types.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main);

BUILD_ASSERT(CONFIG_RF_FRONT_END_MODULE, "CONFIG_RF_FRONT_END_MODULE is not defined. The RF front "
					 "end module is required for this sample");

/**
 * @brief LR11xx interrupt mask used by the application
 */
#define IRQ_MASK (LR11XX_SYSTEM_IRQ_TX_DONE)

/* Radio parameters */
#define FHSS_RF_FREQ_IN_HZ       2008450000
/* range [-17, +22] for sub-G, range [-18, 13] for 2.4G ( HF_PA ) */
#define FHSS_TX_OUTPUT_POWER_DBM 6
#define FHSS_PA_RAMP_TIME        LR11XX_RADIO_RAMP_208_US

/* Max buffer length */
#define MAX_PAYLOAD_LENGTH 255

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
/* For now use hardcoded values - sband-test-3 */
/*
uint8_t app_key[16] = {0x9C, 0x4B, 0x4D, 0x51, 0x63, 0xAA, 0x3D, 0xB9,
		       0x4E, 0xC8, 0x94, 0xD9, 0x0D, 0xF5, 0xB0, 0x0B};
uint8_t network_key[16] = {0x4F, 0xF4, 0x2D, 0xDB, 0x69, 0x2F, 0x3D, 0x25,
			   0xCB, 0xFC, 0x4D, 0xE8, 0xFA, 0xAA, 0x4D, 0xA4};
uint8_t dev_addr[4] = {0x26, 0x0B, 0xB6, 0xDE};
*/
/* lr11xx context */
const struct device *context = DEVICE_DT_GET(DT_NODELABEL(lr11xx));

/* FHSS parameters */
static const uint8_t sync_word[] = {0x2c, 0x0f, 0x79, 0x95};

const lr11xx_lr_fhss_params_t params = {
	.lr_fhss_params = {.sync_word = sync_word,
			   .modulation_type = LR_FHSS_V1_MODULATION_TYPE_GMSK_488,
			   .cr = LR_FHSS_V1_CR_1_3,
			   .grid = LR_FHSS_V1_GRID_3906_HZ,
			   .bw = LR_FHSS_V1_BW_136719_HZ,
			   .enable_hopping = true,
			   .header_count = 3},
	.device_offset = 0};

/* Payload to send */
#define LR_PAYLOAD_LENGTH 4
uint8_t payload[LR_PAYLOAD_LENGTH] = {0x01, 0x02, 0x03, 0x04};

/* Message buffer */
static uint8_t buffer_tx[MAX_PAYLOAD_LENGTH];

/* Send done flag */
static bool send_done = false;

static size_t prv_lrfhss_max_payload(uint8_t nb_header, lr_fhss_v1_cr_t cr)
{
	size_t retval = 0;

	static const uint8_t max_nb_header = 4;
	static const uint8_t min_nb_header = 1;
	static const uint32_t data_block_bit_size = 50;
	static const uint32_t data_fragment_bit_size = 48;
	static const uint32_t header_block_size = 114;
	static const uint32_t max_phy_bit_size = (255 * 8);

	if ((nb_header >= min_nb_header) && (nb_header <= max_nb_header)) {

		uint32_t header_bit_size = header_block_size * nb_header;

		uint32_t max_data_bit_size = max_phy_bit_size - (header_bit_size + 6);

		uint32_t max_data_blocks = max_data_bit_size / data_block_bit_size;

		uint32_t max_encoded_payload_bits = max_data_blocks * data_fragment_bit_size;

		uint32_t max_payload_bytes;
		uint32_t max_payload_bits;

		switch (cr) {
		case LR_FHSS_V1_CR_5_6:
			max_payload_bits = max_encoded_payload_bits * 5 / 6;
			break;
		case LR_FHSS_V1_CR_2_3:
			max_payload_bits = max_encoded_payload_bits * 2 / 3;
			break;
		case LR_FHSS_V1_CR_1_2:
			max_payload_bits = max_encoded_payload_bits / 2;
			break;
		case LR_FHSS_V1_CR_1_3:
		default:
			max_payload_bits = max_encoded_payload_bits / 3;
			break;
		}

		max_payload_bytes = max_payload_bits / 8;

		// a crc is encoded with the payload
		if (max_payload_bytes >= 2) {

			max_payload_bytes -= 2;
		} else {

			max_payload_bytes = 0;
		}

		retval = max_payload_bytes;
	}

	return retval;
}

static void prv_radio_init(const void *context, const lr11xx_lr_fhss_params_t *params)
{
	int ret;

	/* Init FHSS mode - set packet type */
	ret = lr11xx_lr_fhss_init(context);
	if (ret) {
		LOG_ERR("Failed to init FHSS mode.");
		return;
	}

	/* Set frequency */
	ret = lr11xx_radio_set_rf_freq(context, FHSS_RF_FREQ_IN_HZ);
	if (ret) {
		LOG_ERR("Failed set RF freq.");
	}

	/* Set RSSI calibration */
	ret = lr11xx_radio_set_rssi_calibration(
		context, lr11xx_board_get_rssi_calibration_table(FHSS_RF_FREQ_IN_HZ));
	if (ret) {
		LOG_ERR("Failed set RSSI calibration.");
	}

	/* Get power config for given settings */
	const lr11xx_board_pa_pwr_cfg_t *pa_pwr_cfg =
		lr11xx_board_get_pa_pwr_cfg(FHSS_RF_FREQ_IN_HZ, FHSS_TX_OUTPUT_POWER_DBM);
	if (pa_pwr_cfg == NULL) {
		LOG_ERR("Invalid target frequency or power level");
		while (true) {
		}
	}

	LOG_INF("PA config:  %d %d %d %d %d", pa_pwr_cfg->power,
		pa_pwr_cfg->pa_config.pa_duty_cycle, pa_pwr_cfg->pa_config.pa_reg_supply,
		pa_pwr_cfg->pa_config.pa_hp_sel, pa_pwr_cfg->pa_config.pa_sel);

	/* Set power config */
	ret = lr11xx_radio_set_pa_cfg(context, &(pa_pwr_cfg->pa_config));
	if (ret) {
		LOG_ERR("Failed set PA config.");
	}

	/* Set TX params */
	ret = lr11xx_radio_set_tx_params(context, pa_pwr_cfg->power, FHSS_PA_RAMP_TIME);
	if (ret) {
		LOG_ERR("Failed set TX params.");
	}
}

/**
 * @brief Main application entry point.
 */
int main(void)
{
	k_sleep(K_MSEC(1000));
	LOG_INF("===== LR11xx satellite TX example =====");
	k_sleep(K_MSEC(1000));

	/* INIT */
	apps_common_lr11xx_system_init(context);

	apps_common_lr11xx_fetch_and_print_version(context);

	prv_radio_init(context, &params);

	int ret = 0;

	/* Init Front-End module */
	ret = rf_front_end_module_init();
	if (ret) {
		LOG_ERR("Failed to init front-end rf module.");
	}

	/* Set TX mode */
	ret = rf_front_end_module_set_mode(RF_FRONT_END_MODE_TX);
	if (ret) {
		LOG_ERR("Failed to set front-end rf module to TX mode.");
	}

	/* CW test */
	/*
	ret = lr11xx_radio_set_tx_cw(context);
	if (ret) {
		LOG_ERR("Failed to set TX continuous wave.");
	}

	while (1) {
		k_sleep(K_MSEC(1));
	}
	*/
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

		/* Check message size - you will get max back */
		ret = prv_lrfhss_max_payload(params.lr_fhss_params.header_count,
					     params.lr_fhss_params.cr);
		if (ret < buffer_len) {
			LOG_ERR("Message too long.");
		}

		/* SEND */

		/* Get hopping sequence */
		const uint16_t hop_sequence_id =
			rand() % lr11xx_lr_fhss_get_hop_sequence_count(&params);

		/* Write buffer */
		LOG_INF("Write TX buffer");
		ret = lr11xx_lr_fhss_build_frame(context, &params, hop_sequence_id, buffer_tx,
						 buffer_len);
		if (ret) {
			LOG_ERR("Failed to build frame.");
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
