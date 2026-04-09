/** @file bt_cmdq.h
 *
 * @brief Capture bluetooth advertised data from the Cardiac monitoring device Q
 *
 * The Cardiac monitoring device Q advertises over bluetooth with a burst of 8 packets every 3
 * minutes. With an active scan, a scan response can be requested to get the full data.
 *
 * Further information regarding the Cardiac monitoring device Q can be found here:
 * https://github.com/IRNAS/smartparks-opencollar-edge-fw/issues/389
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2024 Irnas.  All rights reserved.
 */

#ifndef BT_CMDQ_H
#define BT_CMDQ_H

#include <stdbool.h>
#include <zephyr/kernel.h>

/**
 * @brief Event handler for scan results.
 *
 * @param[in] data  The data buffer containing the scan results. Is NULL if no cmdq device was
 * detected in the scan.
 * @param[in] len   length of the buffer
 * @param[in] timestamp timestamp of the scan
 *
 */
typedef void (*cmdq_callback_t)(uint8_t *data, size_t len, uint32_t timestamp);

/**
 * @brief Start cmdq operation
 *
 * CMDQ operation is started with a discovery scan.
 * A discovery scan (SCAN_LONG) is an active scan that requests a scan response from
 * the configured bluetooth device. We want to detect the advertisement of the Cardiac monitoring
 * device Q without prior chronological knowledge.
 *
 * @param[in] cb Callback function that will handle the scan results.
 *
 * @return int 0 on success
 * @return -EPERM if the `CMDQ_enabled` setting is not enabled
 * @return -EALREADY if the operation is already running
 * @return negative error codes otherwise
 */
int bt_cmdq_start_operation(cmdq_callback_t cb);

/**
 * @brief stop cmdq operation
 *
 * New scans won't be scheduled and current scans will be stopped.
 */
void bt_cmdq_stop_operation(void);

/**
 * @brief Get cmdq operation status
 *
 * @return true if operation is running
 * @return false if operation is not running
 */
bool bt_cmdq_is_operation_started(void);

#endif /* BT_CMDQ_H */
