/** @file settings_interface.c
 *
 * @brief Interface for the settings structures and functions. Allows for accessing settings
 * structures, updates from and to nvs.
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2020 Irnas. All rights reserved.
 */

#include "lp0.h"
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/reboot.h>

#ifdef CONFIG_BT_CMDQ
#include "bt_cmdq_messaging.h"
#endif /* CONFIG_BT_CMDQ */

#include "common_functions.h"
#include "generated_settings.h"
#include "global_time.h"
#include "led.h"
#include "nvs_storage.h"
#include "status.h"
#include "thread_com.h"
#include "thread_operation.h"

#include "settings_interface.h"

LOG_MODULE_REGISTER(settings_interface); // init logging

static uint16_t prv_sent_settings_counter = 0; /* Internal counter for sent settings */
#ifdef CONFIG_BOARD_COLLAREDGE_NRF52840
/**
 * @brief decouple collar by setting GPIO pin to high for 5 seconds. (CollarEdge specific feature)
 *
 * CollarEdge has a specific port used for the decoupling / releasing of the collar. On command the
 * device turns on the `ext_out_sw_en` GPIO which powers the decoupling connector for 5 seconds.
 */
static void prv_decouple_collar(void)
{
	LOG_INF("Decoupling collar.");
	const struct gpio_dt_spec gpio_ext_out_sw_en =
		GPIO_DT_SPEC_GET(DT_NODELABEL(ext_out_sw_en), gpios);
	gpio_pin_configure_dt(&gpio_ext_out_sw_en, GPIO_OUTPUT_ACTIVE | GPIO_ACTIVE_HIGH);
	gpio_pin_set_dt(&gpio_ext_out_sw_en, 1);

	k_sleep(K_SECONDS(5));
	gpio_pin_set_dt(&gpio_ext_out_sw_en, 0);
}
#endif /* CONFIG_BOARD_COLLAREDGE_NRF52840 */

/*!
 * @brief Initialize GPS position, from storage or settings if not stored yet.
 *
 * @return void
 */
static void prv_init_reference_position(void)
{
	// Initialize assistance position
	uint32_t ref_lat = 0, ref_lon = 0, ref_alt = 0;
	// Latitude
	if (nvs_storage_read(STORAGE_latitude, &ref_lat, sizeof(ref_lat)) != sizeof(ref_lat)) {
		ref_lat = Main_settings.gps_init_lat->def_val;
		nvs_storage_write(STORAGE_latitude, &Main_settings.gps_init_lat->def_val,
				  sizeof(Main_settings.gps_init_lat->def_val));
	}
	Main_values.gps_lat->def_val = ref_lat;

	// Longitude
	if (nvs_storage_read(STORAGE_longitude, &ref_lon, sizeof(ref_lon)) != sizeof(ref_lon)) {
		ref_lon = Main_settings.gps_init_lon->def_val;
		nvs_storage_write(STORAGE_longitude, &Main_settings.gps_init_lon->def_val,
				  sizeof(Main_settings.gps_init_lon->def_val));
	}
	Main_values.gps_lon->def_val = ref_lon;

	// Altitude
	if (nvs_storage_read(STORAGE_altitude, &ref_alt, sizeof(ref_alt)) != sizeof(ref_alt)) {
		ref_alt = 0;
	}
	Main_values.gps_alt->def_val = ref_alt;
}

/**
 * @brief Reset reference GPS position.
 *
 * @return negative integer error code, 0 - OK
 */
static void prv_reset_reference_position(void)
{
	// Initialize assistance position
	uint32_t ref_lat = 0, ref_lon = 0;

	ref_lat = Main_settings.gps_init_lat->def_val;
	nvs_storage_write(STORAGE_latitude, &Main_settings.gps_init_lat->def_val,
			  sizeof(Main_settings.gps_init_lat->def_val));
	Main_values.gps_lat->def_val = ref_lat;

	ref_lon = Main_settings.gps_init_lon->def_val;
	nvs_storage_write(STORAGE_longitude, &Main_settings.gps_init_lon->def_val,
			  sizeof(Main_settings.gps_init_lon->def_val));
	Main_values.gps_lon->def_val = ref_lon;
}

/**
 * @brief Update individual setting by ID from NVS.
 *
 * @param[in] uint8_t id          Unique setting ID.
 * @param[in] uint8_t len         Expected length of setting in bytes.
 *
 * @retval  Error message, return 0 if ok.
 */
static int prv_update_from_nvs(uint8_t id, uint8_t len)
{
	// Prepare storage buffer
	uint8_t stored_data[len];
	// Try to read data - if successful, overwrite settings
	int res = nvs_storage_read((uint16_t)id, stored_data, len);
	// Got predicted number of bytes
	if (res == len) {
		// Write to storage setting
		set_setting_value_by_id(id, stored_data, len);
		LOG_DBG("Setting with id: %x of len: %d, updated from NVS storage with data of "
			"len: %d!",
			id, len, res);
	}
	// Got error
	else if (res < 0) {
		LOG_DBG("Setting with id: %x is not yet stored in NVS.", id);
		return -ENXIO;
	}
	// Wrong length
	else {
		LOG_ERR("Wrong datasize stored in NVS for id: %x. Expected %d, got %d bytes. "
			"Ignore!",
			id, len, res);
		return -ENOTSUP;
	}
	return 0;
}

