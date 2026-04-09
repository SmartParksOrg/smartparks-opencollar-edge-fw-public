/** @file lorawan.c
 *
 * @brief Interface for lr1120 and LoRaWAN communication
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2023 Irnas. All rights reserved.
 */

#include "communication.h"
#include "lorawan.h"
#include "settings_def.h"
#include "status.h"
#include "thread_watchdog.h"

#include <smtc_modem_api.h>

#include <smtc_app.h>
#include <smtc_modem_api_str.h>
#include <smtc_modem_hal_init.h>

/* include hal and ralf so that initialization can be done */
#include <ralf_lr11xx.h>
#include <smtc_modem_hal.h>

#include <zephyr/device.h>
#include <zephyr/kernel.h>

/* lr11xx driver */
#include "lr11xx_board.h"
#include "lr11xx_radio.h"

/* gnss */
#include "gnss.h"

/* almanac update */
#include "almanac.h"

/* wifi scan */
#ifdef CONFIG_LR_WIFI_SCAN
#include "wifi_scan.h"
#endif /* CONFIG_LR_WIFI_SCAN */

/* front-end module */
#ifdef CONFIG_RF_FRONT_END_MODULE
#include <rf_front_end_module.h>
#endif /* CONFIG_RF_FRONT_END_MODULE */

/* lr s-band */
#ifdef CONFIG_LR_S_BAND
#include "lr_s_band.h"
#endif /* CONFIG_LR_S_BAND */

/* The function bellow is not exposed via the SMTC API, and adding the private headers of SWL2001 is
 * complicated. So we just add the function we need to call. This way, we do not have to modify the
 * SMTC code. */
extern void modem_set_duty_cycle_disabled_by_host(uint8_t disabled_by_host);

/* VHF burst */
#include "vhf.h"

#include "nvs_storage.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(lorawan, CONFIG_LORAWAN_LOG_LEVEL);

/**
 * @brief TX power offset in dB.
 * Default power for 868 is 14 dBm.
 *
 */
#define LORAWAN_TX_POWER_OFFSET 10

/**
 * @brief Number of join attempts
 *
 */
#define LORAWAN_JOIN_ATTEMPTS 3

/* Stack id value (multistack modem is not yet available, so use 0) */
#define STACK_ID 0

/* Max message length */
#define LORAWAN_MAX_BUF_SIZE 255

/* Thread stack size */
#define LORAWAN_STACK_SIZE 4096

#define LORAWAN_PRIORITY 6

/* If the same or lower, a race-condition causes the device to incorrectly run the smtc engine,
 * which causes a smtc panic and reboots the system. */
BUILD_ASSERT(LORAWAN_PRIORITY < CONFIG_THREAD_PRIORITY,
	     "lorawan_thread must have higher priority than lora_gps_thread");

/* Lora chip semaphore (shared by Lorawan and LP0) */
extern struct k_sem lora_chip_suspended_sem;

/**
 * @brief Adaptive Data Rate (ADR) profile.
 *
 * Available profiles:
 * 	SMTC_MODEM_ADR_PROFILE_NETWORK_CONTROLLED
 * 	SMTC_MODEM_ADR_PROFILE_MOBILE_LONG_RANGE
 * 	SMTC_MODEM_ADR_PROFILE_MOBILE_LOW_POWER
 * 	SMTC_MODEM_ADR_PROFILE_CUSTOM
 *
 * Default value is a placeholder and can be overwritten by setting configuration with
 * `lorawan_set_configuration()`.
 *
 */
smtc_modem_adr_profile_t prv_adr_profile = SMTC_MODEM_ADR_PROFILE_CUSTOM;

/**
 * @brief ADR custom list when ADR profile is set to SMTC_MODEM_ADR_PROFILE_CUSTOM
 */
uint8_t prv_adr_custom_list[16] = {0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05,
				   0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05};

/* Structure defines */

/**
 * @brief LoRawan message structure.
 * Contains data on message length, content and port to be send on.
 *
 */
struct lorawan_message {
	uint8_t len;                        /* Message length */
	uint8_t port;                       /* Sending port */
	uint8_t data[LORAWAN_MAX_BUF_SIZE]; /* Message */
	bool confirmed;                     /* Send message as confirmed */
	bool join_attempt;                  /* Join attempt if not joined */
};

enum lorawan_cmd_type {
	LORAWAN_CMD_LEAVE_NETWORK = 0,
	LORAWAN_CMD_RESET = 1,
	LORAWAN_CMD_START = 2,
	LORAWAN_CMD_GNSS_AUTONOMOUS = 3,
	LORAWAN_CMD_GNSS_ASSISTED = 4,
	LORAWAN_CMD_ALMANAC_UPDATE = 5,
	LORAWAN_CMD_WIFI_SCAN = 6,
	LORAWAN_CMD_MODEM_SUSPEND = 7,
	LORAWAN_CMD_MODEM_RESUME = 8,
};
struct lorawan_cmd {
	enum lorawan_cmd_type cmd;
};

enum lorawan_event_type {
	LORAWAN_EVENT_MESSAGE = 0,
	LORAWAN_EVENT_CMD = 1,
	LORAWAN_EVENT_SETTING = 2,
	LORAWAN_EVENT_S_BAND_MESSAGE = 3,
	LORAWAN_EVENT_VHF_BURST = 4
};

struct lorawan_event_data {
	enum lorawan_event_type type;
	union data {
		struct lorawan_message msg;
		struct lorawan_cmd cmd;
	} data;
};

