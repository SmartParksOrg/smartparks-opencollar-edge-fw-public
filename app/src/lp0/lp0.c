/** @file lp0.c
 *
 * @brief LP0 thread implementation
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2025 Irnas. All rights reserved.
 */

#include "communication.h"
#include "flash_interface.h"
#include "global_time.h"
#include "led.h"
#include "lorawan.h"
#include "lr11xx_radio.h"
#include "lr11xx_regmem.h"

#ifdef CONFIG_RF_FRONT_END_MODULE
#include "rf_front_end_module.h"
#endif

#include "smtc_modem_api.h"
#include "smtc_modem_hal_init.h"
#include "status.h"
#include "thread_com.h"
#include "thread_watchdog.h"
#include <math.h>
#include <lorawan_tools.h>
#include <lp0_communication.h>
#include <lp0_offload.h>
#include <messages_def.h>
#include <settings_def.h>
#include <values_def.h>

#include <time.h>
#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/base64.h>
#include <lr11xx_lr_fhss_types.h>
#include <lr11xx_radio_types.h>
#include <lr11xx_system.h>

#include <lp0.h>

LOG_MODULE_REGISTER(lp0);

/**
 * @brief LR11xx interrupt mask used by the application
 */
#define IRQ_MASK                                                                                   \
	(LR11XX_SYSTEM_IRQ_TX_DONE | LR11XX_SYSTEM_IRQ_RX_DONE | LR11XX_SYSTEM_IRQ_TIMEOUT |       \
	 LR11XX_SYSTEM_IRQ_HEADER_ERROR | LR11XX_SYSTEM_IRQ_CRC_ERROR |                            \
	 LR11XX_SYSTEM_IRQ_FSK_LEN_ERROR)

/* Thread stack size */
#define LP0_STACK_SIZE 8192

#define LP0_LORA_SYNCWORD 0x34 // 0x12 Private Network, 0x34 Public Network

#define LP0_PACKET_TYPE LR11XX_RADIO_PKT_TYPE_LORA

#define LP0_THREAD_PRIORITY       7
#define LP0_THREAD_START_DELAY_MS 40000
#define LP0_LORAWAN_SEM_TIMEOUT_MS                                                                 \
	((int)(THREAD_LR_GPS_MAX_RESPONSE / 2 * 1000)) /* Max semaphore timeout is half of the     \
							Lorawan thread watchdog timeout time */

/* Define event que to hold 16 lp0 events data
 * This queue is used for other modules to schedule messages or commands to the lp0 thread
 */
K_MSGQ_DEFINE(lp0_event_que, sizeof(struct lp0_event_data), 16, 4);

extern struct k_sem lora_chip_suspended_sem;

/* Sync word for FHSS */
static const uint8_t sync_word[] = {0x2c, 0x0f, 0x79, 0x95};

bool prv_lorawan_suspended = false;
static uint8_t prv_flash_tx_buf[250];
extern struct k_sem lp0_flash_save_sem;

static uint8_t prv_offload_mode =
	0; /* 0 - Device will not offload (LP0 sending is still
	       permitted), 1 - Device will offload, 2 - Device is offload station. */
static enum lp0_mode prv_mode = LP0_MODE_DISABLED;

/* --- Params --- */
static struct lp0_cfg prv_cfg;
static struct lp0_communication_params prv_params;

/* Semaphore used to signal a tx or rx done */
K_SEM_DEFINE(lp0_tx_rx_done_sem, 0, 1);

/* ---------------- LoRa parameters ---------------- */
/* Packet parameters for LoRa packets */
#define LR_LORA_PREAMBLE_LENGTH 8

/* Packet and modulation params - FHSS */
static lr11xx_lr_fhss_params_t prv_lp0_fhss_params = {
	.lr_fhss_params = {.sync_word = sync_word,
			   .modulation_type = LR_FHSS_V1_MODULATION_TYPE_GMSK_488,
			   .cr = LR_FHSS_V1_CR_1_3,
			   .grid = LR_FHSS_V1_GRID_3906_HZ,
			   .bw = LR_FHSS_V1_BW_136719_HZ,
			   .enable_hopping = true,
			   .header_count = 3},
	.device_offset = 0};

/* Packet and modulation params - Normal LR */
static lr11xx_radio_mod_params_lora_t prv_lora_mod_params; /* Set at runtime */

