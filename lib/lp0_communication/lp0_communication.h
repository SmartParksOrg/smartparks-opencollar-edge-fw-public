/** @file lp0_communication.h
 *
 * @brief Interface for lp0 communication operations
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2025 Irnas.  All rights reserved.
 */

#ifndef LP0_COMMUNICATION_H
#define LP0_COMMUNICATION_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lorawan_tools_common.h"
#include <lora_irq_handler.h>

#include <zephyr/kernel.h>
#include <lr11xx_lr_fhss_types.h>
#include <lr11xx_radio_types.h>
#include <lr11xx_system_types.h>

struct lp0_cfg {
	/* LoRaWAN authentication information:
	 * - Network Key
	 * - Application Key
	 * - Device Address
	 */
	struct lorawan_auth_info lora_auth_info;

	uint8_t lora_syncword;
	lr11xx_system_irq_mask_t irq_mask;
	lr11xx_radio_pkt_type_t pkt_type;
};

struct lp0_communication_params {
	/* Union of parameters for standard LoRa or FHSS */
	union {
		struct {
			lr11xx_radio_mod_params_lora_t mod_params;
			lr11xx_radio_pkt_params_lora_t pkt_params;
		} lr_standard;
		struct {
			lr11xx_lr_fhss_params_t fhss_params;
		} lr_fhss;
	};
};

struct lp0_callbacks {
	/**
	 * @brief External callback for TX done event
	 *
	 * NOTE: This function is called on every tx done event which
	 * includes errors (like timeout or similar).
	 *
	 * @param[in] err - error code, 0 if no error
	 */
	void (*tx_done)(int err);

	/**
	 * @brief External callback for RX done event
	 *
	 * This function is called on every rx done event which includes errors (like timeout
	 * or crc error). The error code is passed as first parameter, if no error occurred it is 0.
	 * The port, payload and length parameters are only valid if no error occurred. RX
	 * messages without a payload will have a length of 0.
	 *
	 * NOTE: pkt_info is a pointer to a structure containing additional packet status
	 * information. The actual type of the structure depends on the packet type (LoRa or GFSK).
	 * Can be NULL in case of TIMEOUT or other errors. pkt_type indicates the type of the
	 * packet (LoRa or GFSK).
	 *
	 * @param[in] err - error code, 0 if no error
	 * @param[in] port - port number of the received message
	 * @param[in] payload - pointer to the received payload data
	 * @param[in] len - pointer to the length of the received payload data
	 * @param[in] raw_payload - pointer to the raw received payload data (including headers)
	 * @param[in] raw_len - pointer to the length of the raw received payload data
	 * @param[in] pkt_type - type of the received packet (LoRa or GFSK)
	 * @param[in] pkt_info - pointer to packet status information structure
	 */
	void (*rx_done)(int err, uint8_t *port, uint8_t *payload, size_t *len, uint8_t *raw_payload,
			size_t *raw_len, lr11xx_radio_pkt_type_t pkt_type, void *pkt_info);
};

/**
 * @brief Initialize LP0 communication. Sets up configuration and callbacks. Can be called multiple
 * times.
 *
 * @param[in] context - device pointer to the LoRa chip implementation context
 * @param[in] params - pointer to the LP0 parameters. Must be valid until the next call to this
 * function.
 * @param[in] callbacks - pointer to the LP0 callbacks. Must be valid until the next call to this
 * function.
 *
 * @retval 0 on success
 * @retval -EINVAL if NULL pointer provided
 */
int lp0_init(const struct device *context, struct lp0_cfg *cfg, struct lp0_callbacks *callbacks);

/**
 * @brief Initialize LR11xx system for LP0 communication.
 *
 * @param[in] context - device pointer to the LoRa chip implementation context
 *
 * @retval 0 on success
 * @retval negative error code otherwise
 */
int lp0_lr11xx_system_init(const struct device *context);

/**
 * @brief Update LP0 callbacks. Can be called multiple times.
 *
 * @param[in] callbacks - pointer to the LP0 callbacks. Must be valid until the next call to this
 * function.
 *
 * @retval 0 on success
 * @retval -EINVAL if NULL pointer provided
 */
int lp0_update_callbacks(struct lp0_callbacks *callbacks);

/**
 * @brief Configure LP0 packet parameters for standard of FHSS sending. Use this to update the
 * parameters union to separate between standard and FHSS sending.
 *
 * @param[in] context - device pointer to the LoRa chip implementation context
 * @param[in] fhss - flag indicating if FHSS is used
 * @param[in] freq_hz - frequency in Hz
 * @param[in] params - pointer to the LP0 parameters. Must be valid until the next call to this
 * function.
 *
 * @retval 0 on success, negative error code otherwise
 * @retval -EINVAL if NULL pointer provided
 */
int lp0_configure(const struct device *context, bool fhss, uint32_t freq_hz,
		  struct lp0_communication_params *params);

/**
 * @brief Send LP0 message
 *
 * Sends a Lora message using the provided payload, port, and configuration parameters. Runs
 * asynchronously. On completion, the appropriate callback (tx_done / timeout) callback will be
 * invoked.
 *
 * @param[in] context - device pointer to the LoRa chip implementation context
 * @param[in] payload - pointer to the payload data
 * @param[in] len - length of the payload
 * @param[in] port - port number
 * @param[in] fhss - flag indicating if FHSS is used
 * @param[in] freq_hz - frequency in Hz
 * @param[in] confirmed - flag indicating if the message is confirmed
 * @param[in] lorawan_header - flag indicating if lorawan header is used or custom lp0 header
 * @param[in] iq_setting - IQ setting for LoRa packets (standard or inverted)
 *
 * @return int 0 on success, negative error code otherwise
 */
int lp0_send_message(const struct device *context, uint8_t *payload, uint8_t len, uint8_t port,
		     bool fhss, uint32_t freq_hz, bool confirmed, bool lorawan_header,
		     lr11xx_radio_lora_iq_t iq_setting);

/**
 * @brief Start receiving LP0 message.
 *
 * This function puts the LoRa chip into receive mode at the specified frequency and timeout
 * duration. Runs asynchronously. The device listens for only one incoming message. On
 * message reception or timeout, the respected callback is invoked.
 *
 * If the timeout_ms parameter is set to 0xFFFFFF, the device enters continuous receive mode, only
 * stoppable by calling lp0_stop_continuous_message_receive().
 *
 * @param[in] context - device pointer to the LoRa chip implementation context
 * @param[in] freq_hz - frequency in Hz on which to listen for messages
 * @param[in] iq_setting - IQ setting for LoRa packets (standard or inverted)
 * @param[in] timeout_ms - timeout in milliseconds for the receive operation
 *
 * @return int 0 on success, negative error code otherwise
 */
int lp0_start_message_receive(const struct device *context, uint32_t freq_hz,
			      lr11xx_radio_lora_iq_t iq_setting, uint32_t timeout_ms);

/**
 * @brief Stop continuous receiving LP0 message.
 *
 * If the LP0 is in continuous receive mode, this function stops the receive operation and sets the
 * task back to IDLE.
 *
 * @param[in] context - device pointer to the LoRa chip implementation context
 * @return int 0 on success, negative error code otherwise
 */
int lp0_stop_continuous_message_receive(const struct device *context);

#ifdef __cplusplus
}
#endif

#endif /* LP0_COMMUNICATION_H */
