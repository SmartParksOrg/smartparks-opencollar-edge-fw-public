/** @file lp0.h
 *
 * @brief LP0 thread header file
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2025 Irnas.  All rights reserved.
 */

#ifndef LP0_H
#define LP0_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lr11xx_radio_types.h"
#include <stdbool.h>
#include <stdint.h>
#include <zephyr/device.h>

#include <lp0_common.h>

/* Max message length */
#define LP0_MAX_BUF_SIZE 242

struct lp0_message {
	uint8_t len;                    /* Message length */
	uint8_t port;                   /* Sending port */
	uint8_t data[LP0_MAX_BUF_SIZE]; /* Message */
	bool confirmed;                 /* Send message as confirmed */
	bool fhss;                      /* Use FHSS for sending */
};

struct lp0_data_packet {
	uint32_t timestamp; /* microsecond timestamp from radio */
	uint32_t freq_hz;   /* freq in Hz (e.g., 868100000) */

	uint8_t chan; /* channel */
	uint8_t rfch; /* RF channel */
	uint8_t stat; /* CRC status: 1 = OK, 255 = fail, 0 = no CRC */
	lr11xx_radio_pkt_type_t pkt_type;

	lr11xx_radio_lora_sf_t sf; /* LoRa spreading factor */
	lr11xx_radio_lora_bw_t bw; /* LoRa bandwidth */
	lr11xx_radio_lora_cr_t cr; /* LoRa coding rate */
	uint8_t ldro;              /* LoRa LDRO */

	int8_t rssi;
	int8_t lsnr_x10; /* lsnr * 10 */

	uint32_t gps_lon; /* last known GPS longitude */
	uint32_t gps_lat; /* last known GPS latitude */

	uint8_t data_size;              /* size of payload */
	uint8_t data[LP0_MAX_BUF_SIZE]; /* Payload data */
};

enum lp0_message_contents {
	LP0_PING = 0,
	LP0_PING_ACK = 2,
	LP0_START_OFFLOAD = 3,
	/* Types below is for testing only */
	LP0_FLASH_TEST_FILL_100_MESSAGES = 4,
};

enum lp0_cmd_type {
	LP0_CMD_DISABLE = 0,
	LP0_CMD_START = 1,
	LP0_CMD_RESET = 2,
	LP0_CMD_SUSPEND = 3,
	LP0_CMD_RESUME = 4,
	LP0_CMD_SET_MODE = 5,
	LP0_CMD_SEND_PING = 6,
	LP0_CMD_TEST_FLASH_FILL_100_MESSAGES = 7,
};

enum lp0_event_type {
	LP0_EVENT_MESSAGE = 0,
	LP0_EVENT_CMD = 1,
	LP0_EVENT_S_BAND_MESSAGE = 2,
};

struct lp0_cmd {
	enum lp0_cmd_type cmd;
};

struct lp0_event_data {
	enum lp0_event_type type; /* Event type */
	union data {
		struct lp0_message msg; /* Message to send */
		struct lp0_cmd cmd;     /* Command to execute */
	} data;
	enum lp0_mode mode; /* Switch to mode */
};

/**
 * @brief Start LP0 operations
 */
void lp0_start(void);

/**
 * @brief Disable LP0 operations
 */
void lp0_disable(void);

/**
 * @brief Reset LP0 operations
 */
void lp0_reset(void);

/**
 * @brief Suspend LP0 operations
 */
void lp0_suspend(void);

/**
 * @brief Resume LP0 operations
 */
void lp0_resume(void);

/**
 * @brief Set LP0 mode
 *
 * @param mode - enumerated mode to set
 */
void lp0_set_mode(enum lp0_mode mode);

/**
 * @brief configure LP0 with given settings and send message
 *
 * @param context - lora device context
 * @param payload - pointer to the payload data
 * @param len - length of the payload
 * @param port - port number
 * @param fhss - flag indicating if FHSS is used
 * @param freq_hz - frequency to send on in Hz
 * @param confirmed - flag indicating if the message is confirmed
 * @param lorawan_header - flag indicating if lorawan header should be added to the message
 * @param iq_setting - IQ setting to use for transmission
 *
 * @return int 0 on success, negative error code otherwise
 */
int lp0_prepare_and_send_message(const struct device *context, uint8_t *payload, uint8_t len,
				 uint8_t port, bool fhss, uint32_t freq_hz, bool confirmed,
				 bool lorawan_header, lr11xx_radio_lora_iq_t iq_setting);

/**
 * @brief Start LP0 receive operation with given settings
 *
 * @param context - lora device context
 * @param freq_hz - frequency to receive on
 * @param iq_setting - IQ setting to use for reception
 * @param timeout_ms - timeout for reception in milliseconds
 * @param continuous - if true, reception will be continuous until explicitly stopped, otherwise it
 * will stop after timeout
 *
 * @return int 0 on success, negative error code otherwise
 */
int lp0_start_receive(const struct device *context, uint32_t freq_hz,
		      lr11xx_radio_lora_iq_t iq_setting, uint32_t timeout_ms, bool continuous);

/**
 * @brief Get RX timeout in milliseconds based on current LP0 communication settings
 *
 * @return Calculated RX timeout duration in milliseconds
 */
uint32_t lp0_get_rx_timeout_ms(void);

/**
 * @brief Update LP0 settings from Main_settings
 */
void lp0_update_communication_settings(void);

/**
 * @brief Suspend LoRaWAN and wait for suspension confirmation via semaphore
 *
 * @param timeout_ms - Timeout in milliseconds to wait for suspension confirmation
 *
 * @return int 0 if successful, negative error code if timeout or failure
 */
int lp0_suspend_lorawan_and_wait_for_suspension(uint32_t timeout_ms);

/**
 * @brief Add message to LP0 send queue
 *
 * @param payload - pointer to the payload data
 * @param len - length of the payload
 * @param port - port number
 * @param fhss - flag indicating if FHSS is used
 * @param confirmed - flag indicating if the message is confirmed
 *
 * @return int 0 on success, negative error code otherwise
 */
int lp0_add_message_to_send_queue(uint8_t *payload, uint8_t len, uint8_t port, bool fhss,
				  bool confirmed);

/**
 * @brief Send command to LP0 thread
 *
 * @param cmd - command to send
 *
 * @return int 0 on success
 */
int lp0_send_command(enum lp0_cmd_type cmd);

/**
 * @brief Parse LP0 command message
 *
 * @param[in] message - pointer to the command message
 * @param[in] size - size of the command message
 */
void lp0_command_parser(const uint8_t *message, uint8_t size);

#ifdef __cplusplus
}
#endif

#endif /* LP0_H */
