#include <apps_common.h>
#include <lorawan_tools.h>
#include <lp0_communication.h>
#include <lp0_packetization.h>
#include <lr11xx_system.h>
#ifdef CONFIG_RF_FRONT_END_MODULE
#include <rf_front_end_module.h>
#endif /* CONFIG_RF_FRONT_END_MODULE */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(lp0_send);

/**
 * @brief LR11xx interrupt mask used by the application
 */
#define IRQ_MASK                                                                                   \
	(LR11XX_SYSTEM_IRQ_TX_DONE | LR11XX_SYSTEM_IRQ_RX_DONE | LR11XX_SYSTEM_IRQ_TIMEOUT |       \
	 LR11XX_SYSTEM_IRQ_HEADER_ERROR | LR11XX_SYSTEM_IRQ_CRC_ERROR |                            \
	 LR11XX_SYSTEM_IRQ_FSK_LEN_ERROR)

/* lr11xx radio context and its use in the ralf layer */
static const struct device *prv_context = DEVICE_DT_GET(DT_NODELABEL(lr11xx));

#define LR_SENDING_PERIOD_MS 20000

/* Packet parameters for LoRa packets */
#define LR_LORA_PREAMBLE_LENGTH 8

/* Radio parameters */
#define FREQ_IN_HZ_RX     868100000
#define FREQ_IN_HZ_TX     868100000
#define LP0_RX_TIMEOUT_MS 15000

/* Max buffer length */
#define MAX_PAYLOAD_LENGTH 255
#define LR_LORA_SYNCWORD   0x34 // 0x12 Private Network, 0x34 Public Network

static uint8_t prv_payload[MAX_PAYLOAD_LENGTH];
static uint8_t prv_payload_size = 50;
static uint8_t prv_port = 33;
static bool prv_fhss = false;

/* To view Lora traffic, register your device as ABP to TTN, (like for s-band) set the frame counter
 * to 16 bit and check the "Resets frame counter" to avoid issues. You can also disable ADR. You can
 * use the recommended frequency plans or use the single frequency setting (if using a single
 * frequency for tx, configure the FREQ_IN_HZ_TX definition accordingly). Set the Rx1 delay to 2
 * seconds and set the RX frequency to the above configured (FREQ_IN_HZ_RX) */

/* Lora Keys */
static uint8_t prv_network_key[16] = {0xEE, 0x28, 0x8F, 0x48, 0x2F, 0xCE, 0xD0, 0x9F,
				      0x32, 0x69, 0x2F, 0x19, 0x5C, 0xBF, 0x0E, 0x83};
static uint8_t prv_app_key[16] = {0xF7, 0xFD, 0x1E, 0xB6, 0xE3, 0xAD, 0x01, 0x0C,
				  0x0F, 0xB2, 0xDB, 0x52, 0x0B, 0x49, 0x9F, 0xA6};
static uint8_t prv_dev_addr[4] = {0x26, 0x0B, 0x09, 0x81};

/* Synchronization word for FHSS - Selected by LoRa Alliance for compliance */
static const uint8_t prv_sync_word[] = {0x2c, 0x0f, 0x79, 0x95};

/**
 * @brief RX done handler
 *
 * @param err - error code
 * @param port - pointer to port number
 * @param payload - pointer to received payload
 * @param len - pointer to length of received payload
 * @param raw_payload - pointer to raw received payload
 * @param raw_len - pointer to length of raw received payload
 * @param pkt_type - packet type
 * @param pkt_status - pointer to packet status structure
 */
void lp0_rx_done_handler(int err, uint8_t *port, uint8_t *payload, size_t *len,
			 uint8_t *raw_payload, size_t *raw_len, lr11xx_radio_pkt_type_t pkt_type,
			 void *pkt_status)
{
	ARG_UNUSED(raw_payload);
	ARG_UNUSED(raw_len);
	ARG_UNUSED(pkt_type);
	ARG_UNUSED(pkt_status);

	if (err) {
		if (err == -ETIMEDOUT) {
			LOG_ERR("RX error: -ETIMEDOUT");
			return;
		} else {
			LOG_ERR("RX error: %d", err);
			return;
		}
	}

	if (*len > 0) {
		LOG_INF("Received on Port: %d, length: %d", *port, (int)*len);
		LOG_HEXDUMP_INF(payload, *len, "Received payload");
	} else {
		LOG_INF("Received empty payload");
	}
}

/**
 * @brief TX done handler
 *
 * @param err - error code
 */
void lp0_tx_done_handler(int err)
{
	err = lp0_start_message_receive(prv_context, FREQ_IN_HZ_RX, LR11XX_RADIO_LORA_IQ_INVERTED,
					LP0_RX_TIMEOUT_MS);
	if (err) {
		LOG_ERR("lp0_receive_message failed: %d", err);
	}
}