/*!
 * @brief Update all settings in Main_settings structure from nvs, if data with ID is stored.
 *
 * @retval  Return 0 if OK or -err.
 */
static int prv_update_all_from_nvs(void)
{
	LOG_INF("*   Update settings from NVS   *");
	// Loop over settings ID list
	for (uint8_t i = 0; i < Main_settings.n_settings; i++) {
		prv_update_from_nvs(Main_settings.settings_id[i], Main_settings.settings_length[i]);
	}
	LOG_INF("*   Loading settings from NVS done   *\n");
	return 0;
}

/*!
 * @brief Clear individual setting by ID from NVS.
 *
 * @param[in] uint8_t id          Unique setting ID.
 *
 * @retval  Error message, return 0 if ok.
 */
static int prv_clear_from_nvs(uint8_t id)
{
	return nvs_storage_delete(id);
}

/*!
 * @brief Clear all settings in Main_settings structure from nvs, if data with ID is stored.
 *
 * @retval  Return 0 if OK or -err.
 */
static int prv_clear_all_from_nvs(void)
{
	LOG_INF("*   Clear settings from NVS   *");
	// Loop over settings ID list
	int err = 0;
	for (uint8_t i = 0; i < Main_settings.n_settings; i++) {
		int tmp_err = prv_clear_from_nvs(Main_settings.settings_id[i]);
		if (tmp_err) {
			LOG_ERR("Failed to clear setting with ID: %d, got error: %d",
				Main_settings.settings_id[i], tmp_err);
			err = tmp_err;
		}
	}
	LOG_INF("*   Clear settings from NVS done   *");
	return err;
}

/*!
 * @brief Implement new setting if needed.
 *
 * @param[in] uint8_t port                  Port on which message was sent, to be replaced with
 * response port.
 * @param[in] mb_msg_dest *rsp_dest             Message origin, to be replaced with destination of
 * response message.
 * @param[in] mb_msg_action *rsp_action           Message action.
 * @param[in] uint8_t *rsp_message          Pointer to response Message.
 *
 * @retval  Error message, return 0 if ok.
 */
static int prv_execute_new_setting(mb_msg_dest *rsp_dest, mb_msg_action *rsp_action,
				   uint8_t *rsp_message, uint8_t id)
{
	// Check if setting id is in the structure
	if (check_setting_id(id)) {
		if (id == Main_settings.device_name->id || id == Main_settings.ble_adv->id ||
		    id == Main_settings.ble_advertisement_interval->id) {
			LOG_INF("Notify BT module, to update settings!");
			*rsp_dest = MB_MSG_BT;
			*rsp_action = MB_MSG_EXECUTE;
			rsp_message[0] = id;
			rsp_message[1] = 0;
			int rsp_len = 2;
			return rsp_len;
		} else if (id == Main_settings.led_enabled->id) {
			LOG_INF("Change led status!");
			led_change_status(Main_settings.led_enabled->def_val);
			return 0;
		} else if (id == Main_settings.satellite_enabled->id) {
			LOG_INF("Change satellite module status!");
			*rsp_dest = MB_MSG_SAT;
			*rsp_action = MB_MSG_EXECUTE;
			rsp_message[0] = id;
			rsp_message[1] = 0;
			int rsp_len = 2;
			return rsp_len;
		}
#ifdef CONFIG_FENCE_PORT
		else if (id == Main_settings.fence_enabled->id) {
			LOG_INF("Change fence status!");
			sys_features.fence = Main_settings.fence_enabled->def_val;
			return 0;
		}
#endif // CONFIG_FENCE_PORT
	} else {
		LOG_ERR("Received message with invalid setting id: %x!", id);
	}
	return 0;
}

/*!
 * @brief Return single value.
 *
 * @param[in] uint8_t *response_message      Pointer to response Message.
 * @param[in] uint8_t max_len               Max message len.
 *
 * @retval  Error message, return length if OK.
 */
static int prv_cmd_send_all_val(uint8_t *response_message, uint8_t max_len)
{
	int len = 0; // response message length
	// Loop over value ids
	for (uint8_t i = 0; i < Main_values.n_values; i++) {
		// Check if we did no exceed max length
		if (len + Main_values.values_length[i] + 2 < max_len) {
			response_message[len] = Main_values.values_id[i];
			len++;
			response_message[len] = Main_values.values_length[i];
			len++;
			uint8_t data[Main_values.values_length[i]];
			if (get_value_by_id(Main_values.values_id[i], data)) {
				memcpy(response_message + len, data, Main_values.values_length[i]);
			}
			len += Main_values.values_length[i];
		} else {
			return len;
		}
	}
	return len;
}

