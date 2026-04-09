/**
 * @file lorawan_tools.c
 * @brief
 *
 * @copyright (c) 2023 Irnas. All rights reserved.
 *
 */

#include "AES-128_V10.h"
#include "Encrypt_V20.h"
#include "lorawan_tools.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/types.h>

LOG_MODULE_REGISTER(lorawan_tools);

/* MHDR(1) + DevAddr(4) + FCtrl(1) + FCnt(2) = 8 */
#define LORA_PAYLOAD_FIXED_OFFSET 8
#define MIC_SIZE                  4

/**
 * @brief Build a LoRaWAN uplink payload.
 *
 * This function takes a raw payload, encrypts it using the LoRaWAN
 * encryption scheme, and constructs a full LoRaWAN frame (MHDR..MIC).
 *
 * @param [in] minor_version   LoRaWAN minor version (enum lorawan_minor_version)
 * @param [in] payload         Pointer to raw payload data
 * @param [in] payload_len     Length of the raw payload in bytes
 * @param [in] port            Application port number
 * @param [in] frame_counter   Frame counter value
 * @param [in] frame_counter_32 Whether a 32-bit frame counter is used (true) or 16-bit (false)
 * @param [out] target          Output buffer for full LoRaWAN packet (MHDR..MIC)
 * @param [in] confirmed       true for confirmed uplink, false for unconfirmed
 * @param [in] direction       Link direction (uplink or downlink)
 * @param [in] auth_info       Pointer to the lorawan_auth_info structure containing LoRa keys and
 * the device address
 *
 * @return size_t Total length of the constructed LoRaWAN packet in bytes
 */
static size_t prv_build_lorawan(enum lorawan_minor_version minor_version, uint8_t *payload,
				size_t payload_len, uint8_t port, uint32_t frame_counter,
				bool frame_counter_32, uint8_t *target, bool confirmed,
				enum lora_link_direction direction,
				struct lorawan_auth_info *auth_info)
{
	/* Confirmed data up (true) - Unconfirmed data up (false) */
	uint8_t Mac_Header;

	if (direction == LORA_DIRECTION_UPLINK) {
		Mac_Header = confirmed ? 0x80 : 0x40;
	} else {
		Mac_Header = confirmed ? 0xA0 : 0x60;
	}

	uint8_t i;
	uint8_t Mac_Length = 0x00;
	uint8_t MIC[4];
	uint8_t Frame_Control = 0x00;
	Encrypt_Payload(payload, payload_len, frame_counter, frame_counter_32,
			port == 0 ? true : false, direction, auth_info);

	/* Build the Radiopackage */
	target[0] = Mac_Header;
	target[1] = auth_info->DevAddr[3];
	target[2] = auth_info->DevAddr[2];
	target[3] = auth_info->DevAddr[1];
	target[4] = auth_info->DevAddr[0];
	target[5] = Frame_Control;

	target[6] = (frame_counter & 0x00FF);
	target[7] = ((frame_counter >> 8) & 0x00FF);

	target[8] = port;

	/* Load Data into package */
	for (i = 0; i < payload_len; i++) {
		target[i + 9] = payload[i];
	}

	Mac_Length = payload_len + 9;

	/* Calculate the MIC for the package */
	Calculate_MIC(target, MIC, Mac_Length, frame_counter, frame_counter_32, minor_version,
		      direction, auth_info);

	/* Load MIC in RFM Package */
	for (i = 0; i < 4; i++) {
		target[Mac_Length + i] = MIC[i];
	}

	return Mac_Length + 4;
}

