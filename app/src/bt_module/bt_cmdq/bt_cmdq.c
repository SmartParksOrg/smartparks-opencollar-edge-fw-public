/** @file bt_cmdq.c
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
 * COPYRIGHT NOTICE: (c) 2024 Irnas. All rights reserved.
 */

#include <zephyr/bluetooth/hci.h>
#include <zephyr/logging/log.h>
#include <stdatomic.h>

#include "bt_cmdq.h"
#include "bt_con.h"
#include "bt_interface.h"
#include "global_time.h"
#include "settings_def.h"
#include "zephyr/kernel.h"

LOG_MODULE_REGISTER(BT_CMDQ);

/* Size of data buffer provided by CMDQ */
#define CMDQ_DATA_LEN 31

/**
 * @brief Configuration structure containing the dependency injected callback function.
 *
 * This callback function is called when a scan response from the configured mac address is
 * received.
 */
static struct bt_cmdq_cfg {
	cmdq_callback_t ext_cb;
} cfg;

/**
 * @brief Bt scan structure for ACTIVE scanning
 *
 * We need active scanning to request a scan response from the device.
 */
static struct bt_le_scan_param scan_param = {
	.type = BT_HCI_LE_SCAN_ACTIVE,
	.options = BT_LE_SCAN_OPT_FILTER_DUPLICATE,
	.interval = 32,
	.window = 32,
	.timeout = 0,
	.interval_coded = 0,
	.window_coded = 0,
};

/**
 * @brief Available scan types.
 */
enum scan_type {
	/* Immediate scan
	 * When starting the CMDQ operation, an immediate scan is performed, because we don't have
	 * any chronological data of the devices advertisement. The search interval is set to 0, so
	 * the scanning begins immediately.
	 *
	 * In short: The same as SCAN_LONG, but with an immediate search interval.
	 */
	SCAN_IMMEDIATE,
	/* Short scan
	 * This scan type uses the configured scan duration and search interval.
	 * The search interval is shortened for half the scan duration to avoid missing a scan
	 * response.
	 */
	SCAN_SHORT,
	/* Long scan
	 * When a match is not found with a short scan a long scan is performed.
	 * A long scans scan duration is the configured search interval (Not to be confused with the
	 * configured `scan_duration`!), as we don't have any chronological data of the devices
	 * advertisement we need to scan for one whole advertisement cycle.
	 */
	SCAN_LONG,
	/* Scan timeout
	 * When a match is not found with a long scan, a configured timeout period is started.
	 * After expiring, a scan is performed. Scan duration is the configured search interval.
	 *
	 * In short: The same as SCAN_LONG, but with a really long search interval.
	 */
	SCAN_TIMEOUT
};

/**
 * @brief Scan arguments consisting of scan type and scan duration.
 *
 */
struct scan_args {
	enum scan_type scan_type;
	int32_t scan_duration;
};

static atomic_bool prv_cmdq_scan_enabled = false;
static atomic_bool prv_successful_scan = false;
static struct scan_args prv_scan_args;

/*********************************************************/
/*                Private functions                       */
/*********************************************************/

/**
 * @brief BT_CMDQ scan handler
 *
 * @param[in] prv_scan_type one of the available scan types
 *
 * @return int 0 on success negative error code otherwise
 */
static void prv_scan_handler(enum scan_type prv_scan_type);

/**
 * @brief Check if BT is ready, if not, try to init.
 *
 * @return int 0 on success, negative error code otherwise.
 */
static int prv_check_bt(void)
{
	if (!bt_is_ready()) {
		LOG_INF("BT not ready, trying to init.");
		int err = bt_enable(NULL);
		if (err) {
			LOG_ERR("Bluetooth init failed (err %d).", err);
			return err;
		}
	}
	return 0;
}

/**
 * @brief check if mac addresses of a bt_addr_le_t structure match
 *
 * @param[in] mac1 first mac address
 * @param[in] mac2 second mac address
 *
 * @return true if mac addresses match, false otherwise
 */
static bool prv_mac_addr_match(const bt_addr_le_t *mac1, const bt_addr_le_t *mac2)
{
	return memcmp(mac1->a.val, mac2->a.val, sizeof(mac1->a.val)) == 0;
}

/**
 * @brief Stop bluetooth scan handler.
 *
 * This function is called when the scan timer expires. It stops the scan.
 *
 * @param[in] work
 */
static void prv_work_scan_stop_handler(struct k_work *work)
{
	int err = bt_le_scan_stop();
	if (err) {
		LOG_ERR("Stop scan failed (err %d).", err);
	}

	/* Restart Bluetooth advertisement */
	err = start_bt_advertisement();
	if (err) {
		LOG_DBG("BLE advertisement start failed (err %d).", err);
	}

	/* Send empty message */
	if (cfg.ext_cb && !prv_successful_scan) {
		cfg.ext_cb(NULL, 0, get_global_unix_time());
	}
}