/* Define event que to hold 8 lorawan events data */
K_MSGQ_DEFINE(event_que, sizeof(struct lorawan_event_data), 8, 4);

/* Define message que to hold 4 lorawan messages */
K_MSGQ_DEFINE(msg_que, sizeof(struct lorawan_event_data), 4, 4);

enum lorawan_state {
	LORAWAN_SETUP = 0,
	LORAWAN_INIT = 1,
	LORAWAN_ENGINE = 2,
	LORAWAN_SUSPENDED = 3
};

enum lorawan_state state = LORAWAN_SETUP;

/* ---------------- Function declarations ---------------- */
/* LoRaWAN callbacks */
static void on_modem_reset(uint16_t reset_count);
static void on_modem_network_joined(void);
static void on_modem_join_fail(void);
static void on_modem_tx_done(smtc_modem_event_txdone_status_t status);
static void on_modem_down_data(int8_t rssi, int8_t snr,
			       smtc_modem_event_downdata_window_t rx_window, uint8_t port,
			       const uint8_t *payload, uint8_t size);

/* CUSTOM STORAGE IMPLEMENTATION */
static void context_store(const uint8_t ctx_id, const uint8_t *buffer, const uint32_t size);
static void context_restore(const uint8_t ctx_id, uint8_t *buffer, const uint32_t size);

/* Downlink external handler function pointer */
lorawan_recv_handler_t prv_downlink_data_handler = NULL;

/* LoRaWAN thread loop */
static void lorawan_main_loop(void);

/* Create LoRaWAN thread */
K_THREAD_DEFINE(lorawan_thread_id, LORAWAN_STACK_SIZE, lorawan_main_loop, NULL, NULL, NULL,
		LORAWAN_PRIORITY, 0, 0);

/*LoRaWAN configuration */
static struct smtc_app_lorawan_cfg lorawan_cfg = {
	.use_chip_eui_as_dev_eui = true,
	.join_eui = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	.app_key = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		    0x00, 0x00, 0x00},
	.class = SMTC_MODEM_CLASS_A,
	.region = SMTC_MODEM_REGION_EU_868,
};

static struct smtc_app_event_callbacks event_callbacks = {
	.reset = on_modem_reset,
	.joined = on_modem_network_joined,
	.join_fail = on_modem_join_fail,
	.tx_done = on_modem_tx_done,
	.down_data = on_modem_down_data,
};

/* This is required to implement the custom storage. */
struct smtc_app_env_callbacks env_callbacks = {
	/* The other callbacks are not required */
	.context_store = context_store,
	.context_restore = context_restore,
};

/* lr11xx radio context and its use in the ralf layer */
const ralf_t modem_radio = RALF_LR11XX_INSTANTIATE(DEVICE_DT_GET(DT_NODELABEL(lr11xx)));

/* Joining status */
static bool lorawan_joining = false;

/* Join fail counter */
static int lorawan_join_failed_counter = 0;

/* lr11xx enabled flag */
static bool lorawan_enabled = false;

/* wait on TX done */
static bool lorawan_wait_tx_done = false;

static uint8_t tx_max_payload = LORAWAN_MAX_BUF_SIZE;

static bool prv_vhf_burst_queued = false;

/**
 * @brief Put new event in event que
 *
 * @param[in] msg new event
 */
static void prv_put_event_in_que(struct lorawan_event_data msg)
{
	/* Put event command in que */
	while (k_msgq_put(&event_que, &msg, K_NO_WAIT) != 0) {
		/* command queue is full: purge old data & try again */
		LOG_WRN("Message que is full, remove oldest message!");
		struct lorawan_event_data tmp_msg;
		k_msgq_get(&event_que, &tmp_msg, K_NO_WAIT);
	}
}

/**
 * @brief Put waiting to send message in message que
 *
 * @param[in] msg message
 */
static void prv_put_msg_in_que(struct lorawan_event_data msg)
{
	/* Put massage in que */
	while (k_msgq_put(&msg_que, &msg, K_NO_WAIT) != 0) {
		/* message queue is full: purge old data & try again */
		LOG_WRN("Message que is full, remove oldest message!");
		struct lorawan_event_data tmp_msg;
		k_msgq_get(&msg_que, &tmp_msg, K_NO_WAIT);
	}
}

/**
 * @brief   Send an application frame on LoRaWAN port defined by LORAWAN_APP_PORT
 *
 * This function checks if we are allowed to send (due to duty cycle limitations).
 * It also checks if we are allowed to send the number of bytes we are sending. If not, an empty
 * uplink is sent in order to flush mac commands.
 *
 * @param [in] buffer     Buffer containing the LoRaWAN buffer
 * @param [in] length     Payload length
 * @param [in] confirmed  Send a confirmed or unconfirmed uplink [false : unconfirmed / true :
 * confirmed]
 */