static lr11xx_radio_pkt_params_lora_t prv_lora_pkt_params = {
	.preamble_len_in_symb = LR_LORA_PREAMBLE_LENGTH,
	.header_type = LR11XX_RADIO_LORA_PKT_EXPLICIT,
	.pld_len_in_bytes = 0, // to be set later
	.crc = LR11XX_RADIO_LORA_CRC_ON,
	.iq = LR11XX_RADIO_LORA_IQ_STANDARD,
};

static const struct device *prv_lora_context = DEVICE_DT_GET(DT_NODELABEL(lr11xx));

/* LoRaWAN thread loop */
static void lp0_main_loop(void);

/* Create LP0 thread */
K_THREAD_DEFINE(lp0_thread_id, LP0_STACK_SIZE, lp0_main_loop, NULL, NULL, NULL, LP0_THREAD_PRIORITY,
		0, LP0_THREAD_START_DELAY_MS);

/**
 * @brief Prepare and send message with given settings
 *
 * @param timer_id - Timer ID for scheduling ping message send (unused)
 */
static void ping_event_timer_handler(struct k_timer *timer_id)
{
	ARG_UNUSED(timer_id);
	struct lp0_event_data new_event;

	new_event.type = LP0_EVENT_CMD;
	new_event.data.cmd.cmd = LP0_CMD_SET_MODE;
	new_event.mode = LP0_MODE_DEVICE_DISCOVERY;

	int ret = k_msgq_put(&lp0_event_que, &new_event, K_NO_WAIT);
	if (ret != 0) {
		LOG_ERR("Failed to schedule LP0 ping event: %d", ret);
	}
}

K_TIMER_DEFINE(ping_event_timer, ping_event_timer_handler, NULL);

/*!
 * @brief A function to get the value for low data rate optimization setting
 *
 * @param [in] sf  LoRa Spreading Factor
 * @param [in] bw  LoRa Bandwidth
 *
 * @return 1 if low data rate optimization should be enabled, 0 otherwise
 */
inline static uint8_t prv_lp0_compute_lora_ldro(const lr11xx_radio_lora_sf_t sf,
						const lr11xx_radio_lora_bw_t bw)
{
	switch (bw) {
	case LR11XX_RADIO_LORA_BW_500:
		return 0;

	case LR11XX_RADIO_LORA_BW_250:
		if (sf == LR11XX_RADIO_LORA_SF12) {
			return 1;
		} else {
			return 0;
		}

	case LR11XX_RADIO_LORA_BW_800:
	case LR11XX_RADIO_LORA_BW_400:
	case LR11XX_RADIO_LORA_BW_200:
	case LR11XX_RADIO_LORA_BW_125:
		if ((sf == LR11XX_RADIO_LORA_SF12) || (sf == LR11XX_RADIO_LORA_SF11)) {
			return 1;
		} else {
			return 0;
		}

	case LR11XX_RADIO_LORA_BW_62:
		if ((sf == LR11XX_RADIO_LORA_SF12) || (sf == LR11XX_RADIO_LORA_SF11) ||
		    (sf == LR11XX_RADIO_LORA_SF10)) {
			return 1;
		} else {
			return 0;
		}

	case LR11XX_RADIO_LORA_BW_41:
		if ((sf == LR11XX_RADIO_LORA_SF12) || (sf == LR11XX_RADIO_LORA_SF11) ||
		    (sf == LR11XX_RADIO_LORA_SF10) || (sf == LR11XX_RADIO_LORA_SF9)) {
			return 1;
		} else {
			return 0;
		}

	case LR11XX_RADIO_LORA_BW_31:
	case LR11XX_RADIO_LORA_BW_20:
	case LR11XX_RADIO_LORA_BW_15:
	case LR11XX_RADIO_LORA_BW_10:
		// case LR11XX_RADIO_LORA_BW_7:
		return 1;

	default:
		return 0;
	}
}

/**
 * @brief RX done handler
 *
 * @param err - error code, 0 if no error
 * @param port - port on which message was received
 * @param payload - pointer to received payload
 * @param len - pointer to length of received payload
 * @param raw_payload - pointer to raw payload bytes
 * @param raw_len - pointer to length of raw payload
 * @param pkt_type - packet type
 * @param pkt_status - pointer to packet status structure
 */
