/** @file bt_nus.h
 *
 * @brief Interface for ble NUS service
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2022 Irnas. All rights reserved.
 */

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/types.h>
#include <soc.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/hci_vs.h>
#include <zephyr/bluetooth/uuid.h>
#include <bluetooth/services/nus.h>

#include "bt_con.h"
#include "bt_nus.h"
#include "commands_def.h"

LOG_MODULE_REGISTER(BT_NUS);

// Fifo for storing outgoing messages
static K_FIFO_DEFINE(fifo_tx_data);
// Fifo for storing received messages
static K_FIFO_DEFINE(fifo_rx_data);

/* BT receive data callback */
static void bt_receive_cb(struct bt_conn *conn, const uint8_t *const data, uint16_t len)
{
	char addr[BT_ADDR_LE_STR_LEN] = {0};

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, ARRAY_SIZE(addr));
	bt_con_reset_disconnect_timeout();

	LOG_INF("Received data from: %s\n", addr);

	for (uint16_t pos = 0; pos != len;) {
		nus_data_t *rx = k_malloc(sizeof(*rx));

		if (!rx) {
			LOG_INF("Not able to allocate send data buffer\n");
			return;
		}

		/* Keep the last byte of TX buffer for potential LF char. */
		size_t rx_data_size = sizeof(rx->data) - 1;

		if ((len - pos) > rx_data_size) {
			rx->len = rx_data_size;
		} else {
			rx->len = (len - pos);
		}

		memcpy(rx->data, &data[pos], rx->len);

		pos += rx->len;
		LOG_INF("Data length: %d", rx->len);

		/* Append the LF character when the CR character triggered
		 * transmission from the peer.
		 */
		if ((pos == len) && (data[len - 1] == '\r')) {
			rx->data[rx->len] = '\n';
			rx->len++;
		}

		k_fifo_put(&fifo_rx_data, rx);
	}
}

static struct bt_nus_cb nus_cb = {
	.received = bt_receive_cb,
};

int register_bt_service(void)
{
	int err = bt_nus_init(&nus_cb);

	return err;
}

nus_data_t *get_fifo_rx_data(void)
{
	nus_data_t *buf = k_fifo_get(&fifo_rx_data, K_FOREVER);

	// Discard if pin is not validated
	if (!bt_pin_is_validated() && buf->data[1] != CMD_CHECK_PIN) {
		LOG_ERR("Pin not validated! Discar message!");
		k_free(buf);
		return NULL;
	}

	return buf;
}

void put_fifo_tx_data(nus_data_t *tx)
{
	k_fifo_put(&fifo_tx_data, tx);
}

int bt_nus_send_data(void)
{
	/* Wait indefinitely for data to be sent over bluetooth */
	nus_data_t *buf = k_fifo_get(&fifo_tx_data, K_FOREVER);

	for (int i = 0; i < buf->len; i++) {
		LOG_DBG("%x ", buf->data[i]);
	}

	// Get max mtu size
	int nus_max_send_len = bt_con_get_max_payload();
	if (nus_max_send_len <= 0) {
		k_free(buf);
		LOG_ERR("We don't have available MTU buffer!");
		return -EIO;
	}

	// Determine nr. of pactets
	uint32_t buf_count = 0;
	uint32_t buf_size = 0;
	if (0 != buf->len % nus_max_send_len) {
		buf_count = (buf->len / nus_max_send_len) + 1;
	} else {
		buf_count = buf->len / nus_max_send_len;
	}

	LOG_INF("BT thread, got RX buf, of length: %d, available MTU: %d, send in %d packets!\n",
		buf->len, nus_max_send_len, buf_count);

	int err = 0;
	for (uint32_t i = 0; i < buf_count; i++) {

		if ((buf_count - 1) == i && (0 != buf->len % nus_max_send_len)) {
			buf_size = buf->len % nus_max_send_len;
		} else {
			buf_size = nus_max_send_len;
		}
		err = bt_nus_send(NULL, buf->data + i * nus_max_send_len, buf_size);
#ifdef CONFIG_DEBUG_MODE
		/*
		printk("Send BLE message: ");
		for(int i=0; i< buf->len; i++)
		{
		    printk("%x ", buf->data[i]);
		}
		printk("\n");
		*/
#endif
		if (err) {
			LOG_ERR("Failed to send data over BLE connection\n");
		}
	}

	k_free(buf);

	return err;
}