static int send_frame(const uint8_t *buffer, uint8_t length, uint8_t port, bool tx_confirmed)
{
	int32_t duty_cycle;

	/* Check if duty cycle is available */
	int err = smtc_modem_get_duty_cycle_status(&duty_cycle);
	if (err) {
		LOG_ERR("ERR code: %d. Failed to get duty cycle", err);
		return err;
	}
	while (duty_cycle < 0) {
		LOG_WRN("Duty-cycle limitation - next possible uplink in %d ms", duty_cycle);
		k_sleep(K_MSEC(-1 * duty_cycle));
	}

	err = smtc_modem_get_next_tx_max_payload(STACK_ID, &tx_max_payload);
	if (err) {
		LOG_ERR("ERR code: %d. Failed to get max tx payload", err);
		return err;
	}

	if (length > tx_max_payload) {
		LOG_WRN("Not enough space in buffer - requesting empty uplink to flush MAC "
			"commands %d/%d",
			length, tx_max_payload);
		err = smtc_modem_request_empty_uplink(STACK_ID, true, port, tx_confirmed);
		if (err) {
			LOG_ERR("ERR code: %d. Empty uplink request failed", err);
			return err;
		}
	} else {
		LOG_INF("Requesting uplink");
		err = smtc_modem_request_uplink(STACK_ID, port, tx_confirmed, buffer, length);
		if (err) {
			LOG_ERR("ERR code: %d. Uplink request failed", err);
			return err;
		}
	}

	/* Wait for send to be completed before handling new events */
	lorawan_wait_tx_done = true;

	return 0;
}

static void prv_lorawan_reschedule_messages(void)
{
	struct lorawan_event_data msg;

	while (!k_msgq_get(&msg_que, &msg, K_NO_WAIT)) {
		LOG_DBG("Message in que to send! Reschedule.");
		prv_put_event_in_que(msg);
	}
	LOG_DBG("No more messages to reschedule!");
}

__unused static void lorawan_update_cfg(uint32_t id)
{
	/* EvaTODO */
	/*
	switch (id) {
	case LORAWAN_APP_KEY: {
		memcpy(lorawan_cfg.app_key,
		       (uint8_t *)user_settings_get_with_id(LORAWAN_APP_KEY, NULL),
		       sizeof(lorawan_cfg.app_key));
		break;
	}
	case LORAWAN_JOIN_EUI: {
		memcpy(lorawan_cfg.join_eui,
		       (uint8_t *)user_settings_get_with_id(LORAWAN_JOIN_EUI, NULL),
		       sizeof(lorawan_cfg.join_eui));
		break;
	}
	case LORAWAN_REGION: {
		memcpy(&lorawan_cfg.region,
		       (uint8_t *)user_settings_get_with_id(LORAWAN_REGION, NULL),
		       sizeof(lorawan_cfg.region));
		break;
	}
	case LORAWAN_CLASS: {
		memcpy(&lorawan_cfg.class,
		       (uint8_t *)user_settings_get_with_id(LORAWAN_CLASS, NULL),
		       sizeof(lorawan_cfg.class));
		break;
	}
	default:
		break;
	}
	*/
}

static uint8_t prv_lorawan_validate_adr_setting(uint8_t adr)
{
	if (lorawan_cfg.region == SMTC_MODEM_REGION_EU_868) {
		if (adr > 7) {
			LOG_WRN("ADR setting of: %d not compatible with EU region: %d! Change to "
				"1!",
				adr, lorawan_cfg.region);
			return 1;
		}
	} else if (lorawan_cfg.region == SMTC_MODEM_REGION_US_915) {
		if ((adr == 0) || (adr > 4 && adr < 8) || (adr > 13)) {
			LOG_WRN("ADR setting of: %d not compatible with US region: %d! Change to "
				"1!",
				adr, lorawan_cfg.region);
			return 1;
		}
	}

	return adr;
}