static void prv_lp0_device_on_rx_done(int err, uint8_t *port, uint8_t *payload, size_t *len,
				      uint8_t *raw_payload, size_t *raw_len,
				      lr11xx_radio_pkt_type_t pkt_type, void *pkt_status)
{
#ifdef CONFIG_RF_FRONT_END_MODULE
	if (Main_settings.s_band_rf_frequency_hz->def_val >= 1000000000) {
		rf_front_end_module_set_mode(RF_FRONT_END_MODE_SLEEP);
	}
#endif
	if (err) {
		led_turn_off(LED_ALL);
		if (err == -ETIMEDOUT) {
			/* NOTE: Logging here can delay RX windows */
			LOG_WRN("RX TIMED OUT");
		} else {
			LOG_ERR("RX error: %d", err);
		}

		if (prv_mode == LP0_MODE_OFFLOAD_STATION) {
			LOG_WRN("Restarting RX");
			/* Stop LP0 RX + LoRaWAN RX1 windows */
			lp0_stop_continuous_message_receive(prv_lora_context);
			lp0_start_receive(prv_lora_context,
					  Main_settings.lp0_rx_frequency_hz->def_val,
					  LR11XX_RADIO_LORA_IQ_STANDARD, 0, true);
		}

		goto exit_rx_done;
	}

	if (*len > 0) {
		LOG_INF("Received on Port: %d, length: %d", *port, (int)*len);
		LOG_HEXDUMP_DBG(payload, *len, "Received payload");

		if (*port == 33) {
			/* Port 33 is only used for offloading communication */
			led_turn_on(LED_Y);
			if (prv_mode == LP0_MODE_OFFLOAD_STATION) {
				/* If offload station: LUKATODO:TODO-FUTURE:
				- Get pinging devices dev_addr from the message, check if there's a
				  match
				- number of last saved msg
				- toggle if saving messages from all sources or only pinged ACK
				  device
				*/

				lp0_offload_station_discovery_acknowledge();

				goto exit_rx_done;
			} else {
				/* Handle LP0 commands */
				lp0_offload_handle_lp0_recv(payload, len, &prv_mode);
				prv_lorawan_suspended = false;
			}

		} else {
			if (prv_mode == LP0_MODE_OFFLOAD_STATION) {
				int err = lp0_offload_station_data_receive(raw_payload, raw_len,
									   pkt_type, pkt_status);
				if (err) {
					LOG_ERR("Failed to handle offload station discovery RX: %d",
						err);
				}
				goto exit_rx_done;
			} else {
				/* Serve data to Lorawan msg parser */
				handle_lora_recv(payload, *len, *port);
			}
		}
	} else {
		LOG_INF("Received empty payload");
	}

exit_rx_done:
	if (prv_mode != LP0_MODE_OFFLOAD_STATION) {
		k_sem_give(&lp0_tx_rx_done_sem);
	}
	return;
}

extern struct k_sem flash_send_lp0;
extern struct k_sem flash_offload_start_lp0;
extern struct k_sem flash_offload_done_lp0;

/**
 * @brief TX done handler
 *
 * @param err - error code, 0 if no error
 */
static void prv_lp0_on_tx_done(int err)
{
#ifdef CONFIG_RF_FRONT_END_MODULE
	if (Main_settings.s_band_rf_frequency_hz->def_val >= 1000000000) {
		rf_front_end_module_set_mode(RF_FRONT_END_MODE_SLEEP);
	}
#endif

	if (err) {
		LOG_ERR("TX failed with error: %d", err);
	} else {
		LOG_INF("TX done");
	}

	if (prv_mode == LP0_MODE_DEVICE_DATA_TRANSFER) {
		led_turn_off(LED_ALL);
		k_sem_give(&flash_send_lp0);
	}

	/* Give tx semaphore, signaling rx can start */
	k_sem_give(&lp0_tx_rx_done_sem);
}

/**
 * @brief Put new event in event que
 *
 * @param[in] msg new event
 */
static void prv_put_event_in_que(struct lp0_event_data msg)
{
	/* Put event command in que */
	while (k_msgq_put(&lp0_event_que, &msg, K_NO_WAIT) != 0) {
		/* command queue is full: purge old data & try again */
		LOG_WRN("Message que is full, remove oldest message!");
		struct lp0_event_data tmp_msg;
		k_msgq_get(&lp0_event_que, &tmp_msg, K_NO_WAIT);
	}
}

/**
 * @brief handle LP0 events
 *
 * @param timeout - timeout for waiting for events
 * @param params - pointer to communication params struct
 */
