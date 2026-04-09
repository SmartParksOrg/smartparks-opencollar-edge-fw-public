/** @file bt_con.c
 *
 * @brief Interface for bt connection callbacks
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2020 Irnas. All rights reserved.
 */

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/types.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <bluetooth/services/nus.h>

#include <zephyr/mgmt/mcumgr/smp_bt.h>
#ifdef CONFIG_MCUMGR_CMD_IMG_MGMT
#include <img_mgmt/img_mgmt.h>
#endif /* CONFIG_MCUMGR_CMD_IMG_MGMT */
#ifdef CONFIG_MCUMGR_CMD_OS_MGMT
#include <os_mgmt/os_mgmt.h>
#endif /* CONFIG_MCUMGR_CMD_OS_MGMT */

#include "generated_settings.h"
#include "led.h"

#include "thread_com.h"
#include "thread_operation.h"

#include "bt_con.h"

#define BT_PIN_VALIDATION_TIMEOUT 10000 // In ms

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(BT_CON, 3);

struct bt_conn *current_conn = NULL;
struct bt_conn *auth_conn = NULL;
uint64_t bt_conn_time = 0;

static bool pin_validated = false; // Flag indicates if device pin is validated or not
static bool ignore_disconnect_period = false;
/* =======================================================================================================
 */
/* Static private functions */
/* =======================================================================================================
 */

/**
 * @brief BT connected callback.
 *
 * Check if pin is set and needs to be validated. Change thread sleep time to connected.
 * Blink connected LED.
 *
 * @param conn
 * @param err
 */
static void connected(struct bt_conn *conn, uint8_t err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	if (err) {
		LOG_ERR("Connection failed (err %u)\n", err);
		return;
	}

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	LOG_INF("Connected %s\n", addr);

	current_conn = bt_conn_ref(conn);
	bt_conn_time = k_uptime_get(); // Log start of connection

	// Check if pin needs to be validated - if defout pin is set to all 0, we will not check pin
	// code
	pin_validated = true;
	for (uint8_t i = 0; i < Main_settings.device_pin->len; i++) {
		if (Main_settings.device_pin->def_val[i] != 0) {
			pin_validated = false;
		}
	}
	LOG_INF("Pin needs validation: %d", pin_validated);

	thread_sleep = THREAD_CONNECTED_SLEEP; // Set faster operation time
	led_blink(5, LED_B);
}

/**
 * @brief BT disconnected callback.
 *
 * @param conn
 * @param reason
 */
static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	if (ignore_disconnect_period) {
		bt_con_ignore_disconnect_period_set(
			false); /* Re-enable automatic disconnection period */
	}

	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (reason == BT_HCI_ERR_REMOTE_USER_TERM_CONN) {
		LOG_INF("Disconnected by user: %s (reason %u)\n", addr, reason);
	} else if (reason == BT_HCI_ERR_CONN_TIMEOUT) {
		LOG_INF("Disconnected due to connection timeout: %s (reason %u)\n", addr, reason);
	} else {
		LOG_INF("Disconnected: %s (reason %u)\n", addr, reason);
	}

	if (auth_conn) {
		bt_conn_unref(auth_conn);
		auth_conn = NULL;
	}

	if (current_conn) {
		bt_conn_unref(current_conn);
		current_conn = NULL;
	}
	pin_validated = false; // Wait for pin validation

	thread_sleep = THREAD_LP_SLEEP;
}

/* BT security cganged callback */
#ifdef CONFIG_BT_SMP
static void security_changed(struct bt_conn *conn, bt_security_t level, enum bt_security_err err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (!err) {
		LOG_INF("Security changed: %s level %u\n", addr, level);
	} else {
		LOG_ERR("Security failed: %s level %u err %d\n", addr, level, err);
	}
}
#endif // CONFIG_BT_SMP

/* BT connection callbacks */
static struct bt_conn_cb conn_callbacks = {
	.connected = connected,
	.disconnected = disconnected,
#ifdef CONFIG_BT_SMP
	.security_changed = security_changed,
#endif // CONFIG_BT_SMP
};

#ifdef CONFIG_BT_SMP
/* Security BT callbacks */
static void auth_passkey_display(struct bt_conn *conn, unsigned int passkey)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("Passkey for %s: %06u\n", addr, passkey);
}

static void auth_passkey_confirm(struct bt_conn *conn, unsigned int passkey)
{
	char addr[BT_ADDR_LE_STR_LEN];

	auth_conn = bt_conn_ref(conn);

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("Passkey for %s: %06u\n", addr, passkey);
}

static void auth_cancel(struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("Pairing cancelled: %s\n", addr);
}

static void pairing_confirm(struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	bt_conn_auth_pairing_confirm(conn);

	LOG_INF("Pairing confirmed: %s\n", addr);
}

static void pairing_complete(struct bt_conn *conn, bool bonded)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("Pairing completed: %s, bonded: %d\n", addr, bonded);
}

static void pairing_failed(struct bt_conn *conn, enum bt_security_err reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("Pairing failed conn: %s, reason %d\n", addr, reason);
}