static void prv_lorawan_handle_events(k_timeout_t timeout)
{
	if (lorawan_is_joined()) {
		sys_operation.lr_join = 0;
	} else {
		sys_operation.lr_join = -EIO;
	}
	/* If we are waiting for TX completed do not handle new events */
	if (lorawan_wait_tx_done) {
		k_sleep(timeout);
		return;
	}

	struct lorawan_event_data event;
	int err = k_msgq_get(&event_que, &event, timeout);
	if (err) {
		return;
	}

	switch (event.type) {
	case LORAWAN_EVENT_MESSAGE: {

		/* Check if we are in the run engine state */
		if (state != LORAWAN_ENGINE) {
			LOG_WRN("LoraWAN engine not active, cannot send message, store for "
				"later.");
			/* Put in message que */
			prv_put_msg_in_que(event);
			break;
		}

		/* Check if lorawan module is enabled */
		if (!lorawan_enabled) {
			LOG_WRN("LoraWAN module is not enabled, discard message!");
			break;
		}

		/* Check if we are joined */
		if (lorawan_is_joined() == true) {
			LOG_DBG("LoraWAN joined, send all messages in the buffer.");
			err = send_frame(event.data.msg.data, event.data.msg.len,
					 event.data.msg.port, event.data.msg.confirmed);
			if (err) {
				LOG_ERR("Failed to send message!");
			}
		} else if (event.data.msg.join_attempt) {
			/* Check if we are not in the middle of the join process */
			if (!lorawan_joining) {
				/*  start join sequence */
				LOG_WRN("Start new join sequence!");
				err = smtc_modem_join_network(STACK_ID);
				if (err) {
					LOG_ERR("ERR code: %d. Failed to start join "
						"sequence",
						err);
				}
				lorawan_joining = true;
			}
			LOG_WRN("LoraWAN not joined yet, attempt join and store message "
				"for later!");
			/* Put in message que */
			prv_put_msg_in_que(event);
		} else {
			LOG_WRN("Device has not joined a network yet, try again "
				"later and store message for later.");
			/* Put in message que */
			prv_put_msg_in_que(event);
		}

		break;
	}
	case LORAWAN_EVENT_CMD: {
		switch (event.data.cmd.cmd) {
		case LORAWAN_CMD_LEAVE_NETWORK: {
			LOG_WRN("Leave modem network.");
			int err = smtc_modem_leave_network(STACK_ID);
			if (err) {
				LOG_ERR("ERR code: %d. Failed to leave network", err);
			}
			tx_max_payload = LORAWAN_MAX_BUF_SIZE;
			lorawan_joining = false;
			lorawan_join_failed_counter = 0;
			break;
		}
		case LORAWAN_CMD_START: {
			state = LORAWAN_INIT;
			break;
		}
		case LORAWAN_CMD_RESET: {
			/* Leave network */
			LOG_WRN("Leave modem network.");
			int err = smtc_modem_leave_network(STACK_ID);
			if (err) {
				LOG_ERR("ERR code: %d. Failed to leave network", err);
			}
			/* Go to init state */
			state = LORAWAN_INIT;
			break;
		}
		case LORAWAN_CMD_GNSS_AUTONOMOUS: {
			if (!lorawan_enabled) {
				LOG_WRN("LoRaWAN module is not enabled, cannot perform autonomous "
					"gnss scan.");
				break;
			}

			LOG_INF("Try to perform autonomous gnss scan.");

			int err = smtc_modem_suspend_before_user_radio_access();
			if (err) {
				LOG_ERR("Failed to suspend modem before attempting gnss "
					"scan.");
			} else {
				state = LORAWAN_SUSPENDED;
			}

			gnss_scan_autonomous(modem_radio.ral.context, 10,
					     LR11XX_GNSS_GPS_MASK | LR11XX_GNSS_BEIDOU_MASK);

			smtc_modem_hal_irq_reset_radio_irq();

			err = smtc_modem_resume_after_user_radio_access();
			if (err) {
				LOG_ERR("Failed to resume modem after attempting gnss "
					"scan.");
			} else {
				state = LORAWAN_ENGINE;
			}
			break;
		}
		case LORAWAN_CMD_GNSS_ASSISTED: {
			if (!lorawan_enabled) {
				LOG_WRN("LoRaWAN module is not enabled, cannot perform assisted "
					"gnss scan.");
				break;
			}

			LOG_INF("Try to perform autonomous gnss scan.");

			int err = smtc_modem_suspend_before_user_radio_access();
			if (err) {
				LOG_ERR("Failed to suspend modem before attempting gnss "
					"scan.");
			} else {
				state = LORAWAN_SUSPENDED;
			}

			gnss_scan_assisted(modem_radio.ral.context, 10,
					   LR11XX_GNSS_GPS_MASK | LR11XX_GNSS_BEIDOU_MASK);

			smtc_modem_hal_irq_reset_radio_irq();

			err = smtc_modem_resume_after_user_radio_access();
			if (err) {
				LOG_ERR("Failed to resume modem after attempting gnss "
					"scan.");
			} else {
				state = LORAWAN_ENGINE;
			}
			break;
		}
		case LORAWAN_CMD_ALMANAC_UPDATE: {
			if (!lorawan_enabled) {
				LOG_WRN("LoRaWAN module is not enabled, cannot update almanac.");
				break;
			}

			LOG_INF("Try to update almanac.");
			int err = smtc_modem_suspend_before_user_radio_access();
			if (err) {
				LOG_ERR("Failed to suspend modem before attempting gnss "
					"scan.");
			} else {
				state = LORAWAN_SUSPENDED;
			}

			almanac_update(modem_radio.ral.context);

			smtc_modem_hal_irq_reset_radio_irq();

			err = smtc_modem_resume_after_user_radio_access();
			if (err) {
				LOG_ERR("Failed to resume modem after attempting gnss "
					"scan.");
			} else {
				state = LORAWAN_ENGINE;
			}
			break;
		}
		case LORAWAN_CMD_WIFI_SCAN: {
#ifdef CONFIG_LR_WIFI_SCAN
			if (!lorawan_enabled) {
				LOG_WRN("LoRaWAN module is not enabled, cannot perform wifi scan.");
				break;
			}

			LOG_INF("Try to perform wifi scan.");

			int err = smtc_modem_suspend_before_user_radio_access();
			if (err) {
				LOG_ERR("Failed to suspend modem before attempting wifi "
					"scan.");
			} else {
				state = LORAWAN_SUSPENDED;
			}

#ifdef CONFIG_RF_FRONT_END_MODULE
			err = rf_front_end_module_set_mode(RF_FRONT_END_MODE_RX_LNA);
			if (err) {
				LOG_ERR("Failed to set RF front end module to RX LNA mode.");
			}
#endif /* CONFIG_RF_FRONT_END_MODULE */

			wifi_scan_default(modem_radio.ral.context);

#ifdef CONFIG_RF_FRONT_END_MODULE
			err = rf_front_end_module_set_mode(RF_FRONT_END_MODE_SLEEP);
			if (err) {
				LOG_ERR("Failed to set RF front end module to sleep mode.");
			}
#endif /* CONFIG_RF_FRONT_END_MODULE */

			smtc_modem_hal_irq_reset_radio_irq();

			err = smtc_modem_resume_after_user_radio_access();
			if (err) {
				LOG_ERR("Failed to resume modem after attempting wifi "
					"scan.");
			} else {
				state = LORAWAN_ENGINE;
			}
#else
			LOG_WRN("Wifi scan not supported!");
#endif /* CONFIG_LR_WIFI_SCAN */
			break;
		}
		case LORAWAN_CMD_MODEM_SUSPEND: {

			if (state != LORAWAN_ENGINE) {
				LOG_WRN("Modem already suspended!");
				lorawan_enabled = false;
				break;
			}

			int err = smtc_modem_suspend_before_user_radio_access();
			if (err) {
				LOG_ERR("Failed to suspend modem.");
			} else {
				state = LORAWAN_SUSPENDED;
				lorawan_enabled = false;
				k_sem_give(&lora_chip_suspended_sem);
			}
			break;
		}
		case LORAWAN_CMD_MODEM_RESUME: {
			if (state == LORAWAN_SUSPENDED) {
				/* configure LoRaWAN modem */
				smtc_modem_hal_irq_reset_radio_irq();
				int err = smtc_modem_resume_after_user_radio_access();
				if (err) {
					LOG_ERR("Failed to resume modem.");
				} else {
					state = LORAWAN_ENGINE;
					lorawan_enabled = true;
					/* Retake lora chip post suspension. Wait for max response
					 * time divided by 5 */
					k_sem_take(
						&lora_chip_suspended_sem,
						K_SECONDS((int)(THREAD_LR_GPS_MAX_RESPONSE / 5)));
				}
			} else if (state == LORAWAN_ENGINE) {
				LOG_WRN("Modem already running!");
				lorawan_enabled = true;
			}
			break;
		}
		default: {
			LOG_WRN("Command not supported: %d!", event.data.cmd.cmd);
			break;
		}
		}
		break;
	}
	case LORAWAN_EVENT_SETTING: {
		/* Not implemented yet EvaTODO */
		break;
	}
	case LORAWAN_EVENT_S_BAND_MESSAGE: {
#ifdef CONFIG_LR_S_BAND
		if (Main_settings.s_band_send_mode->def_val == LR_S_BAND_SEND_WITH_FHSS ||
		    Main_settings.s_band_send_mode->def_val == LR_S_BAND_SEND_BOTH) {
			LOG_INF("Try to send message over S-Band.");
			int err = smtc_modem_suspend_before_user_radio_access();
			if (err) {
				LOG_ERR("Failed to suspend modem before attempting s-band send "
					"scan.");
			} else {
				state = LORAWAN_SUSPENDED;
			}

			err = lr_s_band_send_message_fhss(modem_radio.ral.context,
							  event.data.msg.data, event.data.msg.len,
							  event.data.msg.port);
			if (err) {
				LOG_ERR("Failed to send message over S-Band.");
			}

			smtc_modem_hal_irq_reset_radio_irq();

			err = smtc_modem_resume_after_user_radio_access();
			if (err) {
				LOG_ERR("Failed to resume modem after attempting s-band send "
					"scan.");
			} else {
				state = LORAWAN_ENGINE;
			}
		}
		/* If both send mods selected, sleep for increased separability */
		if (Main_settings.s_band_send_mode->def_val == LR_S_BAND_SEND_BOTH) {
			k_sleep(K_MSEC(100));
		}
		if (Main_settings.s_band_send_mode->def_val == LR_S_BAND_SEND_WITHOUT_FHSS ||
		    Main_settings.s_band_send_mode->def_val == LR_S_BAND_SEND_BOTH) {
			LOG_INF("Try to send message over S-Band without fhss.");
			int err = smtc_modem_suspend_before_user_radio_access();
			if (err) {
				LOG_ERR("Failed to suspend modem before attempting s-band "
					"send "
					"scan.");
			} else {
				state = LORAWAN_SUSPENDED;
			}

			err = lr_s_band_send_message_lr(modem_radio.ral.context,
							event.data.msg.data, event.data.msg.len,
							event.data.msg.port);
			if (err) {
				LOG_ERR("Failed to send message over S-Band without fhss.");
			}

			smtc_modem_hal_irq_reset_radio_irq();

			err = smtc_modem_resume_after_user_radio_access();
			if (err) {
				LOG_ERR("Failed to resume modem after attempting s-band "
					"without "
					"fhss send "
					"scan.");
			} else {
				state = LORAWAN_ENGINE;
			}
		}

#else  /* CONFIG_LR_S_BAND */
		LOG_WRN("S-Band not supported!");
#endif /* CONFIG_LR_S_BAND */
		break;
	}
	case LORAWAN_EVENT_VHF_BURST: {
		for (int i = 0; i < Main_settings.vhf_num_of_packets_per_burst->def_val; i++) {
			/* Suspend LoRaWAN modem operation */
			int err = smtc_modem_suspend_before_user_radio_access();
			if (err) {
				LOG_ERR("Failed to suspend modem before attempting RF scan.");
			} else {
				state = LORAWAN_SUSPENDED;
			}

			/* Perform VHF burst */
			vhf_send_burst();

			/* Resume LoRaWAN modem operation */
			smtc_modem_hal_irq_reset_radio_irq();
			err = smtc_modem_resume_after_user_radio_access();
			if (err) {
				LOG_ERR("Failed to resume modem after VHF burst.");
			} else {
				state = LORAWAN_ENGINE;
			}
			k_sleep(K_MSEC(Main_settings.vhf_time_between_packets_ms->def_val));
		}
		prv_vhf_burst_queued = false;
		break;
	}
	default: {
		LOG_WRN("Event type not supported: %d!", event.type);
		break;
	}
	}
}