static void prv_lp0_handle_events(k_timeout_t timeout, struct lp0_communication_params *params)
{
	struct lp0_event_data event;
	LOG_INF("Waiting for LP0 event");

	int err = k_msgq_get(&lp0_event_que, &event, timeout);
	if (err) {
		return;
	}

	switch (event.type) {
	case LP0_EVENT_MESSAGE:
	case LP0_EVENT_S_BAND_MESSAGE:
		if (params == NULL) {
			LOG_ERR("LP0 params is NULL, cannot send "
				"message");
			break;
		}

		lp0_update_communication_settings();

		/* Suspend lorawan module and wait for suspension */
		lp0_suspend_lorawan_and_wait_for_suspension(LP0_LORAWAN_SEM_TIMEOUT_MS);

		/* Reset semaphore count in case of residual TX/RX error states from
		 * previous operations */
		k_sem_reset(&lp0_tx_rx_done_sem);

		/* Re-initialize LR11XX chip for LP0 */
		lp0_lr11xx_system_init(prv_lora_context);

		if (event.data.msg.fhss) {
			LOG_DBG("LP0 got FHSS message to send on port: "
				"%d of length: %d",
				event.data.msg.port, event.data.msg.len);
			params->lr_fhss.fhss_params = prv_lp0_fhss_params;
		} else {
			LOG_DBG("LP0 got LR message to send on port: %d "
				"of length: %d",
				event.data.msg.port, event.data.msg.len);
			params->lr_standard.mod_params = prv_lora_mod_params;
			params->lr_standard.pkt_params = prv_lora_pkt_params;
		}

		LOG_DBG("LP0 got message to send on port: %d of length: "
			"%d",
			event.data.msg.port, event.data.msg.len);

		if (event.type == LP0_EVENT_S_BAND_MESSAGE) {
#ifdef CONFIG_RF_FRONT_END_MODULE
			if (Main_settings.s_band_rf_frequency_hz->def_val >= 1000000000) {
				/* Enable rf front end module (We expect
				 * the module to be already initialized by
				 * other processes at this point) */
				rf_front_end_module_set_mode(RF_FRONT_END_MODE_TX);
			}
#endif
			/* Send message - use s-band frequency */
			lp0_configure(prv_lora_context, event.data.msg.fhss,
				      Main_settings.s_band_rf_frequency_hz->def_val, params);

			lp0_send_message(prv_lora_context, event.data.msg.data, event.data.msg.len,
					 event.data.msg.port, event.data.msg.fhss,
					 Main_settings.s_band_rf_frequency_hz->def_val,
					 event.data.msg.confirmed, true,
					 LR11XX_RADIO_LORA_IQ_STANDARD);
		} else {
#ifdef CONFIG_RF_FRONT_END_MODULE
			if (Main_settings.lp0_tx_frequency_hz->def_val >= 1000000000) {
				/* Enable rf front end module (We expect
				 * the module to be already initialized by
				 * other processes at this point) */
				rf_front_end_module_set_mode(RF_FRONT_END_MODE_TX);
			}
#endif
			/* Send message */
			lp0_configure(prv_lora_context, event.data.msg.fhss,
				      Main_settings.lp0_tx_frequency_hz->def_val, params);

			LOG_HEXDUMP_DBG(event.data.msg.data, event.data.msg.len, "LP0 Payload");

			lp0_send_message(prv_lora_context, event.data.msg.data, event.data.msg.len,
					 event.data.msg.port, event.data.msg.fhss,
					 Main_settings.lp0_tx_frequency_hz->def_val,
					 event.data.msg.confirmed, true,
					 LR11XX_RADIO_LORA_IQ_STANDARD);

			/* Wait for tx_done */
			k_sem_take(&lp0_tx_rx_done_sem, K_SECONDS(15));

			/* Start receiving window(s) */
			/* Start LP0 RX window(s) + concatenated Lorawan RX1 window */
			lp0_start_receive(prv_lora_context,
					  Main_settings.lp0_rx_frequency_hz->def_val,
					  LR11XX_RADIO_LORA_IQ_INVERTED, 0, true);

			uint32_t rx_timeout =
				Main_settings.lp0_communication_params->def_val[3] * 1000 +
				lp0_get_rx_timeout_ms();

			LOG_INF("Sleeping (ms) for RX windows: %d",
				rx_timeout + LP0_RX_WINDOW_SYNC_TIME_MS);
			k_sleep(K_MSEC(rx_timeout + LP0_RX_WINDOW_SYNC_TIME_MS));

			/* Stop LP0 RX + LoRaWAN RX1 windows */
			lp0_stop_continuous_message_receive(prv_lora_context);

			/* Sleep for one second in between lorawan RX1 and RX2 windows */
			k_sleep(K_SECONDS(1));

			/* Reset semaphore count in case of unexpected TX/RX error */
			k_sem_reset(&lp0_tx_rx_done_sem);

			/* Start Lorawan RX2 window */
			lp0_start_receive(
				prv_lora_context, Main_settings.lp0_rx_frequency_hz->def_val,
				LR11XX_RADIO_LORA_IQ_INVERTED, lp0_get_rx_timeout_ms(), false);

			/* Wait for RX2 window to close */
			k_sem_take(&lp0_tx_rx_done_sem, K_SECONDS(15));

			/* Resume lorawan module */
			k_sem_give(&lora_chip_suspended_sem);
			lorawan_resume();
		}
		break;

	case LP0_EVENT_CMD:
		LOG_DBG("LP0 got cmd: %d", event.data.cmd.cmd);
		switch (event.data.cmd.cmd) {
		// LUKATODO:TODO-FUTURE: add implementation for all commands
		case LP0_CMD_DISABLE:
			LOG_INF("LP0 disable cmd");
			break;
		case LP0_CMD_START:
			LOG_INF("LP0 start cmd");
			break;
		case LP0_CMD_RESET:
			LOG_INF("LP0 reset cmd");

			break;
		case LP0_CMD_SUSPEND:
			LOG_INF("LP0 suspend cmd");
			break;
		case LP0_CMD_RESUME:
			LOG_INF("LP0 resume cmd");

			break;
		case LP0_CMD_SET_MODE:
			LOG_INF("LP0 set mode cmd");
			prv_mode = event.mode;
			break;
		case LP0_CMD_TEST_FLASH_FILL_100_MESSAGES:
			LOG_INF("Received flash fill 100 messages command from LP0 "
				"device!");

			prv_flash_tx_buf[0] = 0xF4;
			prv_flash_tx_buf[1] = 0x0e;
			for (uint8_t i = 0; i < 100; i++) {
				uint8_t status_size = status_get_message(prv_flash_tx_buf + 2,
									 sizeof(prv_flash_tx_buf));
				thread_put_message(MB_MSG_DEV, MB_MSG_FLASH, MB_MSG_STORE,
						   Main_messages.msg_status->port, prv_flash_tx_buf,
						   status_size + 2, 0);

				led_turn_on(LED_R);
				/* Save message to flash and wait for it to save */
				k_sem_take(&lp0_flash_save_sem, K_FOREVER);
				led_turn_off(LED_ALL);
				k_sleep(K_MSEC(100));
			}
			break;
		default:
			LOG_WRN("LP0 unknown cmd: %d", event.data.cmd.cmd);
			break;
		}
		break;

	default:
		LOG_WRN("LP0 unknown event type: %d", event.type);
		break;
	}
}

