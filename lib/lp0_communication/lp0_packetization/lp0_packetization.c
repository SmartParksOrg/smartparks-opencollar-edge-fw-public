/** @file lp0_packetization.c
 *
 * @brief Interface for lp0 packetization operations
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2025 Irnas. All rights reserved.
 */

#include "lr11xx_lr_fhss_types.h"
#include "lr11xx_radio.h"
#include <zephyr/logging/log.h>
#include <lorawan_tools.h>

#include <lp0_packetization.h>

LOG_MODULE_REGISTER(lp0_packetization);

/**
 * @brief Packetize message with custom lp0 header for lp0 transmission
 *
 * @param[in] payload data buffer
 * @param[in] payload_len data buffer length
 * @param[in] auth_info - pointer to lorawan authentication information structure
 * @param[out] target - pointer to buffer to hold packetized data
 * @param[in out] target_len - pointer to size of target buffer, updated with actual length
 * @param[in] msg_type - message type (see README.md for details)
 *
 * @retval int 0 on success
 * @retval -ENOMEM if target buffer is too small
 */
static int prv_lp0_packetize_message_with_custom_header(uint8_t *payload, uint8_t payload_len,
							struct lorawan_auth_info *auth_info,
							uint8_t *target, size_t *target_len,
							uint8_t msg_type)
{
	/* Custom header:
	 * | Byte # | Description                     |
	 * | ------ | ------------------------------- |
	 * | 0-1 | Sync bytes |
	 * | 2-5 | Dev ID (LSB)|
	 * | 6 | Message type / Port |
	 * | 7 | Payload length |
	 * | 8-(n-1) | Payload data |
	 */
	if (*target_len < payload_len + 8) {
		LOG_ERR("Target buffer too small (%d < %d)", *target_len, (int)(payload_len + 8));
		return -ENOMEM;
	}

	/* Sync bytes */
	target[0] = (LP0_CUSTOM_HEADER_SYNC_BYTES >> 8) & 0xFF;
	target[1] = LP0_CUSTOM_HEADER_SYNC_BYTES & 0xFF;

	/* Dev ID - (LSB) */
	target[2] = auth_info->DevAddr[3];
	target[3] = auth_info->DevAddr[2];
	target[4] = auth_info->DevAddr[1];
	target[5] = auth_info->DevAddr[0];

	target[6] = msg_type;                     /* Message type */
	target[7] = payload_len;                  /* Payload length */
	memcpy(&target[8], payload, payload_len); /* Payload data */

	*target_len = payload_len + 8;

	return 0;
}

/**
 * @brief Depacketize message with custom lp0 header
 *
 * @param[in] payload data buffer
 * @param[in] payload_len data buffer length
 * @param[out] devAddr - pointer to buffer where device address will be saved. Must be at least 4
 * bytes.
 * @param[out] target - pointer to buffer where packetized data will be saved
 * @param[in out] target_len - pointer to size of target buffer, updated with actual length
 * @param[out] msg_type - pointer where message type will be saved
 *
 * @retval int 0 on success
 * @retval -ENOMEM if target buffer is too small
 * @retval -EFAULT if invalid parameters are provided
 * @retval -EINVAL if invalid sync bytes are found
 */
static int prv_lp0_depacketize_message_with_custom_header(uint8_t *payload, uint8_t payload_len,
							  uint8_t *devAddr, uint8_t *target,
							  size_t *target_len, uint8_t *msg_type)
{
	if (*target_len < payload_len) {
		LOG_ERR("Target buffer too small (%d < %d)", *target_len, payload_len);
		return -ENOMEM;
	}
	if (payload == NULL || devAddr == NULL || target == NULL || target_len == NULL) {
		LOG_ERR("Null pointer provided to depacketization function");
		return -EFAULT;
	}

	/* Custom header:
	 * | Byte # | Description                     |
	 * | ------ | ------------------------------- |
	 * | 0-1 | Sync bytes |
	 * | 2-5 | Dev ID (LSB)|
	 * | 6 | Message type / Port |
	 * | 7 | Payload length |
	 * | 8-(n-1) | Payload data |
	 */
	if (payload[0] != (LP0_CUSTOM_HEADER_SYNC_BYTES >> 8) ||
	    payload[1] != (LP0_CUSTOM_HEADER_SYNC_BYTES & 0xFF)) {
		LOG_ERR("Invalid sync bytes in received packet payload");
		return -EINVAL;
	}

	memcpy(devAddr, &payload[2], 4);
	devAddr[0] = payload[5];
	devAddr[1] = payload[4];
	devAddr[2] = payload[3];
	devAddr[3] = payload[2];

	*msg_type = payload[6]; /* Message type */

	*target_len = payload_len;                /* Payload length */
	memcpy(&target[0], payload, payload_len); /* Payload data */

	return 0;
}