/**
 * @brief LoRaWAN main process loop.
 *
 */
static void lorawan_main_loop(void)
{
	uint32_t sleep_time_ms = 0;

	while (true) {
		switch (state) {
		case LORAWAN_SETUP: {
			/* If RF front end module is enabled, initialize it */
#ifdef CONFIG_RF_FRONT_END_MODULE
			rf_front_end_module_init();
#endif /* CONFIG_RF_FRONT_END_MODULE */
			/* Forever wait for start command */
			LOG_INF("LoRaWAN module setup");
			prv_lorawan_handle_events(K_FOREVER);
			break;
		}
		case LORAWAN_INIT: {

			LOG_INF("LoRaWAN module init");
			/* Reset prv variables */
			lorawan_join_failed_counter = 0;
			lorawan_joining = false;
			tx_max_payload = LORAWAN_MAX_BUF_SIZE;

			/* configure LoRaWAN modem */
			smtc_app_init(&modem_radio, &event_callbacks, &env_callbacks);
			smtc_app_display_versions();

			modem_set_duty_cycle_disabled_by_host(true);

			/* Start running smtc engine */
			lorawan_enabled = true;
			state = LORAWAN_ENGINE;
			break;
		}
		case LORAWAN_ENGINE: {

			/* Execute modem runtime, this function must be recalled in
			 * sleep_time_ms or sooner. The fist call to smtc_modem_run_engine
			 * will trigger the reset callback. We set our LoRaWAN configuration
			 * then and start the join process - see reset().
			 */
			sleep_time_ms = smtc_modem_run_engine();
			LOG_DBG("Sleeping for %d ms", sleep_time_ms);

			/* Sleep and wait for the new event */
			prv_lorawan_handle_events(K_MSEC(sleep_time_ms));
			break;
		}
		case LORAWAN_SUSPENDED: {
			/* Sleep and wait for the new event */
			prv_lorawan_handle_events(K_FOREVER);
			break;
		}
		}
	}
}
/**
 * @brief Reset event callback
 *
 * @param [in] reset_count reset counter from the modem
 */