/**
 * @brief Main loop for LP0 thread
 */
static void lp0_main_loop(void)
{
	LOG_INF("*    Starting LP0 thread    *");
	LOG_INF("*****************************\n");

	lp0_update_communication_settings();

	struct lp0_callbacks callbacks = {
		.tx_done = prv_lp0_on_tx_done,
		.rx_done = prv_lp0_device_on_rx_done,
	};

	lp0_init(prv_lora_context, &prv_cfg, &callbacks);

	int64_t ping_latest_start_time = 0;

	while (true) {
		k_sleep(K_MSEC(100));
		switch (prv_mode) {
		case LP0_MODE_DISABLED:
			prv_lp0_handle_events(K_SECONDS(1), &prv_params);
			prv_mode = LP0_MODE_SETUP;
			break;

		case LP0_MODE_SETUP:
			LOG_INF("LP0 node params:\nIs offload station? %s\nOffload Station "
				"ID:%d\nMax number of nodes:%d\nSend ping with LoRaWAN header: %s",
				Main_settings.lp0_node_params->def_val[0] == 2 ? "Yes" : "No",
				Main_settings.lp0_node_params->def_val[1],
				Main_settings.lp0_node_params->def_val[2],
				Main_settings.lp0_node_params->def_val[3] ? "Yes" : "No");

			if (Main_settings.lp0_node_params->def_val[0] == 2) {
				/* Device is offload station */
				prv_mode = LP0_MODE_OFFLOAD_STATION;
			} else if (Main_settings.lp0_node_params->def_val[0] == 1) {
				/* Device is normal LP0 node */
				/* Schedule first ping immediately if enabled */
				k_timer_start(&ping_event_timer, K_NO_WAIT, K_NO_WAIT);

				prv_mode = LP0_MODE_DEFAULT;
			} else {
				/* Device is not using offloading, go to idle */
				prv_mode = LP0_MODE_DEFAULT;
			}
			prv_offload_mode = Main_settings.lp0_node_params->def_val[0];
			break;

		case LP0_MODE_DEFAULT:
			if (prv_offload_mode != Main_settings.lp0_node_params->def_val[0]) {
				prv_mode = LP0_MODE_SETUP;
				break;
			}
			/* Handle idle state */
			prv_lp0_handle_events(K_FOREVER, &prv_params);
			break;

		/* LP0 Offload modes below */
		case LP0_MODE_DEVICE_DISCOVERY:
			LOG_INF("Discovery Ping Mode");
			ping_latest_start_time = k_uptime_get();

			led_turn_on(LED_Y);
			lp0_offload_device_discovery_mode();
			led_turn_off(LED_Y);

			/* If after discovery we are still in discovery mode, schedule new ping */
			if (prv_mode == LP0_MODE_DEVICE_DISCOVERY) {
				/* Schedule new discovery event after X (user configurable
				 * amount of) seconds */
				int64_t delay_until_next_ping_ms =
					Main_settings.lp0_node_params->def_val[4] * 1000 -
					(k_uptime_get() - ping_latest_start_time);
				k_timer_start(&ping_event_timer,
					      K_MSEC(delay_until_next_ping_ms > 0
							     ? delay_until_next_ping_ms
							     : 0),
					      K_NO_WAIT);

				/* Set mode back to event handling */
				prv_mode = LP0_MODE_DEFAULT;
			}
			break;

		case LP0_MODE_DEVICE_DATA_TRANSFER:
			/* Transfer X amount of data until timeout or
			 * complete, lastly send confirmed uplink and
			 * expect confirmation for successfully
			 * transferred logs. Mark successfully sent
			 * messages as sent and continue down flash until
			 * all are transferred. Lastly return to Idle */

			/* LUKATODO:TODO-FUTURE: If discovery results in a specific log request,
			 * send that specific message, confirm and return
			 * to idle */

			/* Give semaphore signaling entry into data transfer mode */
			if (prv_lorawan_suspended == false) {
				prv_lorawan_suspended = true;
				/* Suspend lorawan module and wait for suspension */
				lorawan_suspend();
				k_sem_take(&lora_chip_suspended_sem, K_FOREVER);

				lp0_lr11xx_system_init(prv_lora_context);
			}

			k_sem_give(&flash_offload_start_lp0);

			/* Wait for semaphore to signal end of data transfer */
			LOG_INF("Waiting for flash offload complete semaphore...");
			k_sem_take(&flash_offload_done_lp0, K_FOREVER);

			prv_lorawan_suspended = false;
			k_sem_give(&lora_chip_suspended_sem);
			lorawan_resume();

			/* Schedule new discovery event after X (user configurable
			 * amount of) seconds */
			int64_t delay_until_next_ping_ms =
				Main_settings.lp0_node_params->def_val[4] * 1000 -
				(k_uptime_get() - ping_latest_start_time);
			k_timer_start(
				&ping_event_timer,
				K_MSEC(delay_until_next_ping_ms > 0 ? delay_until_next_ping_ms : 0),
				K_NO_WAIT);

			/* Set mode back to event handling */
			prv_mode = LP0_MODE_DEFAULT;

			break;

		case LP0_MODE_OFFLOAD_STATION:
			/*
				1. Enable RX and wait for ping from device
			   to offload station
				2. Offload station responds with ack
				3. Device starts sending data to offload
			   station
				LUKATODO:TODO-FUTURE: 4. and 5.
				4. After X messages received, offload
			   station sends confirmation message with bitmap
			   of sent messages
				5. Device marks messages as sent and
			   continues with #2 until all messages are sent,
			   or timeout occurs (device is out of range)
			*/

			lp0_offload_station_discovery();

			/* Return lorawan module to normal operation */
			prv_lorawan_suspended = false;
			k_sem_give(&lora_chip_suspended_sem);
			lorawan_resume();
			break;
		default:
			prv_lp0_handle_events(K_FOREVER, &prv_params);
			break;
		}
	}
}

