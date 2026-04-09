/**
 * @file lr_s_band.h
 * @brief
 *
 * @copyright (c) 2023 Irnas. All rights reserved.
 *
 */

#ifndef LR_S_BAND_H
#define LR_S_BAND_H

#include <zephyr/kernel.h>

#define LR_S_BAND_TX_TIMEOUT_S 10

/**
 * @brief External callback for TX done event
 *
 */
typedef void (*lr_s_band_tx_done_handler_t)(int err);

enum lr_s_band_send_mode {
	LR_S_BAND_SEND_WITHOUT_FHSS = 0,
	LR_S_BAND_SEND_WITH_FHSS = 1,
	LR_S_BAND_SEND_BOTH = 2
};

/**
 * @brief Register external callback for TX done event
 *
 */
void lr_s_band_tx_done_handler_register(lr_s_band_tx_done_handler_t);

/**
 * @brief Send s-band message using FHSS.
 *
 * @param[in] context Lorawan modem radio context
 * @param[in] buffer data buffer
 * @param[in] buffer_len data buffer length
 * @param[in] port port number
 * @return int 0 on success, negative error code otherwise
 */
int lr_s_band_send_message_fhss(const void *context, uint8_t *buffer, uint8_t buffer_len,
				uint8_t port);

/**
 * @brief Send s-band message using standard LoRaWAN.
 *
 * @param[in] context Lorawan modem radio context
 * @param[in] buffer data buffer
 * @param[in] buffer_len data buffer length
 * @param[in] port port number
 * @return int 0 on success, negative error code otherwise
 */
int lr_s_band_send_message_lr(const void *context, uint8_t *buffer, uint8_t buffer_len,
			      uint8_t port);

/**
 * @brief Set keys for LR S Band to generate payload
 *
 * @param[in] network_key
 * @param[in] app_key
 * @param[in] dev_addr
 */
void lr_s_band_set_keys(uint8_t network_key[16], uint8_t app_key[16], uint8_t dev_addr[4]);

#endif /* LR_S_BAND_H */