static void on_modem_reset(uint16_t reset_count)
{
	int err = 0;

	/* Prevent smtc modem info from being sent */
	err = smtc_modem_dm_set_info_interval(0, 0);
	if (err) {
		LOG_ERR("ERR code: %d. Failed to configure info message sending interval.", err);
		return;
	}

	/* Reset failed join counter */
	lorawan_join_failed_counter = 0;

	/* Increase TX power */
	err = smtc_modem_set_tx_power_offset_db(STACK_ID, LORAWAN_TX_POWER_OFFSET);
	if (err) {
		LOG_ERR("ERR code: %d. Failed to set TX power offset to: %d dB", err,
			LORAWAN_TX_POWER_OFFSET);
	}

	/* configure lorawan parameters after reset */
	err = smtc_app_configure_lorawan_params(STACK_ID, &lorawan_cfg);
	if (err) {
		LOG_ERR("ERR code: %d. Failed to configure lorawan params", err);
		return;
	}

	/*  start join sequence */
	err = smtc_modem_join_network(STACK_ID);
	if (err) {
		LOG_ERR("ERR code: %d. Failed to start join sequence", err);
		return;
	}
	lorawan_joining = true;
}

/**
 * @brief Network Joined event callback
 */
static void on_modem_network_joined(void)
{
	int err = 0;
	/* Set adr profile */
	if (prv_adr_profile == SMTC_MODEM_ADR_PROFILE_CUSTOM) {
		err = smtc_modem_adr_set_profile(STACK_ID, prv_adr_profile, prv_adr_custom_list);
	} else {
		err = smtc_modem_adr_set_profile(STACK_ID, prv_adr_profile, NULL);
	}

	if (err) {
		LOG_ERR("ERR code: %d. Failed to set ADR profile", err);
	}

	LOG_INF("Set ADR to custom value: %d", prv_adr_custom_list[0]);

	/* Terminate joining process */
	lorawan_joining = false;

	/* Reset failed join counter */
	lorawan_join_failed_counter = 0;

	/* If any messages are waiting to be send, send them */
	LOG_INF("We are joined, reschedule all waiting messages!");
	sys_operation.lr_join = 0;
	prv_lorawan_reschedule_messages();
}

/**
 * @brief Network joi failed event callback.
 * Increase counter of failed joined attempts. Leave network after certain number of
 * attempts, otherwise we will try to join forever?
 *
 */
static void on_modem_join_fail(void)
{
	lorawan_join_failed_counter++;
	LOG_WRN("Failed join attempt: %d", lorawan_join_failed_counter);
	sys_operation.lr_join = -EIO;

	if (lorawan_join_failed_counter >= LORAWAN_JOIN_ATTEMPTS) {
		int err = smtc_modem_leave_network(STACK_ID);
		if (err) {
			LOG_ERR("ERR code: %d. Failed to leave network", err);
		}
		LOG_WRN("Leave network and stop joining!");
		lorawan_joining = false;
		lorawan_join_failed_counter = 0;
	}
}

/**
 * @brief Tx done event callback
 *
 * @param [in] status tx done status @ref smtc_modem_event_txdone_status_t
 */
static void on_modem_tx_done(smtc_modem_event_txdone_status_t status)
{
	if (status == SMTC_MODEM_EVENT_TXDONE_NOT_SENT) {
		LOG_ERR("Uplink was not sent");
	} else if (status == SMTC_MODEM_EVENT_TXDONE_SENT) {
		LOG_INF("Uplink sent (not confirmed)");
	} else if (status == SMTC_MODEM_EVENT_TXDONE_CONFIRMED) {
		LOG_INF("Uplink sent (confirmed)");
	}

	lorawan_wait_tx_done = false;
}