/*!
 * @brief Return single value.
 *
 * @param[in] uint8_t id                    Value id.
 * @param[in] uint8_t *response_message      Pointer to response Message.
 * @param[in] uint8_t max_len               Max message len.
 *
 * @retval  Error message, return length if OK.
 */
static int prv_cmd_send_single_val(uint8_t id, uint8_t *response_message, uint8_t max_len)
{
	// Check if value is present in Main_values structure
	if (check_value_id(id)) {
		// Create message to send via LoRa
		uint8_t len = get_value_len(id);
		if (len > 0 && len < max_len - 2) {
			response_message[0] = id;
			response_message[1] = len;
			uint8_t data[len];
			if (get_value_by_id(id, data)) {
				memcpy(response_message + 2, data, len);
				LOG_INF("Send value: %d", bytes_to_int32_t(data));
				return len + 2;
			}
		}
	} else {
		LOG_INF("Value ID: %x not present!", id);
	}

	return 0;
}

/*!
 * @brief Return single setting value.
 *
 * @param[in] uint8_t id                    Setting id.
 * @param[in] uint8_t *response_message      Pointer to response Message.
 * @param[in] uint8_t max_len               Max message len.
 *
 * @retval  Error message, return length if OK.
 */
static int prv_cmd_send_single_setting(uint8_t id, uint8_t *response_message, uint8_t max_len)
{
	// Check if value is present in Main_values structure
	if (check_setting_id(id)) {
		// Create message to send via LoRa
		uint8_t len = get_setting_len(id);
		if (len > 0 && len < max_len - 2) {
			response_message[0] = id;
			response_message[1] = len;
			uint8_t data[len];
			if (get_setting_by_id(id, data)) {
				memcpy(response_message + 2, data, len);
				return len + 2;
			}
		}
	} else {
		LOG_INF("Setting ID: %x not present!", id);
	}

	return 0;
}

/*!
 * @brief Fill response message buffer with settings until max length is reached.
 *
 * @param[in] uint8_t *response_message      Pointer to response Message.
 * @param[in] uint8_t max_len               Max message len.
 *
 * @retval  Error message, return length if OK.
 */
static int prv_cmd_send_all_settings(uint8_t *response_message, uint8_t max_len)
{
	int len = 0; // response message length
	for (uint8_t i = prv_sent_settings_counter; i < Main_settings.n_settings; i++) {
		// Check if we did no exceed max length
		if (len + Main_settings.settings_length[i] + 2 < max_len) {
			response_message[len] = Main_settings.settings_id[i];
			len++;
			response_message[len] = Main_settings.settings_length[i];
			len++;
			uint8_t data[Main_settings.settings_length[i]];
			if (get_setting_by_id(Main_settings.settings_id[i], data)) {
				memcpy(response_message + len, data,
				       Main_settings.settings_length[i]);
			}
			len += Main_settings.settings_length[i];

			prv_sent_settings_counter++;
		} else {
			return len;
		}
	}
	return len;
}

/*!
 * @brief Return status message.
 *
 * @param[in] uint8_t *response_message      Pointer to response Message.
 * @param[in] uint8_t max_len               Max message len.
 *
 * @retval  Error message, return length if ok.
 */
static int prv_cmd_send_status(uint8_t *response_message, uint8_t max_len)
{
	// Get status
	int len =
		status_get_message(response_message + MESSAGE_HEAD_LEN, max_len - MESSAGE_HEAD_LEN);
	// Move and add ID and length
	// memmove(response_message[2], response_message[0], len);
	response_message[0] = MSG_STATUS_ID;
	response_message[1] = len;

	return len + MESSAGE_HEAD_LEN;
}

#ifdef CONFIG_BT_CMDQ
static int prv_cmd_send_bt_cmdq_results(uint8_t *response_message, uint8_t max_len)
{
	/* Compose CMDQ message */
	int len = bt_cmdq_messaging_compose_latest_results_message(response_message, max_len);

	return len;
}
#endif /* CONFIG_BT_CMDQ */

/*!
 * @brief Send location values.
 *
 * @param[in] uint8_t *response_message      Pointer to response Message.
 * @param[in] uint8_t max_len               Max message len.
 *
 * @retval  Error message, return length if ok.
 */