/**
 * @brief Calculate maximum payload length for given FHSS parameters
 *
 * @param[in] nb_header Number of header blocks (1 to 4)
 * @param[in] cr Coding rate
 *
 * @return size_t Maximum payload length in bytes
 */
static size_t prv_lr_fhss_max_payload(uint8_t nb_header, lr_fhss_v1_cr_t cr)
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

		/* crc is encoded with the payload */
		if (max_payload_bytes >= 2) {

			max_payload_bytes -= 2;
		} else {

			max_payload_bytes = 0;
		}

		retval = max_payload_bytes;
	}

	return retval;
}

/**
 * @brief Reset TX buffer and set buffer length to 0.
 *
 * @param[in] packet - pointer to payload data structure to reset
 */
static void prv_payload_data_reset(struct lp0_payload_data *packet)
{
	memset(packet->buffer_tx, 0, LP0_MAX_PAYLOAD_LENGTH);
	packet->buffer_len = sizeof(packet->buffer_tx);
}

int lp0_packetize_message(uint8_t *payload, uint8_t payload_len, uint8_t port, bool fhss,
			  struct lp0_payload_data *packet, struct lp0_communication_params *params,
			  struct lp0_cfg *config, bool confirmed, bool lorawan_header,
			  enum lora_link_direction direction)
{
	int ret = 0;
	if (fhss) {
		/* Check if payload will fit based on FHSS parameters */
		int fhss_max_payload = prv_lr_fhss_max_payload(
			params->lr_fhss.fhss_params.lr_fhss_params.header_count,
			params->lr_fhss.fhss_params.lr_fhss_params.cr);

		if (payload_len > fhss_max_payload) {
			LOG_ERR("Message too long. %d > %d", payload_len,
				prv_lr_fhss_max_payload(
					params->lr_fhss.fhss_params.lr_fhss_params.header_count,
					params->lr_fhss.fhss_params.lr_fhss_params.cr));
			return -EINVAL;
		}
	}

	/* Reset outgoing packet TX buffer and buffer length */
	prv_payload_data_reset(packet);

	if (lorawan_header) {
		/* Build and store lorawan compatible package */
		ret = lorawan_tools_build(payload, payload_len, port, &config->lora_auth_info,
					  packet->frame_counter, packet->buffer_tx,
					  &packet->buffer_len, confirmed, direction);
		if (ret) {
			LOG_ERR("Failed to build lorawan package.");
		}
	} else {
		ret = prv_lp0_packetize_message_with_custom_header(
			payload, payload_len, &config->lora_auth_info, packet->buffer_tx,
			&packet->buffer_len, port /* Using port as message type for now */);
	}
	return ret;
}

int lp0_depacketize_message(uint8_t *payload, uint8_t payload_len, uint8_t *port,
			    struct lp0_cfg *config, uint8_t *target, size_t *target_len,
			    uint32_t *frame_counter, bool lorawan_header)
{
	if (lorawan_header) {
		return lorawan_tools_parse(payload, payload_len, port, frame_counter, false, target,
					   target_len, &config->lora_auth_info);
	} else {
		ARG_UNUSED(frame_counter); /* Not used when using custom header */

		return prv_lp0_depacketize_message_with_custom_header(
			payload, payload_len, config->lora_auth_info.DevAddr, target, target_len,
			port);
	}
}