/**
 * @brief Downlink data event callback.
 * EvaTODO: Implement message parsing and processing - we still need to specify what will be
 * send in downlink, formatting, processing, etc
 *
 * @param [in] rssi       RSSI in signed value in dBm + 64
 * @param [in] snr        SNR signed value in 0.25 dB steps
 * @param [in] rx_window  RX window
 * @param [in] port       LoRaWAN port
 * @param [in] payload    Received buffer pointer
 * @param [in] size       Received buffer size
 */
static void on_modem_down_data(int8_t rssi, int8_t snr,
			       smtc_modem_event_downdata_window_t rx_window, uint8_t port,
			       const uint8_t *payload, uint8_t size)
{
	LOG_INF("EVENT: DOWNDATA");

	LOG_INF("RSSI: %d", rssi);
	LOG_INF("SNR: %d", snr);
	LOG_INF("PORT: %d", port);
	LOG_INF("Payload len: %d", size);
	LOG_HEXDUMP_INF(payload, size, "Payload buffer:");

	/* EvaTODO call handler */
	if (prv_downlink_data_handler) {
		prv_downlink_data_handler(payload, size, port);
	}

	/* Get next max TX size */
	smtc_modem_get_next_tx_max_payload(STACK_ID, &tx_max_payload);
}

/**
 * @brief Store context. The application is responsible for storing the context
 * persistently.
 *
 * @param[in] ctx_id The ID of the context to store. Each ID must be stored separately.
 * @param[in] buffer The buffer containing the context to store.
 * @param[in] size The size of the buffer, in bytes.
 */
static void context_store(const uint8_t ctx_id, const uint8_t *buffer, const uint32_t size)
{
	LOG_WRN("Context store. ID: %d, size: %d", ctx_id, size);
	LOG_HEXDUMP_WRN(buffer, size, "Context buffer:");

	nvs_storage_write(STORAGE_lorawan_ctx_id0 + ctx_id, buffer, size);
}

/**
 * @brief Restore context from persistent storage.
 *
 * @param[in] ctx_id The ID of the context to restore.
 * @param[in] buffer The buffer to load the context into.
 * @param[in] size The size of the buffer, in bytes.
 */
static void context_restore(const uint8_t ctx_id, uint8_t *buffer, const uint32_t size)
{
	LOG_WRN("Context restore. ID: %d, size: %d", ctx_id, size);

	nvs_storage_read(STORAGE_lorawan_ctx_id0 + ctx_id, buffer, size);
}

/**
 * @brief Get the smtc adr profile enumeration from the lorawan adr profile enumeration.
 *
 * @param[in] adr_profile lorawan adr profile enumeration
 * @return smtc_modem_adr_profile_t
 */
static smtc_modem_adr_profile_t prv_get_smtc_adr_profile(enum lorawan_adr_profile adr_profile)
{
	switch (adr_profile) {
	case ADR_PROFILE_NETWORK_CONTROLLED:
		return SMTC_MODEM_ADR_PROFILE_NETWORK_CONTROLLED;
	case ADR_PROFILE_MOBILE_LONG_RANGE:
		return SMTC_MODEM_ADR_PROFILE_MOBILE_LONG_RANGE;
	case ADR_PROFILE_MOBILE_LOW_POWER:
		return SMTC_MODEM_ADR_PROFILE_MOBILE_LOW_POWER;
	case ADR_PROFILE_CUSTOM:
		return SMTC_MODEM_ADR_PROFILE_CUSTOM;
	default:
		return SMTC_MODEM_ADR_PROFILE_CUSTOM;
	}
}

void lorawan_recv_handler_register(lorawan_recv_handler_t handler)
{
	prv_downlink_data_handler = handler;
}

void lorawan_set_configuration(uint8_t join_eui[8], uint8_t app_key[16], uint8_t region,
			       uint8_t adr, enum lorawan_adr_profile adr_profile)
{
	/* Update settings */
	memcpy(lorawan_cfg.join_eui, join_eui, sizeof(lorawan_cfg.join_eui));
	memcpy(lorawan_cfg.app_key, app_key, sizeof(lorawan_cfg.app_key));
	lorawan_cfg.region = region;

	/* Set ADR profile */
	prv_adr_profile = prv_get_smtc_adr_profile(adr_profile);

	/* Copy ADR */
	uint8_t set_adr = prv_lorawan_validate_adr_setting(adr);
	for (int i = 0; i < sizeof(prv_adr_custom_list); i++) {
		prv_adr_custom_list[i] = set_adr;
	}
}

void lorawan_start(void)
{
	/* Create start event */
	LOG_DBG("Got LoraWAN start command, create start event.");
	struct lorawan_event_data event;
	event.type = LORAWAN_EVENT_CMD;

	event.data.cmd.cmd = LORAWAN_CMD_START;

	/* Put event command que */
	prv_put_event_in_que(event);
}

void lorawan_reset(void)
{
	/* Create reset event */
	struct lorawan_event_data event;
	event.type = LORAWAN_EVENT_CMD;

	event.data.cmd.cmd = LORAWAN_CMD_RESET;

	/* Put event command que */
	prv_put_event_in_que(event);
}

bool lorawan_is_joined(void)
{
	uint32_t status = 0;
	smtc_modem_get_status(STACK_ID, &status);
	return (status & SMTC_MODEM_STATUS_JOINED) == SMTC_MODEM_STATUS_JOINED;
}

bool lorawan_joining_status(void)
{
	return lorawan_joining;
}