void lp0_disable(void)
{
	/* Create disable event */
	LOG_DBG("Got LP0 disable command, create disable event.");
	struct lp0_event_data event;
	event.type = LP0_EVENT_CMD;

	event.data.cmd.cmd = LP0_CMD_DISABLE;

	/* Put event command que */
	prv_put_event_in_que(event);
}

void lp0_start(void)
{
	/* Create start event */
	LOG_DBG("Got LP0 start command, create start event.");
	struct lp0_event_data event;
	event.type = LP0_EVENT_CMD;

	event.data.cmd.cmd = LP0_CMD_START;

	/* Put event command que */
	prv_put_event_in_que(event);
}

void lp0_reset(void)
{
	/* Create reset event */
	LOG_DBG("Got LP0 reset command, create reset event.");
	struct lp0_event_data event;
	event.type = LP0_EVENT_CMD;
	event.data.cmd.cmd = LP0_CMD_RESET;

	/* Put event command que */
	prv_put_event_in_que(event);
}

void lp0_suspend(void)
{
	/* Create suspend event */
	LOG_DBG("Got LP0 suspend command, create suspend event.");
	struct lp0_event_data event;
	event.type = LP0_EVENT_CMD;

	event.data.cmd.cmd = LP0_CMD_SUSPEND;

	/* Put event command que */
	prv_put_event_in_que(event);
}