/* Register stop scan work */
K_WORK_DEFINE(work_scan_stop, prv_work_scan_stop_handler);

/**
 * @brief Bluetooth scan timer stop callback.
 *
 * When the scan stop timer expires, this function is called. It stops the scan and schedules a new
 * one
 *
 * @param[in] k_timer timer structure
 */
static void prv_scan_stop_timer_cb(struct k_timer *k_timer)
{
	/* BT scan stop is blocking */
	k_work_submit(&work_scan_stop);

	/* If scan was unsuccessful */
	if (!prv_successful_scan) {
		LOG_INF("Scan unsuccessful.");

		/* Schedule new scan */
		switch (prv_scan_args.scan_type) {
		case SCAN_IMMEDIATE:
		case SCAN_SHORT:
			prv_scan_handler(SCAN_LONG);
			break;
		case SCAN_LONG:
		case SCAN_TIMEOUT:
			prv_scan_handler(SCAN_TIMEOUT);
			break;
		}
	} else {
		prv_scan_handler(SCAN_SHORT);
	}
}

K_TIMER_DEFINE(scan_stop_timer, prv_scan_stop_timer_cb, prv_scan_stop_timer_cb);

/**
 * @brief Get bt_addr_le_t structure that contains the MAC address from the configured Main
 * settings.
 *
 * @return bt_addr_le_t structure
 */
static bt_addr_le_t prv_settings_MAC_addr_le(void)
{
	bt_addr_t address = {.val[0] = Main_settings.cmdq_searched_mac_address->def_val[5],
			     .val[1] = Main_settings.cmdq_searched_mac_address->def_val[4],
			     .val[2] = Main_settings.cmdq_searched_mac_address->def_val[3],
			     .val[3] = Main_settings.cmdq_searched_mac_address->def_val[2],
			     .val[4] = Main_settings.cmdq_searched_mac_address->def_val[1],
			     .val[5] = Main_settings.cmdq_searched_mac_address->def_val[0]};
	bt_addr_le_t MAC_addr_le = {.type = BT_ADDR_LE_PUBLIC, .a = address};
	return MAC_addr_le;
}

/**
 * @brief BT scan callback
 *
 * This function is called for every advertising packet received.
 *
 * If the packet originates from the configured MAC address and is
 * a scan response packet, parse it and send it to the provided external function.
 *
 * Immediately after finding a match, the `scan_stop_timer` is triggered, stopping the current scan
 * and scheduling a new scan.
 *
 * @param[in] addr - bt address
 * @param[in] rssi - result rssi
 * @param[in] adv_type - advertising type
 * @param[in] buf - data buffer
 */
static void prv_scan_cb(const bt_addr_le_t *addr, int8_t rssi, uint8_t adv_type,
			struct net_buf_simple *buf)
{
	bt_addr_le_t MAC_addr_le = prv_settings_MAC_addr_le();

	/* Check if mac addresses match */
	if (prv_mac_addr_match(addr, &MAC_addr_le)) {
		/* If a match was found, schedule a new scan */
		prv_scan_handler(SCAN_SHORT);

		if (adv_type == BT_GAP_ADV_TYPE_SCAN_RSP) {
			LOG_INF("-------------------------");
			LOG_INF("MATCH FOUND");
			LOG_INF("RSSI: %d", rssi);
			LOG_HEXDUMP_INF(buf->data, buf->len, "Complete data: ");
			/* Print scan data if advertisement type is SCAN RESPONSE */
			prv_successful_scan = true;
			k_timer_stop(&scan_stop_timer);
			if (buf->len > CMDQ_DATA_LEN) {
				LOG_ERR("Scan data length is too long. Are you sure you're "
					"scanning the right device?");
				return;
			}
			if (cfg.ext_cb) {
				cfg.ext_cb(buf->data, buf->len, get_global_unix_time());
			} else {
				LOG_WRN("No callback function provided.");
			}
		}
	}
}

/**
 * @brief Handler for starting bluetooth CMDQ scanning.
 *
 * @param[in] work work structure
 */