static int prv_cmd_send_position(uint8_t *response_message, uint8_t max_len)
{
	// Check available length
	if (max_len < 12 + MESSAGE_HEAD_LEN) {
		return 0;
	}

	// Message header
	response_message[0] = Main_messages.msg_last_position->id;
	response_message[1] = Main_messages.msg_last_position->length;

	uint8_t tmp_data[4];
	uint8_t idx = MESSAGE_HEAD_LEN;
	// Longitude
	uint32_t_to_bytes(tmp_data, Main_values.gps_lon->def_val);
	memcpy(response_message + idx, tmp_data, 4);
	idx += 4;
	// Latitude
	uint32_t_to_bytes(tmp_data, Main_values.gps_lat->def_val);
	memcpy(response_message + idx, tmp_data, 4);
	idx += 4;
	// Altitude
	uint32_t_to_bytes(tmp_data, Main_values.gps_alt->def_val);
	memcpy(response_message + idx, tmp_data, 4);
	idx += 4;
	// Timestamp
	uint32_t_to_bytes(tmp_data, Main_values.last_position_time->def_val);
	memcpy(response_message + idx, tmp_data, 4);
	idx += 4;

	return idx;
}

/*!
 * @brief Send timestamp values.
 *
 * @param[in] uint8_t *response_message      Pointer to response Message.
 * @param[in] uint8_t max_len               Max message len.
 *
 * @retval  Error message, return length if ok.
 */
static int prv_cmd_send_timestamp(uint8_t *response_message, uint8_t max_len)
{
	// Check available length
	if (max_len < 4 + MESSAGE_HEAD_LEN) {
		return 0;
	}

	// Message header
	response_message[0] = Main_messages.msg_timestamp->id;
	response_message[1] = Main_messages.msg_timestamp->length;

	uint8_t tmp_data[4];
	uint8_t idx = MESSAGE_HEAD_LEN;

	// Timestamp
	uint32_t_to_bytes(tmp_data, get_global_unix_time());
	memcpy(response_message + idx, tmp_data, 4);
	idx += 4;

	return idx;
}

static void prv_cmd_set_position_and_time(uint8_t *data)
{
	// Parse longitude
	Main_settings.gps_init_lon->def_val = bytes_to_int32_t(data);
	// Parse latitude
	Main_settings.gps_init_lat->def_val = bytes_to_int32_t(data + 4);
	// Parse unix time
	Main_settings.init_time->def_val = bytes_to_uint32_t(data + 8);

	LOG_INF("New position LON: %d LAT: %d and TIME: %d", Main_settings.gps_init_lon->def_val,
		Main_settings.gps_init_lat->def_val, Main_settings.init_time->def_val);
}

/*!
 * @brief Execute command message - choose handler based on the id switch.
 *
 * @param[in] uint8_t port                  Port on which message was sent, to be replaced with
 * response port.
 * @param[in] mb_msg_dest *rsp_dest             Message origin, to be replaced with destination of
 * response message.
 * @param[in] mb_msg_action *rsp_action           Message action.
 * @param[in] uint8_t *rsp_message          Pointer to response Message.
 * @param[in] uint8_t id                    Command id.
 * @param[in] uint8_t len                   Message length.
 * @param[in] uint8_t *data                 Message.
 * @param[in] uint8_t msg_max_rsp_len       Max rsp length.
 *
 * @retval  Error message, return 0 if ok.
 */