int lorawan_send_message(uint8_t *payload, uint8_t payload_length, uint8_t send_port,
			 bool confirmed, bool join_attempt)
{
	int err = 0;

	if (payload_length < 0 || payload_length > LORAWAN_MAX_BUF_SIZE) {
		LOG_ERR("Message to long to send!");
		return -EMSGSIZE;
	}

	/* Create new message */
	struct lorawan_event_data event;
	event.type = LORAWAN_EVENT_MESSAGE;

	memcpy(event.data.msg.data, payload, payload_length);
	event.data.msg.len = payload_length;
	event.data.msg.port = send_port;
	event.data.msg.confirmed = confirmed;
	event.data.msg.join_attempt = join_attempt;

	/* Put message in send que */
	prv_put_event_in_que(event);

	return err;
}

int lorawan_get_max_payload(void)
{
	return tx_max_payload;
}

int lorawan_get_dev_eui(uint8_t dev_eui[8])
{
	int err = smtc_modem_get_deveui(STACK_ID, dev_eui);

	return err;
}

void lorawan_get_nwkkey(uint8_t nwkkey[16])
{
	memcpy(nwkkey, lorawan_cfg.app_key, sizeof(lorawan_cfg.app_key));
}

bool lorawan_is_enabled(void)
{
	return lorawan_enabled;
}

void lorawan_suspend(void)
{
	/* Create suspend event */
	struct lorawan_event_data event;
	event.type = LORAWAN_EVENT_CMD;

	event.data.cmd.cmd = LORAWAN_CMD_MODEM_SUSPEND;

	/* Put event command que */
	prv_put_event_in_que(event);
}

void lorawan_resume(void)
{
	/* Create resume event */
	struct lorawan_event_data event;
	event.type = LORAWAN_EVENT_CMD;

	event.data.cmd.cmd = LORAWAN_CMD_MODEM_RESUME;

	/* Put event command que */
	prv_put_event_in_que(event);
}

void lorawan_leave_network(void)
{
	/* Create leave network event */
	struct lorawan_event_data event;
	event.type = LORAWAN_EVENT_CMD;

	event.data.cmd.cmd = LORAWAN_CMD_LEAVE_NETWORK;

	/* Put event command que */
	prv_put_event_in_que(event);
}

void lorawan_gnss_scan_autonomous(void)
{
	/* Create leave network event */
	struct lorawan_event_data event;
	event.type = LORAWAN_EVENT_CMD;

	event.data.cmd.cmd = LORAWAN_CMD_GNSS_AUTONOMOUS;

	/* Put event command que */
	prv_put_event_in_que(event);
}

void lorawan_gnss_scan_assisted(int32_t lat, int32_t lon, uint32_t gps_time)
{

	/* Convert lat and lon to reference position struct */
	lr11xx_gnss_solver_assistance_position_t assistance_position;
	assistance_position.latitude = ((float)lat) / 10000000;
	assistance_position.longitude = ((float)lon) / 10000000;

	/* Save assistance position */
	gnss_scan_set_ref_position(assistance_position);

	/* Save reference gps time */
	gnss_scan_set_ref_gps_time(gps_time);

	/* Create leave network event */
	struct lorawan_event_data event;
	event.type = LORAWAN_EVENT_CMD;

	event.data.cmd.cmd = LORAWAN_CMD_GNSS_ASSISTED;

	/* Put event command que */
	prv_put_event_in_que(event);
}

void lorawan_update_almanac(void)
{
	/* Create update almanac event */
	struct lorawan_event_data event;
	event.type = LORAWAN_EVENT_CMD;

	event.data.cmd.cmd = LORAWAN_CMD_ALMANAC_UPDATE;

	/* Put event command que */
	prv_put_event_in_que(event);
}

void lorawan_wifi_scan(void)
{
	/* Create wifi scan event */
	struct lorawan_event_data event;
	event.type = LORAWAN_EVENT_CMD;

	event.data.cmd.cmd = LORAWAN_CMD_WIFI_SCAN;

	/* Put event command que */
	prv_put_event_in_que(event);
}

void lorawan_s_band_send(uint8_t *payload, uint8_t payload_length, uint8_t port,
			 uint8_t network_key[16], uint8_t app_key[16], uint8_t dev_addr[4])
{

	LOG_INF("Send message over S-Band.");
#ifdef CONFIG_LR_S_BAND
	lr_s_band_set_keys(network_key, app_key, dev_addr);
#endif /* CONFIG_LR_S_BAND */

	/* Create new message */
	struct lorawan_event_data event;
	event.type = LORAWAN_EVENT_S_BAND_MESSAGE;

	memcpy(event.data.msg.data, payload, payload_length);
	event.data.msg.len = payload_length;
	event.data.msg.port = port;

	LOG_WRN("Event put in que type: %d, len: %d port: %d", event.type, event.data.msg.len,
		event.data.msg.port);

	/* Put message in send que */
	prv_put_event_in_que(event);
}

void lorawan_vhf_burst(void)
{
	if (lorawan_enabled && lorawan_joining) {
		LOG_WRN("LoRaWAN module is in the process of joining, cannot perform VHF "
			"burst at the moment.");
		return;
	}

	/* Create new message */
	struct lorawan_event_data event;
	event.type = LORAWAN_EVENT_VHF_BURST;

	/* Check if VHF event is already in queue */
	if (prv_vhf_burst_queued) {
		LOG_WRN("VHF burst event already in queue, ignore new request.");
		return;
	}

	LOG_INF("VHF burst event put in queue");
	/* Put message in send que */
	prv_put_event_in_que(event);
	prv_vhf_burst_queued = true;
}

bool lorawan_get_vhf_burst_queued(void)
{
	return prv_vhf_burst_queued;
}
