/** @file bt_adv.c
 *
 * @brief Interface for ble BT advertisement
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
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/hci_vs.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/sys/byteorder.h>

#include "bt_adv.h"

#define DFU_UUID_SERVICE BT_UUID_128_ENCODE(0x84aa6074, 0x528a, 0x8b86, 0xd34c, 0xb71d1ddc538d)
#define BLE_ADV_SCALING  0.625

LOG_MODULE_REGISTER(ble_adv);

static struct bt_le_adv_param *adv_param = BT_LE_ADV_PARAM(BT_LE_ADV_OPT_CONNECTABLE,
							   800, //(N * 0.625 = 500ms)
							   832, //(N * 0.625 = 520ms)
							   NULL);

static char device_name[DEVICE_NAME_LEN_TOTAL];

static uint8_t adv_data[MAX_ADVERTISEMENT_LEN];

static const struct bt_data ad[] = {
	BT_DATA(BT_DATA_MANUFACTURER_DATA, adv_data, MAX_ADVERTISEMENT_LEN),
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA(BT_DATA_NAME_COMPLETE, device_name, DEVICE_NAME_LEN_TOTAL)};

/* currently NUS (Nordic Uart Service) is not advertised but it is available to user after
 * connection*/
#ifdef DFU_UUID_SERVICE
static const struct bt_data sd[] = {
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, /*NUS_UUID_SERVICE,*/ DFU_UUID_SERVICE),
};
#else
static const struct bt_data sd[] = {
	BT_DATA_BYTES(BT_DATA_UUID128_ALL /*, NUS_UUID_SERVICE*/),
};
#endif

void ble_adv_init(char *new_name, uint8_t len)
{
	// Set
	ble_adv_device_name_update(new_name, len);

	adv_data[0] = BT_MANUFACTURER_DATA & 0x00ff;
	adv_data[1] = BT_MANUFACTURER_DATA >> 8;
}

int ble_adv_start(void)
{
	return bt_le_adv_start(adv_param, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
}

int ble_adv_stop(void)
{
	return bt_le_adv_stop();
}

void ble_adv_device_name_update(char *new_name, uint8_t len)
{
	// Check if valid new device name is provided
	if (len == 0 || !new_name[0]) {
		LOG_INF("Empty device name string passed. Construct default name.");
		const char _name[] = DEVICE_NAME;
		for (int i = 0; i < DEVICE_NAME_LEN; i++) {
			device_name[i] = _name[i];
		}
		// Get advertisement mac address
		bt_addr_le_t addr;
		size_t count = 6;
		bt_id_get(&addr, &count);

		for (int i = 0; i < (DEVICE_NAME_LEN_MAC / 2); i++) {
			char tmp[3];
			sprintf(&tmp[0], "%02x", addr.a.val[i]);
			device_name[DEVICE_NAME_LEN_TOTAL - 2 * (i + 1)] = tmp[0];
			device_name[DEVICE_NAME_LEN_TOTAL - 2 * (i + 1) + 1] = tmp[1];
		}
	} else {

		uint8_t new_len = MIN(len, DEVICE_NAME_LEN_TOTAL);
		// Erase old name
		for (int i = 0; i < DEVICE_NAME_LEN_TOTAL; i++) {
			device_name[i] = '\0';
		}
		for (int i = 0; i < new_len; i++) {
			device_name[i] = new_name[i];
		}
	}
	LOG_INF("Set adv name: %c %c %c %c %c %c %c %c", device_name[0], device_name[1],
		device_name[2], device_name[3], device_name[4], device_name[5], device_name[6],
		device_name[7]);

	bt_le_adv_update_data(ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
}

char *ble_adv_get_device_name(void)
{
	return device_name;
}

void ble_adv_set_interval(uint32_t interval)
{
	// Check stored interval
	LOG_INF("BLE advertising interval: %d ms", interval);
	adv_param->interval_min = (uint32_t)(interval / BLE_ADV_SCALING);
	adv_param->interval_max = (uint32_t)(interval / BLE_ADV_SCALING) + 32;
}

int ble_adv_data_update(uint8_t *data, uint8_t len)
{
	if (len > MAX_ADVERTISEMENT_LEN) {
		LOG_ERR("Invalid adv data length!");
		return -EIO;
	}
	memcpy(adv_data + 2, data, len);
	int err = bt_le_adv_update_data(ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));

	return err;
}

void ble_adv_data_set(uint8_t *data, uint8_t len)
{
	if (len > MAX_ADVERTISEMENT_LEN) {
		LOG_ERR("Invalid adv data length!");
		return;
	}
	memcpy(adv_data + 2, data, len);
}