void lp0_resume(void)
{
	/* Create resume event */
	LOG_DBG("Got LP0 resume command, create resume event.");
	struct lp0_event_data event;
	event.type = LP0_EVENT_CMD;

	event.data.cmd.cmd = LP0_CMD_RESUME;

	/* Put event command que */
	prv_put_event_in_que(event);
}

void lp0_set_mode(enum lp0_mode mode)
{
	/* Create set mode event */
	LOG_DBG("Got LP0 set mode command, create set mode event.");
	struct lp0_event_data event;
	event.type = LP0_EVENT_CMD;

	event.data.cmd.cmd = LP0_CMD_SET_MODE;
	event.mode = mode;

	/* Put event command que */
	prv_put_event_in_que(event);
}

int lp0_add_message_to_send_queue(uint8_t *payload, uint8_t len, uint8_t port, bool fhss,
				  bool confirmed)
{
	/* Create send message event */
	LOG_DBG("Got LP0 send message command, create send message "
		"event.");
	struct lp0_event_data event;
	event.type = LP0_EVENT_MESSAGE;

	memcpy(event.data.msg.data, payload, len);
	event.data.msg.len = len;
	event.data.msg.port = port;
	event.data.msg.fhss = fhss;
	event.data.msg.confirmed = confirmed;

	/* Put event command que */
	prv_put_event_in_que(event);

	return 0;
}

int lp0_send_command(enum lp0_cmd_type cmd)
{
	/* Create send command event */
	LOG_DBG("Got LP0 send command: %d, create send command "
		"event.",
		cmd);
	struct lp0_event_data event;
	event.type = LP0_EVENT_CMD;

	event.data.cmd.cmd = cmd;

	/* Put event command que */
	prv_put_event_in_que(event);

	return 0;
}

uint32_t lp0_get_rx_timeout_ms(void)
{
	lr11xx_radio_pkt_params_lora_t pkt_params;
	memcpy(&pkt_params, &prv_params.lr_standard.pkt_params, sizeof(pkt_params));
	prv_params.lr_standard.pkt_params.pld_len_in_bytes = LP0_MAX_BUF_SIZE;

	lr11xx_radio_mod_params_lora_t mod_params;
	memcpy(&mod_params, &prv_params.lr_standard.mod_params, sizeof(mod_params));

	return lr11xx_radio_get_lora_time_on_air_in_ms(&pkt_params, &mod_params);
}

void lp0_update_communication_settings(void)
{
	/* Packet and modulation params - Normal LR */
	prv_lora_mod_params.sf = Main_settings.lp0_communication_params->def_val[0];
	prv_lora_mod_params.bw = Main_settings.lp0_communication_params->def_val[1];
	prv_lora_mod_params.cr = Main_settings.lp0_communication_params->def_val[2];
	prv_lora_mod_params.ldro =
		prv_lp0_compute_lora_ldro(Main_settings.lp0_communication_params->def_val[0],
					  Main_settings.lp0_communication_params->def_val[1]);

	prv_cfg.lora_syncword = LP0_LORA_SYNCWORD;
	prv_cfg.irq_mask = IRQ_MASK;
	prv_cfg.pkt_type = LP0_PACKET_TYPE;

	memcpy(prv_cfg.lora_auth_info.NwkSKey, Main_settings.lp0_network_key->def_val,
	       sizeof(prv_cfg.lora_auth_info.NwkSKey));
	memcpy(prv_cfg.lora_auth_info.AppSKey, Main_settings.lp0_app_key->def_val,
	       sizeof(prv_cfg.lora_auth_info.AppSKey));
	memcpy(prv_cfg.lora_auth_info.DevAddr, Main_settings.lp0_dev_addr->def_val,
	       sizeof(prv_cfg.lora_auth_info.DevAddr));
}