static int prv_execute_command_message(uint8_t *port, mb_msg_dest *rsp_dest,
				       mb_msg_action *rsp_action, uint8_t *rsp_message, uint8_t id,
				       uint8_t len, uint8_t *data, uint8_t msg_max_rsp_len)
{
	uint8_t rsp_len = 0;

	switch (id) {
	// Send actions - set rsp action to send and compose rsp. If needed, change destination.
	case CMD_SEND_SINGLE_VAL: {
		LOG_INF("Send single value.");
		if (len == 1) {
			*port = PORT_VALUES; // Set port to value port
			*rsp_action = MB_MSG_SEND;
			rsp_len = prv_cmd_send_single_val(data[0], rsp_message, msg_max_rsp_len);
			if (rsp_len == 0) {
				rsp_message[0] = id;
				rsp_len = compose_response_msg(rsp_message, -EIO, port);
			}
			return rsp_len;
		}
		break;
	}
	case CMD_SEND_ALL_VAL: {
		LOG_INF("Send all values.");
		*port = PORT_VALUES; // Set port to value port
		*rsp_action = MB_MSG_SEND;
		rsp_len = prv_cmd_send_all_val(rsp_message, msg_max_rsp_len);
		if (rsp_len == 0) {
			rsp_message[0] = id;
			rsp_len = compose_response_msg(rsp_message, -EIO, port);
		}
		return rsp_len;
	}
	case CMD_SEND_STATUS: {
		LOG_INF("Send status");
		*port = Main_messages.msg_status->port; // Set port message status port
		*rsp_action = MB_MSG_SEND;
		return prv_cmd_send_status(rsp_message, msg_max_rsp_len);
	}

	case CMD_SEND_BT_CMDQ_RESULTS: {
#ifdef CONFIG_BT_CMDQ
		LOG_INF("Send BT CMDQ results.");
		*port = Main_messages.msg_ble_cmdq->port; // Set port message status port
		*rsp_action = MB_MSG_SEND;
		return prv_cmd_send_bt_cmdq_results(rsp_message, msg_max_rsp_len);
#else
		LOG_ERR("BT CMDQ not enabled!");
		return -ENOTSUP;
#endif /* CONFIG_BT_CMDQ */
	}

	case CMD_SEND_STATUS_LR: {
		LOG_INF("Send status via LR");
		*port = Main_messages.msg_status->port; // Set port message status port
		*rsp_dest = MB_MSG_LORA;
		*rsp_action = MB_MSG_SEND;
		return prv_cmd_send_status(rsp_message, msg_max_rsp_len);
	}
	case CMD_SEND_POSITION: {
		LOG_INF("Send location");
		*port = Main_messages.msg_last_position->port; // Set port to value port
		*rsp_action = MB_MSG_SEND;
		rsp_len = prv_cmd_send_position(rsp_message, msg_max_rsp_len);
		if (rsp_len == 0) {
			rsp_message[0] = id;
			rsp_len = compose_response_msg(rsp_message, -EIO, port);
		}
		return rsp_len;
	}
	case CMD_SEND_TIMESTAMP: {
		LOG_INF("Send timestamp");
		*port = Main_messages.msg_timestamp->port; // Set port to value port
		*rsp_action = MB_MSG_SEND;
		rsp_len = prv_cmd_send_timestamp(rsp_message, msg_max_rsp_len);
		if (rsp_len == 0) {
			rsp_message[0] = id;
			rsp_len = compose_response_msg(rsp_message, -EIO, port);
		}
		return rsp_len;
	}
	case CMD_SEND_SINGLE_SETTING: {
		LOG_INF("Send single setting.");
		*port = PORT_SETTINGS; // Set port to settings port
		*rsp_action = MB_MSG_SEND;
		rsp_len = prv_cmd_send_single_setting(data[0], rsp_message, msg_max_rsp_len);
		if (rsp_len == 0) {
			rsp_message[0] = id;
			rsp_len = compose_response_msg(rsp_message, -EIO, port);
		}
		return rsp_len;
	}
	case CMD_SEND_ALL_SETTINGS: {
		/* We fill the response message with settings until response message is full. If
		 * there are settings remaining that were not already sent, we send another
		 * `CMD_SEND_ALL_SETTINGS` command to ourself triggering another send of the
		 * remaining settings. After all settings have been sent, we send a confirmation
		 * message. */

		LOG_INF("Send all settings.");
		*port = PORT_SETTINGS; // Set port to settings port
		*rsp_action = MB_MSG_SEND;

		if (prv_sent_settings_counter < Main_settings.n_settings - 1) {
			rsp_len = prv_cmd_send_all_settings(rsp_message, msg_max_rsp_len);
			if (rsp_len == 0) { /* Error handling */
				rsp_message[0] = id;
				rsp_len = compose_response_msg(rsp_message, -EIO, port);
				return -EIO;
			}

			/* Send remaining settings */
			uint8_t cmd[2] = {0xa7, 0x00};
			thread_put_message(MB_MSG_BT, MB_MSG_DEV, MB_MSG_EXECUTE, 32, cmd,
					   sizeof(cmd), msg_max_rsp_len);

			LOG_INF("Sent settings: %d/%d", prv_sent_settings_counter,
				Main_settings.n_settings - 1);
		} else {
			/* Send confirmation */
			int err = 0;
			rsp_message[0] = 0xa7;
			len = compose_response_msg(rsp_message, err, port);
			prv_sent_settings_counter = 0;
			LOG_WRN("All settings sent. Sending confirmation.");
			return thread_put_message(MB_MSG_DEV, MB_MSG_BT, MB_MSG_SEND, 31,
						  rsp_message, len, 0);
		}

		return rsp_len;
	}
	// General commands
	case CMD_RESET: {
		/* code */
		LOG_INF("Reboot device.");
		k_sleep(K_MSEC(100));
		sys_reboot(0);
		break;
	}
	case CMD_CLEAR_NVS: {
		LOG_INF("Clear NVS storage. Device will reboot.");
		nvs_storage_clear();
		k_sleep(K_MSEC(100));
		sys_reboot(0);
		break;
	}
	case CMD_RESET_TO_DEF_SETTINGS: {
		LOG_INF("Revert to def settings and reboot the device.");
		prv_clear_all_from_nvs();
		k_sleep(K_MSEC(100));
		sys_reboot(0);
		break;
	}
	case CMD_SET_OPERATION_MODE_MAIN_TH: {
		if (data[0] == THREAD_DISABLED) {
			LOG_INF("Disable main thread functionality.");
			// sensors_disable();
			// LOG_INF("Sensors disabled!");
			set_thread_operation(MAIN_THREAD, THREAD_DISABLED);
		} else if (data[0] == THREAD_LOW_POWER) {
			LOG_INF("Set main thread to low power operation!");
			set_thread_operation(MAIN_THREAD, THREAD_LOW_POWER);
		} else if (data[0] == THREAD_NORMAL) {
			LOG_INF("Set main thread to normal operation!");
			set_thread_operation(MAIN_THREAD, THREAD_LOW_POWER);
		}
		break;
	}
	case CMD_SET_HIBERNATION_MODE: {
		hibernation_mode();
		break;
	}
	case CMD_RESET_INITIAL_POSITION: {
		prv_reset_reference_position();
		rsp_message[0] = id;
		rsp_len = compose_response_msg(rsp_message, 0, port);
		return rsp_len;
	}
	case CMD_RESET_INITIAL_TIME: {
		reset_time_from_settings();
		rsp_message[0] = id;
		rsp_len = compose_response_msg(rsp_message, 0, port);
		return rsp_len;
	}
	case CMD_SET_LOCATION_AND_TIME: {
		LOG_INF("Reset position and time.");
		rsp_message[0] = id;
		// Check length
		if (len == 12) {
			// Set position and time reference values
			prv_cmd_set_position_and_time(data);
			// reset stored position
			prv_reset_reference_position();
			// reset time
			reset_time_from_settings();
			rsp_len = compose_response_msg(rsp_message, 0, port);
		} else {
			LOG_ERR("Invalid message length!");
			rsp_len = compose_response_msg(rsp_message, -EIO, port);
		}

		return rsp_len;
	}
	// BT command - forward cmd to BT module
	case CMD_GET_BLE_SCAN:
	case CMD_GET_MAC:
	case CMD_DISABLE_BT_TH:
	case CMD_CHECK_PIN: {
		LOG_INF("Perform action in BT module.");
		*rsp_dest = MB_MSG_BT;
		*rsp_action = MB_MSG_EXECUTE;
		// Copy command data to response
		rsp_message[0] = id;
		rsp_message[1] = len;
		rsp_len = 2;
		return rsp_len;
	}
	// LR commands - forward cmd to LR module
	case CMD_JOIN:
	case CMD_SEND_LR_FIX:
	case CMD_RESET_GPS:
	case CMD_GET_WIFI_SCAN:
	case CMD_S_BAND_SEND:
	case CMD_GET_LR_SATELLITE_DATA:
	case CMD_GET_UBLOX_SATELLITE_DATA:
	case CMD_ALMANAC_UPDATE:
	case CMD_GET_UBLOX_FIX:
	case CMD_RESET_LR:
	case CMD_SET_OPERATION_MODE_COM_TH:
	case CMD_READ_ALL_LR_MESSAGES: {
		LOG_INF("LR module execute command.");
		*rsp_dest = MB_MSG_LORA;
		*rsp_action = MB_MSG_EXECUTE;
		// Copy command data to response
		rsp_message[0] = id;
		rsp_message[1] = len;
		for (uint8_t i = 0; i < len; i++) {
			rsp_message[i + 2] = data[i];
		}
		rsp_len = len + 2;
		return rsp_len;
	}
	case CMD_SEND_LR_MESSAGE: {
		LOG_INF("LR module send user message.");
		*rsp_dest = MB_MSG_LORA;
		*rsp_action = MB_MSG_STORE;
		// Copy command data to response
		rsp_message[0] = id;
		rsp_message[1] = len;
		for (uint8_t i = 0; i < len; i++) {
			rsp_message[i + 2] = data[i];
		}
		rsp_len = len + 2;
		return rsp_len;
	}
	// Flash
	case CMD_FLASH_CLEAR:
	case CMD_FLASH_GET_ALL:
	case CMD_FLASH_GET_FROM_HEAD:
	case CMD_DISABLE_FLASH_TH:
	case CMD_GET_FLASH_STATUS: {
		LOG_INF("Flash module execute command.");
		*rsp_dest = MB_MSG_FLASH;
		*rsp_action = MB_MSG_EXECUTE;
		// Copy command data to response
		rsp_message[0] = id;
		rsp_message[1] = len;
		for (uint8_t i = 0; i < len; i++) {
			rsp_message[i + 2] = data[i];
		}
		rsp_len = len + 2;
		return rsp_len;
	}

	/* External modules */
	/* Decouple collar */
	case CMD_DECOUPLE_COLLAR: {
#ifdef CONFIG_BOARD_COLLAREDGE_NRF52840
		prv_decouple_collar();
#else
		LOG_ERR("Decouple collar command fail: Unsupported board!");
#endif
		break;
	}
	// Satellite module
	case CMD_SEND_SAT_BUFFER: {
#ifdef CONFIG_SATELLITE
		LOG_INF("Satellite thread execute command.");
		*rsp_dest = MB_MSG_SAT;
		*rsp_action = MB_MSG_EXECUTE;
		// Copy command data to response
		rsp_message[0] = id;
		rsp_message[1] = len;
		for (uint8_t i = 0; i < len; i++) {
			rsp_message[i + 2] = data[i];
		}
		rsp_len = len + 2;
#else
		LOG_ERR("Satellite not supported for give HW!");
		rsp_message[0] = id;
		rsp_len = compose_response_msg(rsp_message, -EIO, port);
#endif
		return rsp_len;
	}
	case CMD_FENCE_MEASURE: {
#ifdef CONFIG_FENCE_PORT
		LOG_INF("Fence measure execute command.");
		*rsp_dest = MB_MSG_LORA;
		*rsp_action = MB_MSG_EXECUTE;
		// Copy command data to response
		rsp_message[0] = id;
		rsp_message[1] = len;
		for (uint8_t i = 0; i < len; i++) {
			rsp_message[i + 2] = data[i];
		}
		rsp_len = len + 2;
#else
		LOG_ERR("Fence not supported for give HW!");
		rsp_message[0] = id;
		rsp_len = compose_response_msg(rsp_message, -EIO, port);
#endif // CONFIG_FENCE_PORT
		return rsp_len;
	}
	case CMD_LP0_COMMAND: {
		LOG_INF("Pass command to LP0");
		lp0_send_command(data[0]);
		return 0;
	}
	default: {
		// Invalid id
		LOG_ERR("Invalid command id: %d!", id);
		return -EIO;
		break;
	}
	}
	return 0;
}

