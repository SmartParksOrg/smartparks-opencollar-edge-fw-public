/** @file bt_cmdq_messaging.h
 *
 * @brief Messaging functions for bt_cmdq module.
 *
 * Handles communication between the bt_cmdq module and satellite/LoRa/flash modules.
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2024 Irnas.  All rights reserved.
 */

#ifndef BT_CMDQ_MESSAGING_H
#define BT_CMDQ_MESSAGING_H

#ifdef __cplusplus
extern "C" {
#endif

#include <zephyr/kernel.h>

/**
 * @brief Print important scan data
 *
 * For detailed definition of the important scan data, follow the link below:
 * https://github.com/IRNAS/smartparks-opencollar-edge-fw/issues/389
 *
 * @param[in] data scan data buffer
 * @param[in] len scan data buffer length
 * @param[in] timestamp timestamp of the scan
 */
void bt_cmdq_messaging_print_scan_data(uint8_t *data, size_t len, uint32_t timestamp);

/**
 * @brief save scan data to internal buffer
 *
 * @param data buffer containing data
 * @param len  length of the buffer
 * @param timestamp timestamp of the scan
 */
void bt_cmdq_messaging_save_to_buffer(uint8_t *data, size_t len, uint32_t timestamp);

/**
 * @brief Send scan data to satellite/LoRa/flash modules
 *
 * @param[out] message buffer to store the message
 * @param[in] max_len maximum length of the message
 *
 * @return length of message (without the header)
 */
int bt_cmdq_messaging_results_send(uint8_t *message, size_t max_len);

/**
 * @brief Compose the most recently acquired scan result message and save it into payload buffer
 *
 * @param[out] payload buffer to store the message
 * @param[in] max_len maximum length of the message
 *
 * @return int length of message
 */
int bt_cmdq_messaging_compose_latest_results_message(uint8_t *payload, uint8_t max_len);

/**
 * @brief Compose scan results message and save it into payload buffer
 *
 * @param[out] payload buffer to store the message
 * @param[in] max_len maximum length of the message
 *
 * @return int length of message
 */
int bt_cmdq_messaging_compose_results_message(uint8_t *payload, uint8_t max_len);

#ifdef __cplusplus
}
#endif

#endif /* BT_CMDQ_MESSAGING_H */