void main(void)
{
	LOG_INF("LP0 send sample");
	apps_common_lr11xx_system_init(prv_context);

	apps_common_lr11xx_fetch_and_print_version(prv_context);

	/* --- Define params --- */
	/* Packet and modulation params - FHSS */
	const lr11xx_lr_fhss_params_t lp0_fhss_params = {
		.lr_fhss_params =
			{
				.sync_word = prv_sync_word,
				.modulation_type = LR_FHSS_V1_MODULATION_TYPE_GMSK_488,
				.cr = LR_FHSS_V1_CR_1_3,
				.grid = LR_FHSS_V1_GRID_3906_HZ,
				.bw = LR_FHSS_V1_BW_136719_HZ,
				.enable_hopping = true,
				.header_count = 3,
			},
		.device_offset = 0,
	};

	/* Packet and modulation params - Normal LR */
	const lr11xx_radio_mod_params_lora_t lora_mod_params = {
		.sf = LR11XX_RADIO_LORA_SF9,
		.bw = LR11XX_RADIO_LORA_BW_125,
		.cr = LR11XX_RADIO_LORA_CR_4_5,
		.ldro = apps_common_compute_lora_ldro(LR11XX_RADIO_LORA_SF9,
						      LR11XX_RADIO_LORA_BW_125),
	};

	const lr11xx_radio_pkt_params_lora_t lora_pkt_params = {
		.preamble_len_in_symb = LR_LORA_PREAMBLE_LENGTH,
		.header_type = LR11XX_RADIO_LORA_PKT_EXPLICIT,
		.pld_len_in_bytes = 0, /* Gets set when sending (lp0_send_message) */
		.crc = LR11XX_RADIO_LORA_CRC_ON,
		.iq = LR11XX_RADIO_LORA_IQ_STANDARD,
	};

	/* --- Set params --- */
	struct lp0_cfg cfg;
	cfg.lora_syncword = LR_LORA_SYNCWORD;
	cfg.irq_mask = IRQ_MASK;
	cfg.pkt_type = LR11XX_RADIO_PKT_TYPE_LORA;

	struct lorawan_auth_info lora_auth_info;
	memcpy(lora_auth_info.AppSKey, prv_app_key, sizeof(lora_auth_info.AppSKey));
	memcpy(lora_auth_info.NwkSKey, prv_network_key, sizeof(lora_auth_info.NwkSKey));
	memcpy(lora_auth_info.DevAddr, prv_dev_addr, sizeof(lora_auth_info.DevAddr));
	cfg.lora_auth_info = lora_auth_info;

	struct lp0_communication_params params;
	if (prv_fhss) {
		params.lr_fhss.fhss_params = lp0_fhss_params;
	} else {
		params.lr_standard.mod_params = lora_mod_params;
		params.lr_standard.pkt_params = lora_pkt_params;
	}

#ifdef CONFIG_RF_FRONT_END_MODULE
	/* --- Configure rf front end module --- */
	rf_front_end_module_init();

	/* Set rf front end module to tx mode */
	rf_front_end_module_set_mode(RF_FRONT_END_MODE_TX);
#endif /* CONFIG_RF_FRONT_END_MODULE */

	struct lp0_callbacks callbacks = {
		.rx_done = lp0_rx_done_handler,
		.tx_done = lp0_tx_done_handler,
	};

	/* Initialize LP0 */
	lp0_init(prv_context, &cfg, &callbacks);

	/* Configure LP0 */
	lp0_configure(prv_context, prv_fhss, FREQ_IN_HZ_TX, &params);

	/* --- Set random payload --- */
	for (int i = 0; i < 255; i++) {
		if (i % 2 == 0) {
			prv_payload[i] = 42;
		} else {
			prv_payload[i] = 69;
		}
	}

	while (1) {
		int64_t start_time = k_uptime_get();
		/* Request sending */
		int err = lp0_send_message(prv_context, prv_payload, prv_payload_size, prv_port,
					   prv_fhss, FREQ_IN_HZ_TX, false, true,
					   LR11XX_RADIO_LORA_IQ_STANDARD);
		if (err) {
			LOG_ERR("lp0_send_message failed: %d", err);
		}
		LOG_INF("Message sent on port %d, length %d", prv_port, prv_payload_size);
		/* Sleep for the rest of the period */
		if (k_uptime_get() - start_time < LR_SENDING_PERIOD_MS) {
			k_sleep(K_MSEC(LR_SENDING_PERIOD_MS - (k_uptime_get() - start_time)));
		}
	}

	LOG_ERR("EXITING");
	return;
}
