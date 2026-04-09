/** @file lorawan.h
 *
 * @brief Interface for lr1120 and LoRaWAN communication
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2023 Irnas. All rights reserved.
 */

#ifndef LORAWAN_H
#define LORAWAN_H

#include <zephyr/kernel.h>
/**
 * @brief Port to send and receive test messages
 */
#define LORAWAN_MSG_PORT 1

/**
 * @brief Port to receive settings data
 */
#define LORAWAN_SETTINGS_PORT 2

/**
 * @brief available adaptive data rate (ADR) profiles
 *
 */
enum lorawan_adr_profile {
	ADR_PROFILE_NETWORK_CONTROLLED = 0x00, /* Network Server controlled for static devices */
	ADR_PROFILE_MOBILE_LONG_RANGE = 0x01,  /* Long range distribution for mobile devices */
	ADR_PROFILE_MOBILE_LOW_POWER = 0x02,   /* Low power distribution for mobile devices */
	ADR_PROFILE_CUSTOM = 0x03,             /* User defined distribution */
};

/**
 * @brief Type for function that will handle downlink events from lorawan
 *
 * @param payload const uint8_t * (byte array)
 * @param size of payload (number of bytes)
 * @param port on which the payload was received
 */
typedef void (*lorawan_recv_handler_t)(const uint8_t *payload, uint8_t size, uint8_t port);

/**
 * @brief Register external callback for downlink events
 *
 */
void lorawan_recv_handler_register(lorawan_recv_handler_t);

void lorawan_set_configuration(uint8_t join_eui[8], uint8_t app_key[16], uint8_t region,
			       uint8_t adr, enum lorawan_adr_profile adr_profile);

/**
 * @brief Start Semtech engine. Call this function to initiate reset and lora join.
 *
 */
void lorawan_start(void);

/**
 * @brief Reset lorawan module.
 *
 */
void lorawan_reset(void);

/**
 * @brief Check if device has joined the LoRaWAN network
 *
 * @retval true If joined
 * @retval false If not joined
 */
bool lorawan_is_joined(void);

/**
 * @brief Check if we are in the middle of the joining process. Do not attempt direct radio access
 * during that time.
 *
 * @return true - we are in the middle of joining
 * @return false - we are not in the middle of joining
 */
bool lorawan_joining_status(void);

/**
 * @brief Send message via LoRaWAN
 * Put message in the send que. Message will be send on a FIFO principle.
 *
 * @param[in] payload - message to send
 * @param[in] payload_length - message length
 * @param[in] send_port - port to send message on
 * @param[in] confirmed - send message as confirmed or not confirmed
 * @param[in] join_attempt - attempt re-join if not joined
 *
 * @return int - negative error code, 0 on success
 * @retval -EMSGSIZE Message length invalid.
 */
int lorawan_send_message(uint8_t *payload, uint8_t payload_length, uint8_t send_port,
			 bool confirmed, bool join_attempt);

/**
 * @brief Obtain next message size
 *
 * @return message len or error code.
 */
int lorawan_get_max_payload(void);

/**
 * @brief Get lr11xx chip eui.
 *
 * @param[in] dev_eui buffer to store dev eui
 *
 * @retval SMTC_MODEM_RC_OK                 Command executed without errors
 * @retval SMTC_MODEM_RC_BUSY               Modem is currently in test mode
 * @retval SMTC_MODEM_RC_INVALID_STACK_ID   Invalid stack_id
 */
int lorawan_get_dev_eui(uint8_t dev_eui[8]);

/**
 * @brief Get configured network key.
 *
 * @param[in] nwkkey buffer to store network key
 */
void lorawan_get_nwkkey(uint8_t nwkkey[16]);

/**
 * @brief Return lorawan module status.
 *
 * @return true - enabled
 * @return false - disabled
 */
bool lorawan_is_enabled(void);

/**
 * @brief Temporary suspend smtc engine and label lorawan module as suspended.
 *
 */
void lorawan_suspend(void);

/**
 * @brief Resume smtc engine.
 *
 */
void lorawan_resume(void);

/**
 * @brief Leave network.
 *
 * @retval SMTC_MODEM_RC_OK                Command executed without errors
 * @retval SMTC_MODEM_RC_BUSY              Modem is currently in test mode
 * @retval SMTC_MODEM_RC_INVALID_STACK_ID  Invalid stack id
 */
void lorawan_leave_network(void);

/**
 * @brief Perform autonomous gnss scan.
 *
 */
void lorawan_gnss_scan_autonomous(void);

/**
 * @brief Perform assisted gnss scan.
 *
 */
void lorawan_gnss_scan_assisted(int32_t lat, int32_t lon, uint32_t gps_time);

/**
 * @brief If new almanac is available, update it.
 *
 */
void lorawan_update_almanac(void);

/**
 * @brief Preform wifi scan.
 *
 */
void lorawan_wifi_scan(void);

/**
 * @brief Preform s-band send
 *
 * @param[in] payload - message to send
 * @param[in] payload_length - message length
 * @param[in] port - port to send message on
 * @param[in] network_key - network key to use for payload encoding
 * @param[in] app_key - app key to use for payload encoding
 * @param[in] dev_addr - device address to use for payload encoding
 */
void lorawan_s_band_send(uint8_t *payload, uint8_t payload_length, uint8_t port,
			 uint8_t network_key[16], uint8_t app_key[16], uint8_t dev_addr[4]);

/**
 * @brief Add VHF (very high frequency) burst to the lorawan event queue.
 */
void lorawan_vhf_burst(void);

/**
 * @brief Get VHF burst queued status. Used to check if VHF burst is in the LoRaWAN queue.
 *
 * @return true if VHF burst is in the LoRaWAN queue, false otherwise.
 */
bool lorawan_get_vhf_burst_queued(void);

#endif /* LORAWAN_H */
