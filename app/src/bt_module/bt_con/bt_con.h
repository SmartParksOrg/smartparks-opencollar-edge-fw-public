/** @file bt_con.h
 *
 * @brief Interface for bt connection callbacks
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2020 Irnas. All rights reserved.
 */

#ifndef BT_CON_H
#define BT_CON_H

#include <zephyr/kernel.h>

/**
 * @brief Register connection callbacks and enable DFU service.
 *
 * @return int 0 on success, -EAGAIN if timed out, or other negative error codes
 */
int bt_con_init(void);

/**
 * @brief If device is connected, disconnect.
 *
 * @return int 0 on success, -EAGAIN if timed out, or other negative error codes
 */
int bt_con_disconnect(void);

/**
 * @brief Return connection status.
 *
 * @return true - if device is connected.
 * @return false - if device is not connected.
 */
bool bt_is_connected(void);

/**
 * @brief Get max available MTU size if device is connected.
 *
 * @return int 0 if device is not connected or max MTU size if device is connected.
 */
int bt_con_get_max_payload(void);

/**
 * @brief Return pin validation status.
 *
 * @return true - pin is validated or it doesn't need validation
 * @return false - pin is not validated
 */
bool bt_pin_is_validated(void);

/**
 * @brief Check if we need to disconnect the device.
 * There are two possible scenarios:
 * Pin was not yet validated and connection is established for more than BT_PIN_VALIDATION_TIMEOUT
 * ms. Connection is established for more than Main_settings.ble_auto_disconnect time without and
 * action. If any of the criteria is reached, disconnect the device.
 *
 */
void bt_con_check_disconnect_period(void);

/**
 * @brief Validate pin against set pin code or app key.
 * If pin is invalid disconnect.
 *
 * @param pin[in] - pin byte array
 * @param pin_len[in] - pin length
 * @return int 0 on success, or other negative error codes
 */
int bt_con_check_pin(uint8_t *pin, uint8_t pin_len);

/**
 * @brief Reset connection time to avoid disconnection.
 *
 */
void bt_con_reset_disconnect_timeout(void);

/**
 * @brief Ignore/Heed the disconnect period
 *
 * This is used to bypass the auto-disconnect period. For example, when sending a lot of data,
 * allowing the streaming time to exceed the auto-disconnect period (`ble_auto_disconnect` user
 * setting), without changing the setting itself (protection in case of reboot while streaming).
 *
 * When enabled, the auto-disconnect setting is ignored.
 * When disabled, the auto-disconnect setting is heeded.
 *
 * The default state is disabled (setting is heeded).
 *
 * This is automatically set to false when the BT connection is disconnected/drops.
 *
 * @param enable[in] - true to enable, false to disable.
 */
void bt_con_ignore_disconnect_period_set(bool enable);

#endif // BT_CON_H