static void prv_work_scan_start_handler(struct k_work *work)
{
	int err;
	/* Stop bluetooth advertisement during scan */
	err = stop_bt_advertisement();
	if (err) {
		LOG_DBG("BLE advertisement stop failed (err %d).", err);
		return;
	}
	/* Check if BT is connected and disconnect if needed */
	if (bt_is_connected()) {
		err = bt_con_disconnect();
		if (err) {
			LOG_ERR("Disconnect failed (err %d).", err);

			/* If disconnect failed, restart advertisement and return */
			err = start_bt_advertisement();
			if (err) {
				LOG_DBG("BLE advertisement start failed (err %d).", err);
			}
			return;
		}
	}

	/* Start scan */
	LOG_INF("Start BT - CMDQ scan. Scan type: %s",
		prv_scan_args.scan_type == SCAN_IMMEDIATE ? "SCAN_IMMEDIATE"
		: prv_scan_args.scan_type == SCAN_SHORT   ? "SCAN_SHORT"
		: prv_scan_args.scan_type == SCAN_LONG    ? "SCAN_LONG"
		: prv_scan_args.scan_type == SCAN_TIMEOUT ? "SCAN_TIMEOUT"
							  : "UNKNOWN");
	prv_successful_scan = false;
	err = bt_le_scan_start(&scan_param, prv_scan_cb);
	if (err) {
		LOG_ERR("Scan start failed (err %d).", err);
		/* Scan unsuccessful, stop timer and operation */
		k_timer_start(&scan_stop_timer, K_NO_WAIT, K_NO_WAIT);
	}

	/* Stop scan timer */
	k_timer_start(&scan_stop_timer, K_MSEC(prv_scan_args.scan_duration), K_NO_WAIT);
}

/* Register start scan work */
K_WORK_DEFINE(work_scan_start, prv_work_scan_start_handler);

/**
 * @brief start BT CMDQ scan timer callback and work submitter.
 *
 * @param[in] k_timer timer structure
 */
static void prv_scan_timer_cb(struct k_timer *k_timer)
{
	k_work_submit(&work_scan_start);
}

/* Register scan timer */
K_TIMER_DEFINE(scan_timer, prv_scan_timer_cb, prv_scan_timer_cb);

/**
 * @brief Bluetooth scan handler. Starts different timers for the specified scan types.
 *
 * @param scan_type bluetooth scan type	setting that controls the timer. Check `scan_type` above for
 * detailed definitions.
 */
static void prv_scan_handler(enum scan_type prv_scan_type)
{
	if (!prv_cmdq_scan_enabled) {
		LOG_WRN("Scan disabled.");
		return;
	}

	prv_scan_args.scan_type = prv_scan_type;

	switch (prv_scan_type) {
	case SCAN_IMMEDIATE:
		prv_scan_args.scan_duration =
			Main_settings.cmdq_search_interval->def_val * 1000; // convert to ms
		k_timer_start(&scan_timer, K_NO_WAIT, K_NO_WAIT);
		break;

	case SCAN_SHORT:
		prv_scan_args.scan_duration = Main_settings.cmdq_scan_duration->def_val;
		k_timer_start(&scan_timer,
			      K_MSEC(Main_settings.cmdq_search_interval->def_val * 1000 -
				     (Main_settings.cmdq_scan_duration->def_val / 2)),
			      K_NO_WAIT);
		break;

	case SCAN_LONG:
		prv_scan_args.scan_duration =
			Main_settings.cmdq_search_interval->def_val * 1000; // convert to ms
		k_timer_start(&scan_timer, K_SECONDS(Main_settings.cmdq_search_interval->def_val),
			      K_NO_WAIT);
		break;

	case SCAN_TIMEOUT:
		prv_scan_args.scan_duration =
			Main_settings.cmdq_search_interval->def_val * 1000; // convert to ms
		k_timer_start(&scan_timer,
			      K_SECONDS(Main_settings.cmdq_on_no_detection_wait_duration->def_val),
			      K_NO_WAIT);
		break;
	default:
		LOG_ERR("Unknown scan type.");
		break;
	}
}

/*********************************************************/
/*                Public functions                       */
/*********************************************************/

int bt_cmdq_start_operation(cmdq_callback_t cb)
{
	if (!Main_settings.cmdq_enabled->def_val) {
		LOG_WRN("CMDQ operation not enabled.");
		return -EPERM;
	}
	if (prv_cmdq_scan_enabled == true) {
		LOG_INF("Scan already enabled.");
		return -EALREADY;
	}
	cfg.ext_cb = cb;
	prv_cmdq_scan_enabled = true;

	/* Check bluetooth */
	int err = prv_check_bt();
	if (err) {
		LOG_ERR("Bluetooth check failed (err %d).", err);
		return err;
	}

	/* Start scan */
	prv_scan_handler(SCAN_IMMEDIATE);

	return 0;
}

void bt_cmdq_stop_operation(void)
{
	if (prv_cmdq_scan_enabled == false) {
		LOG_INF("Scan already disabled.");
		return;
	}
	prv_cmdq_scan_enabled = false;
	prv_successful_scan = false;
	k_timer_stop(&scan_stop_timer);
}

bool bt_cmdq_is_operation_started(void)
{
	return prv_cmdq_scan_enabled;
}