int lorawan_tools_parse(uint8_t *payload, size_t payload_len, uint8_t *port,
			uint32_t *frame_counter, bool frame_counter_32, uint8_t *target,
			size_t *target_len, struct lorawan_auth_info *auth_info)
{
	if (payload_len < 12) {
		LOG_ERR("Invalid LoRaWAN packet length");
		*target_len = 0;

		return -EINVAL;
	}

	uint8_t MHDR = payload[0];
	uint8_t FCtrl = payload[5];
	uint16_t FCnt = payload[6] | (payload[7] << 8);

	uint8_t FOptsLen = FCtrl & 0x0F;

	enum lora_link_direction direction;
	if (MHDR == 0x00 || MHDR == 0x40 || MHDR == 0x80 || MHDR == 0xC0) {
		direction = LORA_DIRECTION_UPLINK;
	} else if (MHDR == 0x20 || MHDR == 0x60 || MHDR == 0xA0) {
		direction = LORA_DIRECTION_DOWNLINK;
	} else {
		LOG_WRN("Lora Proprietary MHDR detected! Passing raw payload.");
		if (*target_len < payload_len) {
			LOG_ERR("Target buffer too small (%d < %d)", *target_len, (int)payload_len);
			return -ENOMEM;
		}
		memcpy(target, payload, payload_len);
		*target_len = payload_len;
		return 0;
	}

	LOG_DBG("MHDR: 0x%02X", MHDR);
	LOG_DBG("DevAddr: %02X%02X%02X%02X", payload[4], payload[3], payload[2], payload[1]);
	LOG_DBG("FCtrl: 0x%02X  FCnt: %u", FCtrl, FCnt);

	/* Payload length = LORA_PAYLOAD_FIXED_OFFSET - FOpts - MIC(4) */
	int FRMPayload_len = payload_len - LORA_PAYLOAD_FIXED_OFFSET - FOptsLen - MIC_SIZE;
	LOG_DBG("Encrypted payload length: %d", (int)FRMPayload_len);

	/* Shorten FRMPayload len for FPort(1) if present */
	if (FRMPayload_len > 0) {
		FRMPayload_len -= 1;
	}

	if (FRMPayload_len > 0) {
		uint8_t FRMPayload[FRMPayload_len];
		memcpy(FRMPayload, &payload[LORA_PAYLOAD_FIXED_OFFSET + FOptsLen + 1],
		       FRMPayload_len); /* +1 for FPort */

		uint8_t FPort = payload[8 + FOptsLen];
		LOG_DBG("FCtrl: 0x%02X  FCnt: %u  FPort: %u", FCtrl, FCnt, FPort);

		/* ---- Decrypt the FRMPayload ---- */
		/* Note: We're using symmetrical encryption. Encrypting the payload again will
		 * decrypt it. */
		Encrypt_Payload(FRMPayload, FRMPayload_len, FCnt, frame_counter_32,
				FPort == 0 ? true : false, direction, auth_info);
		if (*target_len < FRMPayload_len) {
			LOG_ERR("Target buffer too small (%d < %d)", *target_len,
				(int)FRMPayload_len);
			return -ENOMEM;
		}
		memcpy(target, FRMPayload, FRMPayload_len);
		*target_len = FRMPayload_len;
		*port = FPort;
		*frame_counter = FCnt;
	} else {
		*target_len = 0;
	}

	return 0;
}

int lorawan_tools_build(uint8_t *payload, uint8_t payload_len, uint8_t port,
			struct lorawan_auth_info *auth_info, uint32_t frame_counter,
			uint8_t *target, size_t *target_len, bool confirmed,
			enum lora_link_direction direction)
{
	if (payload_len > *target_len - LORA_PAYLOAD_FIXED_OFFSET - MIC_SIZE - 1) {
		LOG_ERR("Payload length too large: %d/%d", payload_len,
			*target_len - LORA_PAYLOAD_FIXED_OFFSET - MIC_SIZE - 1);
		return -EINVAL;
	}

	/* LoRaWAN minor version */
	enum lorawan_minor_version minor_version = LORAWAN_MINOR_VERSION_1_0;
	/* Use 16 or 32 bit frame counter - use 16 */
	bool frame_counter_32 = false;

	*target_len = prv_build_lorawan(minor_version, payload, payload_len, port, frame_counter,
					frame_counter_32, target, confirmed, direction, auth_info);

	return 0;
}