int lp0_prepare_and_send_message(const struct device *context, uint8_t *payload, uint8_t len,
				 uint8_t port, bool fhss_enabled, uint32_t freq_hz, bool confirmed,
				 bool lorawan_header, lr11xx_radio_lora_iq_t iq_setting)
{
	lp0_update_communication_settings();

#ifdef CONFIG_RF_FRONT_END_MODULE
	if (Main_settings.lp0_tx_frequency_hz->def_val >= 1000000000) {
		/* Enable rf front end module (We expect the module to be already
		 * initialized by other processes at this point) */
		rf_front_end_module_set_mode(RF_FRONT_END_MODE_TX);
	}
#endif

	/* Packet and modulation params - Normal LR */
	prv_params.lr_standard.mod_params = prv_lora_mod_params;
	prv_params.lr_standard.pkt_params = prv_lora_pkt_params;

	/* Send message */
	lp0_configure(prv_lora_context, fhss_enabled, freq_hz, &prv_params);

	int err = lp0_send_message(prv_lora_context, payload, len, port, false, freq_hz, confirmed,
				   lorawan_header, iq_setting);
	if (err) {
		LOG_ERR("ERR: %d", err);
		return err;
	}

	return 0;
}

int lp0_start_receive(const struct device *context, uint32_t freq_hz,
		      lr11xx_radio_lora_iq_t iq_setting, uint32_t timeout_ms, bool continuous)
{
	int err;

	if (continuous == true) {
		timeout_ms = 0xFFFFFF; /* continuous rx */
	}

	prv_params.lr_standard.mod_params = prv_lora_mod_params;
	prv_params.lr_standard.pkt_params = prv_lora_pkt_params;

	lp0_configure(prv_lora_context, false, freq_hz, &prv_params);

	LOG_DBG("LP0 RX params: SF: %d, BW: %d, CR: %d, LDRO: %d", prv_lora_mod_params.sf,
		prv_lora_mod_params.bw, prv_lora_mod_params.cr, prv_lora_mod_params.ldro);

	LOG_DBG("PKT_PARAMS: Preamble: %d, Header: %d, PLD len: %d, CRC: %d, IQ: %d",
		prv_lora_pkt_params.preamble_len_in_symb, prv_lora_pkt_params.header_type,
		prv_lora_pkt_params.pld_len_in_bytes, prv_lora_pkt_params.crc,
		prv_lora_pkt_params.iq);

	LOG_DBG("FREQ: %d", freq_hz);

	err = lp0_start_message_receive(prv_lora_context, freq_hz, iq_setting, timeout_ms);
	if (err) {
		LOG_ERR("Failed to start message receive: %d", err);
		return err;
	}

	return 0;
}

void lp0_command_parser(const uint8_t *message, uint8_t size)
{
	enum lp0_cmd_type command_type = message[0];

	// LUKATODO:TODO-FUTURE: add command implementations
	switch ((int)command_type) {
	case LP0_CMD_DISABLE:
		break;
	case LP0_CMD_SUSPEND:
		break;
	case LP0_CMD_RESUME:
		break;
	case LP0_CMD_SET_MODE:
		if (size != 2) {
			LOG_WRN("LP0 SET MODE command size too small: %d", size);
			break;
		}
		enum lp0_mode new_mode = message[1];
		prv_mode = new_mode;
		LOG_INF("Set LP0 mode to: %d", prv_mode);
		break;
	case LP0_CMD_SEND_PING:
		break;
	case LP0_CMD_START:
		break;
	case LP0_CMD_RESET:
		break;
	default:
		LOG_WRN("Unknown LP0 command: %d", command_type);
		break;
	}
}

int lp0_suspend_lorawan_and_wait_for_suspension(uint32_t timeout_ms)
{
	/* Reset LoRaWAN suspension semaphore to avoid unexpected behavior */
	k_sem_reset(&lora_chip_suspended_sem);

	/* Send suspend event to LoRaWAN thread */
	lorawan_suspend();

	/* Wait for LoRaWAN thread to suspend */
	int err = k_sem_take(&lora_chip_suspended_sem, K_MSEC(timeout_ms));
	if (err) {
		LOG_ERR("Failed to take lora_chip_suspended_sem: %d", err);
		return err;
	}
	return 0;
}
