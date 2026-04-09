/** @file bt_interface.c
 *
 * @brief Interface for uart ble and dfu
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2020 Irnas. All rights reserved.
 */

#include <stdio.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/types.h>
#include <soc.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/hci_vs.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/sys/byteorder.h>
#include <bluetooth/services/nus.h>

#include "generated_settings.h"
#include "global_time.h"
#include "thread_com.h"
#include "thread_operation.h"

#include "bt_adv.h"

#ifdef CONFIG_BT_CMDQ
#include "bt_cmdq_messaging.h"
#endif /* CONFIG_BT_CMDQ */

#include "bt_con.h"
#include "bt_interface.h"
#include "bt_nus.h"
#include "bt_scan.h"

#define BLE_RADIO_TX_POWER_DBM 8

LOG_MODULE_REGISTER(BT_INTERFACE, 4);

/* BT */
static uint32_t adv_data_update_period = ADV_DATA_UPDATE_INTERVAL;
static uint64_t adv_data_update = 0; // Last advertisement data update
static bool prv_adv_enabled = false;

/* =======================================================================================================
 */
/* Static private functions */
/* =======================================================================================================
 */

/* Get BT TX power */
static int get_tx_power(uint8_t handle_type, uint16_t handle, int8_t *tx_pwr_lvl)
{
	struct bt_hci_cp_vs_read_tx_power_level *cp;
	struct bt_hci_rp_vs_read_tx_power_level *rp;
	struct net_buf *buf, *rsp = NULL;
	int err;

	*tx_pwr_lvl = 0xFF;
	buf = bt_hci_cmd_create(BT_HCI_OP_VS_READ_TX_POWER_LEVEL, sizeof(*cp));
	if (!buf) {
		LOG_ERR("Unable to allocate command buffer");
		return -ENOBUFS;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	cp->handle = sys_cpu_to_le16(handle);
	cp->handle_type = handle_type;

	err = bt_hci_cmd_send_sync(BT_HCI_OP_VS_READ_TX_POWER_LEVEL, buf, &rsp);
	if (err) {
		uint8_t reason =
			rsp ? ((struct bt_hci_rp_vs_read_tx_power_level *)rsp->data)->status : 0;
		LOG_ERR("Read Tx power err: %d reason 0x%02x", err, reason);
		return err;
	}

	rp = (void *)rsp->data;
	*tx_pwr_lvl = rp->tx_power_level;

	net_buf_unref(rsp);
	return 0;
}

/* Set BT TX power */
int set_tx_power(uint8_t handle_type, uint16_t handle, int8_t tx_pwr_lvl)
{
	struct bt_hci_cp_vs_write_tx_power_level *cp;
	struct bt_hci_rp_vs_write_tx_power_level *rp;
	struct net_buf *buf, *rsp = NULL;
	int err;

	buf = bt_hci_cmd_create(BT_HCI_OP_VS_WRITE_TX_POWER_LEVEL, sizeof(*cp));
	if (!buf) {
		LOG_ERR("Unable to allocate command buffer");
		return -ENOBUFS;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	cp->handle = sys_cpu_to_le16(handle);
	cp->handle_type = handle_type;
	cp->tx_power_level = tx_pwr_lvl;

	err = bt_hci_cmd_send_sync(BT_HCI_OP_VS_WRITE_TX_POWER_LEVEL, buf, &rsp);
	if (err) {
		uint8_t reason =
			rsp ? ((struct bt_hci_rp_vs_write_tx_power_level *)rsp->data)->status : 0;
		LOG_ERR("Set Tx power err: %d reason 0x%02x", err, reason);
		return err;
	}

	rp = (void *)rsp->data;
	LOG_INF("Set Tx Power to %d dBm is OK", rp->selected_tx_power);

	net_buf_unref(rsp);
	return 0;
}

/* END Static private functions */

/* =======================================================================================================
 */
/* Public functions */
/* =======================================================================================================
 */

int init_bt_module(void)
{
	int8_t txp_get = 0xFF;

	// Register callbacks and init DFU
	int err = bt_con_init();

	// Init BLE and set transmit power to max
	err = bt_enable(NULL);
	if (err) {
		return err;
	}
	LOG_INF("Bluetooth initialized");

	err = set_tx_power(BT_HCI_VS_LL_HANDLE_TYPE_ADV, 0, BLE_RADIO_TX_POWER_DBM);
	if (err) {
		LOG_ERR("Unable to set BLE radio TX power to %d!", BLE_RADIO_TX_POWER_DBM);
	}
	err = get_tx_power(BT_HCI_VS_LL_HANDLE_TYPE_ADV, 0, &txp_get);
	if (err) {
		LOG_ERR("Unable to read BLE radio TX power!");
	} else {
		LOG_INF("BLE TX power is set to: %d dBm.", txp_get);
	}

	// Init adv module
	ble_adv_init(Main_settings.device_name->def_val, Main_settings.device_name->len);
	ble_adv_set_interval(Main_settings.ble_advertisement_interval->def_val);

	return 0;
}

int start_bt_service(void)
{
	int err = register_bt_service();
	if (err) {
		LOG_ERR("Failed to initialize BT service (err: %d)", err);
		/*terminate essential thread - fatal error*/
		return err;
	}

	/* Start BLE advertisement */
	if (Main_settings.ble_adv->def_val) {
		err = start_bt_advertisement();
		if (err) {
			LOG_DBG("Advertising failed to start (err %d)", err);
			return err;
		}
	} else {
		LOG_WRN("BLE adv not supported!");
	}

	return 0;
}

int stop_bt_advertisement(void)
{
	LOG_INF("Stop BT advertisement!");
	int err = ble_adv_stop();
	if (err) {
		LOG_ERR("Advertising failed to stop (err %d)", err);
	} else {
		prv_adv_enabled = false;
	}
	return err;
}

int start_bt_advertisement(void)
{
	LOG_INF("Start BT advertisement!");
	int err = ble_adv_start();
	if (err) {
		LOG_ERR("Advertising failed to start (err %d)", err);
	} else {
		prv_adv_enabled = true;
	}
	return err;
}

int stop_bt_service(void)
{
	int err = bt_con_disconnect();
	err = stop_bt_advertisement();
	LOG_INF("Disable receive and send threads.");
	set_thread_operation(BLE_WRITE_THREAD, THREAD_DISABLED);
	set_thread_operation(BLE_READ_THREAD, THREAD_DISABLED);

	return err;
}

int parse_ble_message(uint8_t *message, uint8_t message_length, mb_msg_dest message_origin,
		      mb_msg_action message_action, uint8_t port, uint8_t msg_max_rsp_len)
{
	int err = 0;
	uint8_t id, len;
	// Put message in rx buffer
	if (message_action == MB_MSG_SEND) {
		send_ble_message(message, message_length, port);
	} else if (message_action == MB_MSG_EXECUTE) {
		// Parse command message
		id = message[0];
		len = message[1];
		switch (id) {
		case CMD_GET_BLE_SCAN: {
			// Send via bt
			LOG_INF("Send BT scan data via BT");
			len = compose_message_bt_scan_results(message + MESSAGE_HEAD_LEN,
							      msg_max_rsp_len - MESSAGE_HEAD_LEN,
							      BT_SCAN_MAX_RES);
			message[0] = Main_messages.msg_ble_scan_aggregated->id;
			message[1] = len;
			len += MESSAGE_HEAD_LEN; // Add head
			port = Main_messages.msg_ble_scan_aggregated->port;
			break;
		}
		case CMD_GET_MAC: {
			message[0] = Main_messages.msg_mac_id->id;
			message[1] = Main_messages.msg_mac_id->length;
			get_mac(message + 2, Main_messages.msg_mac_id->length);
			port = Main_messages.msg_mac_id->port;
			len = Main_messages.msg_mac_id->length + 2;
			break;
		}
		case CMD_BT_DISCONNECT: {
			if (bt_is_connected()) {
				bt_con_disconnect();
			}
			len = 0;
			break;
		}
		case CMD_DISABLE_BT_TH: {
			stop_bt_service();
			// Response len
			len = compose_response_msg(message, err, &port);
			break;
		}
		case CMD_CHECK_PIN: {
			err = bt_con_check_pin(message + 2, len);
			len = compose_response_msg(message, err, &port);
			break;
		}
		case CMD_SINGLE_BT_SCAN: {
			if (!Main_settings.cmdq_enabled->def_val) {
				LOG_INF("Got command to execute BT single scan.");
				// Set response length to 0, as we do not need to send response to
				// sender
				len = 0;

				// Do not perform scan if we are connected
				if (bt_is_connected()) {
					break;
				}

				// Initiate single BT scan
				bt_scan();
				uint32_t scan_time = get_global_unix_time();
				bt_scan_send(message, msg_max_rsp_len, scan_time);
			} else {
				LOG_WRN("Got command to execute BT single scan. Cannot run scan "
					"with CMDQ enabled.");
			}
			break;
		}
		case CMD_AGGREGATED_BT_SCAN: {
			LOG_INF("Got command to compose BT scan aggregated message.");
			// Set response length to 0, as we do not need to send response to sender
			len = 0;

			bt_scan_send_aggregated(message, msg_max_rsp_len);
			break;
		}
		case CMD_SEND_BT_CMDQ_RESULTS: {
			LOG_INF("Got command to send gathered BT CMDQ data.");
			// Set response length to 0, as we do not need to send response to
			// sender
			len = 0;
#ifdef CONFIG_BT_CMDQ
			bt_cmdq_messaging_results_send(message, msg_max_rsp_len);
#else
			LOG_ERR("BT CMDQ not enabled!");
#endif /* CONFIG_BT_CMDQ */
			break;
		}
		}
		if (id == Main_settings.device_name->id) {
			// Update name
			ble_adv_device_name_update(Main_settings.device_name->def_val,
						   Main_settings.device_name->len);
			len = 0;
		} else if (id == Main_settings.ble_adv->id) {
			if (Main_settings.ble_adv->def_val) {
				err = start_bt_advertisement();
				// Response len
				len = 0;
			} else {
				err = stop_bt_advertisement();
				// Response len
				len = 0;
			}
		} else if (id == Main_settings.ble_advertisement_interval->id) {
			LOG_INF("Update BT advertisement interval!");
			err = stop_bt_advertisement();
			ble_adv_set_interval(Main_settings.ble_advertisement_interval->def_val);
			err = start_bt_advertisement();
		}

		// Send response
		if (len > 0) {
			// Send via LR
			if (message_origin == MB_MSG_LORA) {
				err = thread_put_message(MB_MSG_DEV, MB_MSG_LORA, MB_MSG_SEND, port,
							 message, len, msg_max_rsp_len);
			} else if (message_origin == MB_MSG_BT) {
				send_ble_message(message, len, port);
			}
		}
	} else {
		err = -EIO;
	}

	return err;
}

void send_ble_message(uint8_t *message, uint8_t message_length, uint8_t port)
{
	nus_data_t *tx = k_malloc(sizeof(*tx));
	tx->len = (uint16_t)message_length + 1;
	LOG_INF("Add message of length %d to BLE buffer", tx->len);
	memcpy(tx->data + 1, message, tx->len);
	tx->data[0] = port;
	put_fifo_tx_data(tx);
	tx = NULL;
}

bool check_adv_data_update_period(void)
{
	if (bt_is_connected() || !Main_settings.ble_adv->def_val || !prv_adv_enabled) {
		return false;
	}
	if ((uint32_t)(k_uptime_get() - adv_data_update) > adv_data_update_period ||
	    adv_data_update == 0) {
		adv_data_update = k_uptime_get();
		return 1;
	}
	return 0;
}

void get_mac(uint8_t *mac, uint8_t len)
{
	// Get advertisement mac address
	bt_addr_le_t addr;
	size_t count = 6;
	bt_id_get(&addr, &count);

	if (len > 8) {
		len = 8;
	}
	for (uint8_t i = 0; i < len; i++) {
		mac[i] = addr.a.val[i];
	}
}
