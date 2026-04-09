/** @file bt_interface.h
 *
 * @brief Interface for uart ble and dfu
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2020 Irnas. All rights reserved.
 */

#ifndef BT_INTERFACE_H
#define BT_INTERFACE_H

#include <zephyr/kernel.h>

#include "thread_com.h"

/* Public functions */

/**
 * @brief Initialize BT module.
 *
 * Init DFU, NUS support and connection cbs.
 *
 * @return int -  negative error code or 0 if ok.
 */
int init_bt_module(void);

/**
 * @brief Start BT service.
 *
 * Register BT service and start BT advertisement.
 *
 * @return int
 */
int start_bt_service(void);

/**
 * @brief Stop BT service.
 *
 * If connected, disconnect. Stop advertising and set BT threads as disabled.
 *
 * @return int - negative error code or 0 if ok.
 */
int stop_bt_service(void);

/**
 * @brief Stop BT advertisement.
 *
 * @return int - negative error code or 0 if ok.
 */
int stop_bt_advertisement(void);

/**
 * @brief Start BT advertisement.
 *
 * @return int - negative error code or 0 if ok.
 */
int start_bt_advertisement(void);

/* Parse and execute message */
int parse_ble_message(uint8_t *message, uint8_t message_length, mb_msg_dest message_origin,
		      mb_msg_action message_action, uint8_t port, uint8_t msg_max_rsp_len);

/* Put thread message to uart rx buffer */
void send_ble_message(uint8_t *message, uint8_t message_length, uint8_t port);

/**
 * @brief Check if we need to update advertisement data.
 *
 * If we are connected of advertisement is disabled return false.
 * If period is over or we have not update data in the first place, return true.
 *
 * @return true
 * @return false
 */
bool check_adv_data_update_period(void);

/**
 * @brief Get the mac object of BT device
 *
 * @param mac
 * @param len
 */
void get_mac(uint8_t *mac, uint8_t len);

#endif
