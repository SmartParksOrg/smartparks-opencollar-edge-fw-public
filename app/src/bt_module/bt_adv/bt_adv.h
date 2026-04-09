/** @file bt_adv.h
 *
 * @brief Interface for ble BT advertisement
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2022 Irnas. All rights reserved.
 */

#ifndef BT_ADV_H
#define BT_ADV_H

#include <zephyr/kernel.h>

#define DEVICE_NAME           "SP"
#define DEVICE_NAME_LEN       (sizeof(DEVICE_NAME) - 1)
#define DEVICE_NAME_LEN_MAC   6 // MAC address values added to device name
#define DEVICE_NAME_LEN_TOTAL (DEVICE_NAME_LEN + DEVICE_NAME_LEN_MAC)
#define MAX_ADVERTISEMENT_LEN (24 - DEVICE_NAME_LEN_TOTAL)

#define BT_MANUFACTURER_DATA 0x0A61 // Nordic: 0x0059

/**
 * @brief Init BLE advertisement.
 * Construct default device name and add manufacturer data to advertisement data.
 *
 * @return int
 */
void ble_adv_init(char *new_name, uint8_t len);

/**
 * @brief Start BLE advertisement.
 *
 * @return int 0 on success, -EAGAIN if timed out, or other negative error codes
 */
int ble_adv_start(void);

/**
 * @brief Stop BLE advertisement.
 *
 * @return int 0 on success, -EAGAIN if timed out, or other negative error codes
 */
int ble_adv_stop(void);

/**
 * @brief Update ble advertising name.
 *
 * @param new_name char array. Max length is BLE_ADV_DEVICE_NAME_LEN_TOTAL.
 * @return
 */
void ble_adv_device_name_update(char *new_name, uint8_t len);

/**
 * @brief Return ble advertisement device name.
 *
 * @return char* adv name
 */
char *ble_adv_get_device_name(void);

/**
 * @brief Set advertising interval.
 *
 * @param[in] interval interval in ms
 */
void ble_adv_set_interval(uint32_t interval);

/**
 * @brief Update advertisement data.
 *
 * @param data data array
 * @param len data length
 * @return int 0 on success, -EINVAL if too long, or other negative error codes
 */
int ble_adv_data_update(uint8_t *data, uint8_t len);

/**
 * @brief Set advertisement data - do not update.
 *
 * @param data data array
 * @param len data length
 * @return int 0 on success, -EINVAL if too long, or other negative error codes
 */
void ble_adv_data_set(uint8_t *data, uint8_t len);

#endif // BT_ADV_H
