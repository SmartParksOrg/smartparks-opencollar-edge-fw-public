#include <zephyr/bluetooth/hci.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(bt_scan_mac_sample);

/**
 * @brief Mac address of the advertising bluetooth device
 *
 * IMPORTANT: If you are not using this sample with the elephant BLE sub-skin implant checkout the
 * README.md for further instructions.
 */
#define MAC 0xe4daac6810d8

/**
 * @brief Bluetooth scan interval (seconds)
 * Sets the waiting time in between scans
 * Accepted values: [0  ≈4300000000]s
 */
#define SCAN_INTERVAL 0

/**
 * @brief Bluetooth scan duration
 * Sets the duration of each performed scan.
 *
 * Accepted values: [50  200000]ms
 * NOTE: Blocking operation.
 */
#define SCAN_DURATION 10000

/**
 * @brief Number of scans
 * Sets the number of bluetooth scans to be performed before exiting.
 *
 */
#define NUMBER_OF_SCANS 100

/**
 * @brief Split the provided mac address into an array of bytes
 *
 * @param[in] mac_addr mac address
 *
 */
#define MAC_ADDR_TO_BYTE_ARRAY(mac_addr)                                                           \
	{                                                                                          \
		.val = {(uint8_t)((mac_addr >> 40) & 0xFF),                                        \
			(uint8_t)((mac_addr >> 32) & 0xFF),                                        \
			(uint8_t)((mac_addr >> 24) & 0xFF),                                        \
			(uint8_t)((mac_addr >> 16) & 0xFF),                                        \
			(uint8_t)((mac_addr >> 8) & 0xFF),                                         \
			(uint8_t)(mac_addr & 0xFF) }                                               \
	}

/**
 * @brief Scan parameters
 * NOTE: Parameters can be personalized based on your use-case.
 * More information can be found following the links below
 * https://www.bluetooth.com/specifications/specs/scan-parameters-profile-1-0/
 * https://www.bluetooth.com/specifications/specs/scan-parameters-service-1-0/
 *
 */
static const struct bt_le_scan_param scan_param = {
	.type = BT_HCI_LE_SCAN_ACTIVE,
	.options = BT_LE_SCAN_OPT_FILTER_DUPLICATE,
	.interval = 32, //(N * 0.625 ms)
	.window = 32,   //(N * 0.625 ms)
	.timeout = 0,
	.interval_coded = 0,
	.window_coded = 0,
};

/**
 * @brief Check if the two provided mac addresses match
 *
 * Does not check address type.
 *
 * @param[in] mac1 first mac address
 * @param[in] mac2 second mac address
 *
 * @return true if the mac address matches, false otherwise
 */
static bool mac_addr_match(const bt_addr_le_t *mac1, const bt_addr_le_t *mac2)
{
	return memcmp(mac1->a.val, mac2->a.val, sizeof(mac1->a.val)) == 0;
}

/**
 * @brief Print the advertising type
 *
 * @param[in] adv_type advertising type
 */
static void print_adv_pdu_type(uint8_t adv_type)
{
	if (adv_type == BT_GAP_ADV_TYPE_ADV_IND) {
		LOG_INF("Type: ADV_IND (scannable, connectable)");
	}
	if (adv_type == BT_GAP_ADV_TYPE_ADV_DIRECT_IND) {
		LOG_INF("Type: ADV_DIRECT_IND (directed, connectable)");
	}
	if (adv_type == BT_GAP_ADV_TYPE_ADV_SCAN_IND) {
		LOG_INF("Type: ADV_SCAN_IND (scannable, non-connectable)");
	}
	if (adv_type == BT_GAP_ADV_TYPE_ADV_NONCONN_IND) {
		LOG_INF("Type: ADV_NONCONN_IND (non-scannable, non-connectable)");
	}
	if (adv_type == BT_GAP_ADV_TYPE_SCAN_RSP) {
		LOG_INF("Type: SCAN_RESPONSE (additional advertising data requested by an active "
			"scanner)");
	}
	if (adv_type == BT_GAP_ADV_TYPE_EXT_ADV) {
		LOG_INF("Type EXTENDED ADV (extended advertising, see advertising properties)");
	}
}