/*!
 * @brief Execute command or setting message - choose handler based on the port and id switch.
 *
 * @param[in] uint8_t port                  Port on which message was sent, to be replaced with
 * response port.
 * @param[in] mb_msg_dest *rsp_dest             Message origin, to be replaced with destination of
 * response message if needed.
 * @param[in] mb_msg_action *response_action      Message action.
 * @param[in] uint8_t *rsp_message          Pointer to response Message.
 * @param[in] uint8_t id                    Command id.
 * @param[in] uint8_t len                   Message length.
 * @param[in] uint8_t *data                 Message.
 * @param[in] uint8_t msg_max_rsp_len       Max rsp length.
 *
 * @retval  Error message, or return response length if ok.
 */
static int prv_execute_message(uint8_t *port, mb_msg_dest *rsp_dest, mb_msg_action *rsp_action,
			       uint8_t *rsp_message, uint8_t id, uint8_t len, uint8_t *data,
			       uint8_t msg_max_rsp_len)
{
	// Setting message
	if (*port == PORT_SETTINGS) {
		// Check if setting id is in the structure
		if (check_setting_id(id)) {
			// NVS
			LOG_INF("Store new setting for id: %d", id);
			// Check length
			uint8_t len_expected = get_setting_len(id);
			if (len > len_expected) {
				LOG_ERR("Received message with invalid length: %d, should be: %d!",
					len, len_expected);
				return 0;
			}
			// Validate new data
			if (!validate_setting(id, data)) {
				LOG_ERR("Received setting with invalid data values");
				return 0;
			}
			// Setting update
			set_setting_value_by_id(id, data, len);
			// NVS update
			nvs_storage_write((uint16_t)id, data, len_expected);

			// For some settings, perform action
			return prv_execute_new_setting(rsp_dest, rsp_action, rsp_message, id);
		} else {
			LOG_ERR("Received message with invalid setting id: %x!", id);
		}
		return 0;
	}
	// Value message
	else if (*port == PORT_VALUES) {
		// return parse_value_message(message, message_length);
		return 0;
	}
	// Command message
	else if (*port == PORT_COMMANDS) {
		LOG_INF("Execute command message.");
		return prv_execute_command_message(port, rsp_dest, rsp_action, rsp_message, id, len,
						   data, msg_max_rsp_len);
	}

	return -EINVAL;
}

