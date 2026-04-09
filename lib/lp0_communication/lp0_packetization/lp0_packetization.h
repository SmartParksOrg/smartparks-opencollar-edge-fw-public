/** @file lp0_packetization.h
 *
 * @brief Interface for lp0 packetization operations
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2025 Irnas.  All rights reserved.
 */

#ifndef LP0_PACKETIZATION_H
#define LP0_PACKETIZATION_H

#ifdef __cplusplus
extern "C" {
#endif

#include <lr11xx_lr_fhss_types.h>
#include <lr11xx_radio_types.h>

#include <lp0_communication.h>

#include <zephyr/kernel.h>

#define LP0_MAX_PAYLOAD_LENGTH       255
#define LP0_CUSTOM_HEADER_SYNC_BYTES 0x4269

struct lp0_payload_data {
	uint8_t buffer_tx[LP0_MAX_PAYLOAD_LENGTH];
	size_t buffer_len;

	uint32_t frame_counter;
};

/**
 * @brief Packetize message for lp0 transmission
 *
 * @param[in] payload data buffer
 * @param[in] len data buffer length
 * @param[in] port port number
 * @param[in] fhss true to use FHSS, false for standard LoRa
 * @param[out] packet - pointer to payload data structure to hold the packetized data
 * @param[in] params - pointer to lp0 params structure with keys and other parameters
 * @param[in] config - pointer to lp0 configuration structure with authentication keys
 * @param[in] confirmed - true for confirmed uplink, false for unconfirmed uplink
 * @param[in] lorawan_header - true to use lorawan header, false to use custom lp0 header
 * @param[in] direction - link direction (uplink or downlink)
 *
 * @return int 0 on success, negative error code otherwise
 */
int lp0_packetize_message(uint8_t *payload, uint8_t len, uint8_t port, bool fhss,
			  struct lp0_payload_data *packet, struct lp0_communication_params *params,
			  struct lp0_cfg *config, bool confirmed, bool lorawan_header,
			  enum lora_link_direction direction);

/**
 * @brief Depacketize received lp0 message
 *
 * @param[in] payload data buffer
 * @param[in] payload_len data buffer length
 * @param[out] port port number
 * @param[in] config - pointer to lp0 configuration structure with authentication keys
 * @param[out] target - pointer to buffer to hold depacketized data
 * @param[in out] target_len - pointer to size of target buffer, updated with actual length
 * @param[out] frame_counter - pointer to hold frame counter of received message
 * @param[in] lorawan_header - true if message uses lorawan header, false for custom lp0 header
 *
 * @return int 0 on success, negative error code otherwise
 */
int lp0_depacketize_message(uint8_t *payload, uint8_t payload_len, uint8_t *port,
			    struct lp0_cfg *config, uint8_t *target, size_t *target_len,
			    uint32_t *frame_counter, bool lorawan_header);

#ifdef __cplusplus
}
#endif

#endif /* LP0_PACKETIZATION_H */