/**
 * @brief Process the scan data
 *
 * As this is a sample use-casing the Elephant sub-skin BLE implant, we're displaying case specific
 * data. More on this: https://github.com/IRNAS/smartparks-opencollar-edge-fw/issues/389
 *
 * IMPORTANT: If you're using this sample for your own use-case, you'll need to change the contents
 * to fit your use-case.
 *
 * @param[in] data scan data buffer
 * @param[in] len scan data length
 *
 * @return 0 if successful, negative error code otherwise
 */
static void process_scan_data(const uint8_t *data, size_t len)
{
	if (len < 25) {
		LOG_ERR("Scan data length is too short. Are you sure you're scanning the right "
			"device?");
		return;
	}
	LOG_INF("5 Minute RR Median: %d", data[17]);
	LOG_INF("5 Minute RR Median Modesum: %d", data[18]);
	LOG_INF("5 Minute Activity Average: %d", data[19]);
	LOG_INF("5 Minute Activity Max: %d", data[20]);
	LOG_INF("Active Minutes in the last hour: %d", data[21]);
	LOG_INF("Temperature: %d", (uint16_t)data[22] << 8 | data[23]);
	LOG_INF("Hourly Impedance: %d", (uint16_t)data[24] << 8 | data[25]);
}

/**
 * @brief BT scan callback
 *
 * This function is called for every advertising packet received.
 *
 * If the packet originates from the configured MAC address and is
 * a scan response packet, parse it using process_scan_data().
 *
 * @param[in] addr - bt address
 * @param[in] rssi - result rssi
 * @param[in] adv_type - advertising type
 * @param[in] buf - data buffer
 */
static void scan_cb(const bt_addr_le_t *addr, int8_t rssi, uint8_t adv_type,
		    struct net_buf_simple *buf)
{
	const bt_addr_le_t MAC_addr_le = {.type = BT_ADDR_LE_PUBLIC,
					  .a = MAC_ADDR_TO_BYTE_ARRAY(MAC)};
	/* Check if mac addresses match */
	if (mac_addr_match(addr, &MAC_addr_le)) {
		LOG_INF("-------------------------");
		LOG_INF("MATCH FOUND");

		/* Check advertising type */
		print_adv_pdu_type(adv_type);

		LOG_INF("RSSI: %d", rssi);
		LOG_HEXDUMP_INF(buf->data, buf->len, "Complete data: ");
		/* Print scan data if advertisement type is SCAN RESPONSE */
		if (adv_type == BT_GAP_ADV_TYPE_SCAN_RSP) {
			process_scan_data(buf->data, buf->len);
		}
	}
}

/*
 * @brief Initiate BT scan
 *
 * @return 0 if successful, negative error code otherwise
 */
static int bt_scan(void)
{
	int err = 0;
	LOG_INF("Start BLE scan.");

	err = bt_le_scan_start(&scan_param, scan_cb);
	if (err) {
		LOG_ERR("BLE scanning failed to start (err %d).", err);
		return err;
	}
	/* Sleep during bluetooth scan */
	k_sleep(K_MSEC(SCAN_DURATION));
	err = bt_le_scan_stop();
	if (err) {
		LOG_ERR("BLE scanning failed to stop (err %d).", err);
		return err;
	}
	LOG_INF("Stopped BLE scan.");
	return err;
}

int main(void)
{
	/* Init BLE */
	int err = bt_enable(NULL);
	if (err) {
		LOG_ERR("Bluetooth init failed (err %d)", err);
		return err;
	}
	LOG_INF("Bluetooth initialized");
	/* Start scanning */
	LOG_INF("BT scan sample started.");
	while (true) {
		bt_scan();
		k_sleep(K_SECONDS(SCAN_INTERVAL));
	}
	return 0;
}