int init_settings(void)
{
	// Init nvs storage
	int err = nvs_storage_init();
	if (err) {
		return err;
	}
	LOG_INF("NVS initialized");

	/* Init all settings from internal storage */
	err = prv_update_all_from_nvs();

	/* Update reference position */
	prv_init_reference_position();

	return err;
}

int parse_settings_message(uint8_t *message, uint8_t msg_length, mb_msg_dest msg_origin,
			   uint8_t msg_port, uint8_t msg_max_rsp_len)
{
	int err = 0;

	// Counters
	uint8_t i = 0;        // Go over byte array
	int16_t len = -1;     // Single message length
	bool read_id = false; // New message ID was read or not
	uint8_t id = 0;

	// Response
	uint8_t rsp_len = 0;
	mb_msg_dest rsp_dest = msg_origin;      // Init response destination to its origin
	uint8_t rsp_port = msg_port;            // Init response port to received port
	mb_msg_action rsp_action = MB_MSG_SEND; // Init response action to send
	uint8_t rsp_message[MAX_BUF_SIZE];

	while (i < msg_length) {
		// If all bytes from previous setting are read, read new ID
		if (!read_id) {
			// Check if ID is in the setting structure
			id = message[i];
			read_id = true;
			i++;
		}
		// If all bytes from previous setting are read, read new length
		else if (len < 0) {
			len = message[i];
			// Parse message of 0 length
			if (len == 0) {
				// Copy message port, origin
				rsp_dest = msg_origin;
				rsp_port = msg_port;
				LOG_INF("Execute message!");
				rsp_len = prv_execute_message(&rsp_port, &rsp_dest, &rsp_action,
							      rsp_message, id, len, NULL,
							      msg_max_rsp_len);
				// Send rsp to respective thread
				if (rsp_len > 0) {
					err = thread_put_message(msg_origin, rsp_dest, rsp_action,
								 rsp_port, rsp_message, rsp_len,
								 msg_max_rsp_len);
				}
				// Reset length and id
				len = -1;
				read_id = false;
			}
			i++;
		}
		// Otherwise compose message
		else {
			// Check if msg length is valid
			if (i + len <= msg_length) {
				// Get message
				uint8_t new_array[len];
				for (uint8_t j = 0; j < len; j++) {
					new_array[j] = message[i];
					i++;
				}
				// Copy message port, origin
				rsp_dest = msg_origin;
				rsp_port = msg_port;
				LOG_INF("Execute message!");
				rsp_len = prv_execute_message(&rsp_port, &rsp_dest, &rsp_action,
							      rsp_message, id, len, new_array,
							      msg_max_rsp_len);
				// Send to respective thread
				if (rsp_len > 0) {
					err = thread_put_message(msg_origin, rsp_dest, rsp_action,
								 rsp_port, rsp_message, rsp_len,
								 msg_max_rsp_len);
				}
				// Reset length and id
				len = -1;
				read_id = false;
			} else {
				LOG_INF("Invalid message length for id: %x!", id);
				i = msg_length;
				err = -EMSGSIZE;
			}
		}
	}
	return err;
}

