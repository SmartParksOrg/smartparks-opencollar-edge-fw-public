/**
 * @file lorawan_tools.h
 * @brief
 *
 * @copyright (c) 2023 Irnas. All rights reserved.
 *
 */

#ifndef LORAWAN_TOOLS_H
#define LORAWAN_TOOLS_H

#include <stdbool.h>
#include <zephyr/types.h>
#include <lorawan_tools_common.h>

/**
 * @brief Build a LoRaWAN uplink payload with encryption and MIC.
 *
 * @param [in] payload Pointer to application payload to send
 * @param [in] payload_len Length of application payload in bytes
 * @param [in] port Application port number
 * @param [in] auth_info Pointer to the lorawan_auth_info structure containing LoRa keys and
 * @param [in] frame_counter Frame counter value
 * @param [out] target Output buffer for full LoRaWAN packet
 * @param [in out] target_len [in] Length of target buffer in bytes. [out] Number of bytes written
 * to target.
 * @param [in] confirmed Whether to use confirmed uplink (true) or unconfirmed (false)
 * @param [in] direction Link direction (uplink or downlink)
 *
 * @return Length of the built LoRaWAN packet in bytes
 */
int lorawan_tools_build(uint8_t *payload, uint8_t payload_len, uint8_t port,
			struct lorawan_auth_info *auth_info, uint32_t frame_counter,
			uint8_t *target, size_t *target_len, bool confirmed,
			enum lora_link_direction direction);

/**
 * @brief Decrypt and copy a LoRaWAN downlink payload to target.
 *
 * This function takes a full LoRaWAN frame (as sent over the air), extracts
 * the header fields, decrypts the FRMPayload using the appropriate key,
 * and (in debug mode) prints the decoded bytes.
 *
 * NOTE: There is no way for the function to know if decryption was successful. When using the wrong
 * key on either side, this function will still return 0, even if the data was not correctly
 * decrypted.
 *
 * @param [in] payload         Pointer to full LoRaWAN packet (MHDR..MIC)
 * @param [in] payload_len     Total packet length in bytes
 * @param [out] port            Application port number
 * @param [out] frame_counter   Frame counter value (same as used to encode)
 * @param [in] frame_counter_32 Whether a 32-bit frame counter is used (true) or 16-bit (false)
 * @param [out] target          Output buffer for decoded payload (optional)
 * @param [in out] target_len      On input, size of target buffer; on output, length of decoded
 * payload
 * @param [in] auth_info           Pointer to the lorawan_auth_info structure containing LoRa keys
 * and the device address
 *
 * @retval 0 on success,
 * @retval -ENOMEM if target buffer is too small,
 */
int lorawan_tools_parse(uint8_t *payload, size_t payload_len, uint8_t *port,
			uint32_t *frame_counter, bool frame_counter_32, uint8_t *target,
			size_t *target_len, struct lorawan_auth_info *auth_info);

#endif /* LORAWAN_TOOLS_H */
