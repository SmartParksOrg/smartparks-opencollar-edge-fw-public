/** @file lorawan_tools_common.h
 *
 * @brief This header file contains common structure definitions for LoRaWAN tools.
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2025 Irnas.  All rights reserved.
 */

#ifndef LORAWAN_TOOLS_COMMON_H
#define LORAWAN_TOOLS_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

#include <zephyr/types.h>

/**
 * @brief LoRaWAN common structure definitions
 *
 * This structure holds the necessary keys and device address for LoRaWAN (ABP)
 * communication.
 */
struct lorawan_auth_info {
	uint8_t NwkSKey[16]; /* Network Session Key */
	uint8_t AppSKey[16]; /* Application Session Key */
	uint8_t DevAddr[4];  /* Device Address */
};

enum lora_link_direction {
	LORA_DIRECTION_UPLINK = 0,
	LORA_DIRECTION_DOWNLINK = 1
};

enum lorawan_minor_version {
	LORAWAN_MINOR_VERSION_1_0 = 0, /* for LoRaWAN version 1.0 */
	LORAWAN_MINOR_VERSION_1_1 = 1  /* for LoRaWAN version 1.1 */
};

#ifdef __cplusplus
}
#endif

#endif /* LORAWAN_TOOLS_COMMON_H */