/* Print out all settings for debug purposes */
void print_all_settings(void)
{
	LOG_INF("*   All settings values   *");
	LOG_INF("ID: %x LR send interval: %d s", Main_settings.lr_gps_interval->id,
		Main_settings.lr_gps_interval->def_val);
	LOG_INF("ID: %x UBLOX send interval: %d s", Main_settings.ublox_send_interval->id,
		Main_settings.ublox_send_interval->def_val);
	LOG_INF("ID: %x LR status send interval: %d s", Main_settings.status_send_interval->id,
		Main_settings.status_send_interval->def_val);
	LOG_INF("ID: %x gps_init_lon: %d", Main_settings.gps_init_lon->id,
		Main_settings.gps_init_lon->def_val);
	LOG_INF("ID: %x gps_init_lat: %d", Main_settings.gps_init_lat->id,
		Main_settings.gps_init_lat->def_val);
	LOG_INF("ID: %x unix init_time: %d", Main_settings.init_time->id,
		Main_settings.init_time->def_val);
	LOG_INF("ID: %x gps cold_fix_retry: %d", Main_settings.cold_fix_retry->id,
		Main_settings.cold_fix_retry->def_val);
	LOG_INF("ID: %x gps hot_fix_retry: %d", Main_settings.hot_fix_retry->id,
		Main_settings.hot_fix_retry->def_val);
	LOG_INF("ID: %x gps hot_fix_timeout: %d", Main_settings.hot_fix_timeout->id,
		Main_settings.hot_fix_timeout->def_val);
	LOG_INF("ID: %x ble_advertisement_interval: %d s",
		Main_settings.ble_advertisement_interval->id,
		Main_settings.ble_advertisement_interval->def_val);
	LOG_INF("ID: %x ble_scan_interval: %d s", Main_settings.ble_scan_interval->id,
		Main_settings.ble_scan_interval->def_val);
	LOG_INF("ID: %x ble_scan_aggregated_interval: %d s",
		Main_settings.ble_scan_aggregated_interval->id,
		Main_settings.ble_scan_aggregated_interval->def_val);
	LOG_INF("ID: %x wifi_scan_interval: %d s", Main_settings.wifi_scan_interval->id,
		Main_settings.wifi_scan_interval->def_val);
	LOG_INF("ID: %x wifi_scan_aggregated_interval: %d s",
		Main_settings.wifi_scan_aggregated_interval->id,
		Main_settings.wifi_scan_aggregated_interval->def_val);
	LOG_INF("LR send flags: %d Flash store flag: %d", Main_settings.lr_send_flag->def_val,
		Main_settings.flash_store_flag->def_val);
	for (uint8_t i = 0; i < 32; i++) {
		LOG_INF("Port: %d lr send: %d flash store: %d re-join: %d tx confirm: %d", i + 1,
			check_lr_send_flag(i + 1), check_flash_store_flag(i + 1),
			check_lr_join_flag(i + 1), check_lr_confirm_flag(i + 1));
	}
	LOG_INF("*   End all settings values   *\n");
}