static struct bt_conn_auth_cb conn_auth_callbacks = {.passkey_display = auth_passkey_display,
						     .passkey_confirm = auth_passkey_confirm,
						     .cancel = auth_cancel,
						     .pairing_confirm = pairing_confirm};

static struct bt_conn_auth_info_cb conn_auth_info_callbacks = {.pairing_complete = pairing_complete,
							       .pairing_failed = pairing_failed};
#endif // CONFIG_BT_SMP

/* =======================================================================================================
 */
/* Public function */
/* =======================================================================================================
 */

int bt_con_init(void)
{
	int err = 0;

	/* initialize DFU service */
	/* enable DFU via Nordic DFU service */
#ifdef CONFIG_MCUMGR_CMD_OS_MGMT
	os_mgmt_register_group();
#endif /* CONFIG_MCUMGR_CMD_OS_MGMT */
#ifdef CONFIG_MCUMGR_CMD_IMG_MGMT
	img_mgmt_register_group();
#endif /* CONFIG_MCUMGR_CMD_IMG_MGMT */
#ifdef CONFIG_MCUMGR
	err = smp_bt_register();
	if (err) {
		LOG_ERR("smp_bt_register, err: %d", err);
		return err;
	}
#endif /* CONFIG_MCUMGR */

	/* Register connection cb */
	bt_conn_cb_register(&conn_callbacks);

#ifdef CONFIG_BT_SMP
	err = bt_conn_auth_cb_register(&conn_auth_callbacks);
	if (err) {
		LOG_ERR("Failed to register authorization callbacks.");
		return err;
	}
	err = bt_conn_auth_info_cb_register(&conn_auth_info_callbacks);
	if (err) {
		LOG_ERR("Failed to register authorization info callbacks.");
		return err;
	}
#endif // CONFIG_BT_SMP

	return err;
}

int bt_con_disconnect(void)
{
	/* Enable automatic disconnection period after manual disconnect post data offload */
	if (ignore_disconnect_period) {
		bt_con_ignore_disconnect_period_set(false);
	}

	if (current_conn) {
		return bt_conn_disconnect(current_conn, BT_HCI_ERR_REMOTE_LOW_RESOURCES);
	}

	return 0;
}

bool bt_is_connected(void)
{
	if (current_conn) {
		return true;
	}

	return false;
}

int bt_con_get_max_payload(void)
{
	if (!bt_is_connected()) {
		return 0;
	}

	return bt_nus_get_mtu(current_conn);
}

bool bt_pin_is_validated(void)
{
	return pin_validated;
}

void bt_con_check_disconnect_period(void)
{
	if (current_conn) {
		/* Check if we can ignore the disconnect period */
		if (ignore_disconnect_period) {
			return;
		}
		/* Check pin validation */
		if (!pin_validated &&
		    (uint32_t)(k_uptime_get() - bt_conn_time) > BT_PIN_VALIDATION_TIMEOUT) {
			LOG_ERR("Pin not validated! Disconnect");
			bt_conn_disconnect(current_conn, BT_HCI_ERR_REMOTE_LOW_RESOURCES);
		}
		if ((uint32_t)(k_uptime_get() - bt_conn_time) / 1000 >
			    Main_settings.ble_auto_disconnect->def_val &&
		    Main_settings.ble_auto_disconnect->def_val > 0) {

			LOG_ERR("Timeout for automatic disconnect reached!");
			bt_conn_disconnect(current_conn, BT_HCI_ERR_REMOTE_LOW_RESOURCES);
		}
	}
}

int bt_con_check_pin(uint8_t *pin, uint8_t pin_len)
{
	int err = 0;
	if (pin_len < 4) {
		LOG_ERR("Invalid PIN command!");
		err = -EIO;
	} else {
		// Validate pin code
		for (uint8_t i = 0; i < Main_settings.device_pin->len; i++) {
			// Check pin code
			LOG_INF("IN: %x PIN: %x", pin[i], Main_settings.device_pin->def_val[i]);
			if (Main_settings.device_pin->def_val[i] != pin[i]) {
				err = -EIO;
			}
		}

		// If pin is not valid, check if we got app key
		if (err < 0 && pin_len == Main_settings.app_key->len) {
			err = 0;
			for (uint8_t i = 0; i < Main_settings.app_key->len; i++) {
				// Check pin code
				LOG_INF("IN: %x KEY: %x", pin[i],
					Main_settings.app_key->def_val[i]);
				if (Main_settings.app_key->def_val[i] != pin[i]) {
					LOG_ERR("Mismatch IN: %x KEY: %x", pin[i],
						Main_settings.app_key->def_val[i]);
					err = -EIO;
				}
			}
		}
	}
	if (!err) {
		pin_validated = true;
	} else {
		// Disconnect
		LOG_ERR("Invalid PIN. Disconnect!");
		bt_conn_disconnect(current_conn, BT_HCI_ERR_REMOTE_LOW_RESOURCES);
	}

	return err;
}

void bt_con_reset_disconnect_timeout(void)
{
	bt_conn_time = k_uptime_get(); // Reset
}

void bt_con_ignore_disconnect_period_set(bool enable)
{
	LOG_WRN("Ignore disconnect period: %d", enable);
	ignore_disconnect_period = enable;
}
