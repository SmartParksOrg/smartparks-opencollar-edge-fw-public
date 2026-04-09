/** @file bt_scan.h
 *
 * @brief Interface to process bt scan results
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2020 Irnas. All rights reserved.
 */

#ifndef BT_SCAN_H
#define BT_SCAN_H

#include <zephyr/kernel.h>

/**
 * @brief Perform bt scan.
 *
 * Start BT scan. Results are going to be handled in BT scan callback.
 * Stop scanning after interval defined in the settings.
 *
 * @return int - 0 if ok, negative integer error code otherwise.
 */
int bt_scan(void);

/**
 * @brief Send results of a single BT scan period.
 *
 * Adjust max message length base on destination message is going to be sent (flash, LR or both).
 * Find all results stored in result buffer with specific timestamp, provided as an argument.
 * Sort results by rssi. Loop over the results and formulate one or more results messages, based on
 * max length restriction. Over LR, send only one message, but store all messages to flash.
 *
 * @param message - buffer
 * @param max_len - max length of single message
 * @param scan_time - timestamp of scan we want to obtain results for.
 * @return int - negative error code or 0 if ok.
 */
int bt_scan_send(uint8_t *message, uint8_t max_len, uint32_t scan_time);

/**
 * @brief Sen aggregated results of BT scan. After send empty the results buffer.
 *
 * Adjust max message length base on destination message is going to be sent (flash, LR or both).
 * Compose single message of aggregated results. Include max possible number of results, provided
 * max message length. Send to LR and of flash.
 *
 * @param message - buffer
 * @param max_len - max single message length.
 * @return int - negative error code or 0 if ok.
 */
int bt_scan_send_aggregated(uint8_t *message, uint8_t max_len);

/** Add single entry at the end of the buffer.
 *
 * @param payload - buffer containing message
 * @param max_len max length of message
 *
 * @return message length.
 */
int compose_message_bt_scan_results(uint8_t *payload, uint8_t max_len, uint8_t max_res);

/** Add single entry at the end of the buffer.
 *
 * @param payload - buffer containing message
 * @param max_len max length of message
 * @param scan_time desired scan time
 * @param start_idx start idx of results to store in payload buffer
 *
 * @return message length.
 */
int compose_message_bt_scan_result_single_scan(uint8_t *payload, uint8_t max_len,
					       uint32_t scan_time, uint8_t start_idx);

/** Get number of all scan results in current buffer structure.
 *
 * @return number of all stored aggregated scan results
 */
uint8_t get_nr_scan_results(void);

/** Get number of scan results for specific scan, given the timestamp in current buffer structure.
 *
 * @param timestamp - desired timestamp
 *
 * @return nr of scan results with specific timestamp
 */
uint8_t get_nr_timestamp_scan_results(uint32_t timestamp);

#endif // BT_SCAN_H
