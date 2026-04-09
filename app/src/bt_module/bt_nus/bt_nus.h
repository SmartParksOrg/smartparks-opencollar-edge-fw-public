/** @file bt_nus.h
 *
 * @brief Interface for ble NUS service
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2022 Irnas. All rights reserved.
 */

#ifndef BT_NUS_H
#define BT_NUS_H

#include <zephyr/kernel.h>

#define NUS_BUF_SIZE CONFIG_BT_NUS_BUFFER_SIZE

// NUS buffer
typedef struct {
	void *fifo_reserved;
	uint8_t data[NUS_BUF_SIZE];
	uint16_t len;
} nus_data_t;

/**
 * @brief Register BT bus callback - only RX.
 *
 * @return int
 */
int register_bt_service(void);

/**
 * @brief Get data from rx fifo buffer.
 * Wait indefinitely for something to come in.
 *
 * @return nus_data_t*
 */
nus_data_t *get_fifo_rx_data(void);

/**
 * @brief Put message to be send in tx buffer.
 *
 * @param tx - NUS data buffer
 */
void put_fifo_tx_data(nus_data_t *tx);

/**
 * @brief Wait indefinitely for data in tx buffer and send it.
 *
 * @return int
 */
int bt_nus_send_data(void);

#endif
