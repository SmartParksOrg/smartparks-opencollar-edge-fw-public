/** @file settings_transfer_helper.c
 *
 * @brief Settings transfer helper functions for transferring old settings to new settings
 * structure.
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2026 Irnas. All rights reserved.
 */

#include "nvs_storage.h"
#include "settings_def.h"
#include "zephyr/fs/nvs.h"
#include <settings_transfer_helper.h>

#include <stdint.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#define VALUES_FAMILY 0xA0

LOG_MODULE_REGISTER(settings_transfer_helper);

struct setting_transfer_item {
	/* Family is not needed for old settings, as they are all in the same family (0x00) */
	uint8_t old_id;
	uint8_t len;
	uint8_t new_id;
	uint8_t new_family;
};

/* Structure containing the mapping of old settings to new settings (from version v7.3.0 to
 * v7.4.0) */
static const struct setting_transfer_item legacy_map[] = {
	{.old_id = 0x00, .len = 1, .new_id = 0x00, .new_family = 0x02}, /* tracker_type */
	{.old_id = 0x01, .len = 4, .new_id = 0x00, .new_family = 0x05}, /* lr_gps_interval */
	{.old_id = 0x02, .len = 4, .new_id = 0x00, .new_family = 0x06}, /* ublox_send_interval */
	{.old_id = 0x03, .len = 4, .new_id = 0x01, .new_family = 0x02}, /* status_send_interval */
	{.old_id = 0x04,
	 .len = 4,
	 .new_id = 0x00,
	 .new_family = 0x07}, /* satellite_send_interval */
	{.old_id = 0x05, .len = 4, .new_id = 0x01, .new_family = 0x06},  /* gps_init_lon */
	{.old_id = 0x06, .len = 4, .new_id = 0x02, .new_family = 0x06},  /* gps_init_lat */
	{.old_id = 0x07, .len = 4, .new_id = 0x02, .new_family = 0x02},  /* init_time */
	{.old_id = 0x08, .len = 1, .new_id = 0x00, .new_family = 0x04},  /* ble_adv */
	{.old_id = 0x09, .len = 1, .new_id = 0x01, .new_family = 0x05},  /* gnss_assisted_scan */
	{.old_id = 0x0A, .len = 4, .new_id = 0x03, .new_family = 0x06},  /* gps_resend_interval */
	{.old_id = 0x0B, .len = 1, .new_id = 0x03, .new_family = 0x02},  /* data_log */
	{.old_id = 0x0C, .len = 4, .new_id = 0x00, .new_family = 0x03},  /* lr_send_flag */
	{.old_id = 0x0D, .len = 4, .new_id = 0x01, .new_family = 0x03},  /* flash_store_flag */
	{.old_id = 0x0E, .len = 1, .new_id = 0x02, .new_family = 0x05},  /* lr_adr */
	{.old_id = 0x0F, .len = 1, .new_id = 0x03, .new_family = 0x05},  /* lr_region */
	{.old_id = 0x10, .len = 16, .new_id = 0x04, .new_family = 0x05}, /* app_key */
	{.old_id = 0x11, .len = 8, .new_id = 0x05, .new_family = 0x05},  /* device_eui */
	{.old_id = 0x12, .len = 8, .new_id = 0x06, .new_family = 0x05},  /* app_eui */
	{.old_id = 0x13, .len = 4, .new_id = 0x04, .new_family = 0x06},  /* horizontal_accuracy */
	{.old_id = 0x14, .len = 1, .new_id = 0x05, .new_family = 0x06},  /* cold_fix_retry */
	{.old_id = 0x15, .len = 1, .new_id = 0x06, .new_family = 0x06},  /* hot_fix_retry */
	{.old_id = 0x16, .len = 2, .new_id = 0x07, .new_family = 0x06},  /* cold_fix_timeout */
	{.old_id = 0x17, .len = 2, .new_id = 0x08, .new_family = 0x06},  /* hot_fix_timeout */
	{.old_id = 0x18,
	 .len = 4,
	 .new_id = 0x01,
	 .new_family = 0x04}, /* ble_advertisement_interval */
	{.old_id = 0x19, .len = 4, .new_id = 0x04, .new_family = 0x02}, /* wifi_scan_interval */
	{.old_id = 0x1A,
	 .len = 4,
	 .new_id = 0x05,
	 .new_family = 0x02}, /* wifi_scan_aggregated_interval */
	{.old_id = 0x1B, .len = 4, .new_id = 0x02, .new_family = 0x04}, /* ble_scan_duration */
	{.old_id = 0x1C, .len = 4, .new_id = 0x03, .new_family = 0x04}, /* ble_scan_interval */
	{.old_id = 0x1D,
	 .len = 4,
	 .new_id = 0x04,
	 .new_family = 0x04}, /* ble_scan_aggregated_interval */
	{.old_id = 0x1E, .len = 1, .new_id = 0x05, .new_family = 0x04}, /* ble_scan_filter */
	{.old_id = 0x1F, .len = 4, .new_id = 0x06, .new_family = 0x04}, /* ble_auto_disconnect */
	{.old_id = 0x20, .len = 4, .new_id = 0x07, .new_family = 0x05}, /* s_band_send_interval */
	{.old_id = 0x21, .len = 8, .new_id = 0x06, .new_family = 0x02}, /* device_name */
	{.old_id = 0x22, .len = 4, .new_id = 0x08, .new_family = 0x05}, /* lr_join_flag */
	{.old_id = 0x23, .len = 4, .new_id = 0x09, .new_family = 0x05}, /* lr_confirm_flag */
	{.old_id = 0x24, .len = 2, .new_id = 0x0A, .new_family = 0x05}, /* lr_max_confirm_fail */
	{.old_id = 0x25, .len = 1, .new_id = 0x09, .new_family = 0x06}, /* gps_backoff_factor */
	{.old_id = 0x26, .len = 4, .new_id = 0x0A, .new_family = 0x06}, /* ublox_send_interval_2 */
	{.old_id = 0x27, .len = 1, .new_id = 0x0B, .new_family = 0x06}, /* ublox_interval1_start */
	{.old_id = 0x28, .len = 1, .new_id = 0x0C, .new_family = 0x06}, /* ublox_min_fix_time */
	{.old_id = 0x29,
	 .len = 1,
	 .new_id = 0x0D,
	 .new_family = 0x06}, /* ublox_multiple_intervals */
	{.old_id = 0x2A, .len = 4, .new_id = 0x07, .new_family = 0x02}, /* device_pin */
	{.old_id = 0x2B, .len = 1, .new_id = 0x0E, .new_family = 0x06}, /* ublox_active_tracking */
	{.old_id = 0x2C, .len = 1, .new_id = 0x08, .new_family = 0x02}, /* led_enabled */
	{.old_id = 0x2D, .len = 1, .new_id = 0x00, .new_family = 0x08}, /* motion_ths */
	{.old_id = 0x2E, .len = 1, .new_id = 0x17, .new_family = 0x06}, /* enable_motion_trig_gps */
	{.old_id = 0x2F, .len = 4, .new_id = 0x0F, .new_family = 0x06}, /* gps_triggered_interval */
	{.old_id = 0x30,
	 .len = 1,
	 .new_id = 0x10,
	 .new_family = 0x06}, /* gps_skipped_triggered_interval */
	{.old_id = 0x31,
	 .len = 4,
	 .new_id = 0x0B,
	 .new_family = 0x05}, /* lr_messaging_retry_interval */
	{.old_id = 0x32,
	 .len = 1,
	 .new_id = 0x0C,
	 .new_family = 0x05}, /* lr_messaging_retry_count */
	{.old_id = 0x33, .len = 1, .new_id = 0x11, .new_family = 0x06}, /* ublox_leave_on */
	{.old_id = 0x34, .len = 4, .new_id = 0x09, .new_family = 0x02}, /* memfault_send_interval */
	{.old_id = 0x35, .len = 4, .new_id = 0x0D, .new_family = 0x05}, /* rejoin_interval */
	{.old_id = 0x36, .len = 4, .new_id = 0x0A, .new_family = 0x02}, /* check_error_interval */
	{.old_id = 0x37,
	 .len = 1,
	 .new_id = 0x12,
	 .new_family = 0x06}, /* gnss_constellation_to_use */
	{.old_id = 0x38,
	 .len = 1,
	 .new_id = 0x13,
	 .new_family = 0x06}, /* ublox_min_satellites_timer */
	{.old_id = 0x39, .len = 4, .new_id = 0x02, .new_family = 0x03}, /* sat_send_flag */
	{.old_id = 0x3A, .len = 1, .new_id = 0x01, .new_family = 0x07}, /* satellite_enabled */
	{.old_id = 0x3B, .len = 1, .new_id = 0x02, .new_family = 0x07}, /* satellite_retry */
	{.old_id = 0x3F, .len = 1, .new_id = 0x01, .new_family = 0x08}, /* fence_enabled */
	{.old_id = 0x40, .len = 4, .new_id = 0x02, .new_family = 0x08}, /* fence_interval */
	{.old_id = 0x41, .len = 2, .new_id = 0x03, .new_family = 0x08}, /* fence_sampling_length */
	{.old_id = 0x42,
	 .len = 4,
	 .new_id = 0x04,
	 .new_family = 0x08}, /* fence_mv_scaling_factor */
	{.old_id = 0x43, .len = 4, .new_id = 0x0B, .new_family = 0x02},  /* flash_status_interval */
	{.old_id = 0x44, .len = 16, .new_id = 0x0E, .new_family = 0x05}, /* lp0_app_key */
	{.old_id = 0x45, .len = 16, .new_id = 0x0F, .new_family = 0x05}, /* lp0_network_key */
	{.old_id = 0x46, .len = 4, .new_id = 0x10, .new_family = 0x05},  /* lp0_dev_addr */
	{.old_id = 0x47,
	 .len = 1,
	 .new_id = 0x07,
	 .new_family = 0x04}, /* ble_scan_report_zero_connections_found */
	{.old_id = 0x48,
	 .len = 1,
	 .new_id = 0x0C,
	 .new_family = 0x02}, /* wifi_scan_report_zero_connections_found */
	{.old_id = 0x49, .len = 1, .new_id = 0x08, .new_family = 0x04}, /* cmdq_enabled */
	{.old_id = 0x4A, .len = 4, .new_id = 0x09, .new_family = 0x04}, /* cmdq_scan_duration */
	{.old_id = 0x4B, .len = 4, .new_id = 0x0A, .new_family = 0x04}, /* cmdq_search_interval */
	{.old_id = 0x4C,
	 .len = 4,
	 .new_id = 0x0B,
	 .new_family = 0x04}, /* cmdq_on_no_detection_wait_duration */
	{.old_id = 0x4D,
	 .len = 6,
	 .new_id = 0x0C,
	 .new_family = 0x04}, /* cmdq_searched_mac_address */
	{.old_id = 0x4E,
	 .len = 4,
	 .new_id = 0x0D,
	 .new_family = 0x04}, /* cmdq_reporting_interval */
	{.old_id = 0x4F, .len = 4, .new_id = 0x11, .new_family = 0x05}, /* s_band_rf_frequency_hz */
	{.old_id = 0x50,
	 .len = 1,
	 .new_id = 0x0E,
	 .new_family = 0x04}, /* cmdq_report_zero_messages_to_be_sent */
	{.old_id = 0x52, .len = 1, .new_id = 0x14, .new_family = 0x06}, /* ublox_interval2_start */
	{.old_id = 0x57, .len = 1, .new_id = 0x12, .new_family = 0x05}, /* lr_adr_profile */
	{.old_id = 0x5F, .len = 1, .new_id = 0x15, .new_family = 0x06}, /* ublox_min_satellites */
	{.old_id = 0x60,
	 .len = 1,
	 .new_id = 0x05,
	 .new_family = 0x08}, /* external_switch_detection_gpio_pin_power_enabled */
	{.old_id = 0x61, .len = 1, .new_id = 0x13, .new_family = 0x05}, /* vhf_enabled */
	{.old_id = 0x62, .len = 4, .new_id = 0x14, .new_family = 0x05}, /* vhf_interval1 */
	{.old_id = 0x63, .len = 4, .new_id = 0x15, .new_family = 0x05}, /* vhf_interval2 */
	{.old_id = 0x64, .len = 1, .new_id = 0x16, .new_family = 0x05}, /* vhf_interval1_start */
	{.old_id = 0x65, .len = 1, .new_id = 0x17, .new_family = 0x05}, /* vhf_interval2_start */
	{.old_id = 0x66, .len = 1, .new_id = 0x18, .new_family = 0x05}, /* vhf_multiple_intervals */
	{.old_id = 0x67,
	 .len = 1,
	 .new_id = 0x19,
	 .new_family = 0x05}, /* vhf_num_of_packets_per_burst */
	{.old_id = 0x68,
	 .len = 2,
	 .new_id = 0x1A,
	 .new_family = 0x05}, /* vhf_time_between_packets_ms */
	{.old_id = 0x69, .len = 1, .new_id = 0x1B, .new_family = 0x05}, /* vhf_external_path */
	{.old_id = 0x6A, .len = 4, .new_id = 0x1C, .new_family = 0x05}, /* vhf_tx_frequency_khz */
	{.old_id = 0x6B,
	 .len = 2,
	 .new_id = 0x1D,
	 .new_family = 0x05}, /* vhf_single_pulse_duration_ms */
	{.old_id = 0x6C, .len = 1, .new_id = 0x1E, .new_family = 0x05}, /* s_band_send_mode */
	{.old_id = 0x6D, .len = 1, .new_id = 0x06, .new_family = 0x08}, /* fence_led_blink */
	{.old_id = 0x6E,
	 .len = 1,
	 .new_id = 0x16,
	 .new_family = 0x06}, /* gps_motion_triggered_min_num_of_triggers_per_interval */
	{.old_id = 0x72,
	 .len = 2,
	 .new_id = 0x0F,
	 .new_family = 0x04}, /* ble_scan_manufacturer_id */
	{.old_id = 0x73,
	 .len = 1,
	 .new_id = 0x07,
	 .new_family = 0x08}, /* accel_movement_data_fifo_enabled */
	{.old_id = 0x74, .len = 2, .new_id = 0x08, .new_family = 0x08}, /* accel_odr_hz */
	{.old_id = 0x75, .len = 1, .new_id = 0x09, .new_family = 0x08}, /* accel_g_scale */
	{.old_id = 0x76,
	 .len = 4,
	 .new_id = 0x03,
	 .new_family = 0x07}, /* satellite_send_interval2 */
	{.old_id = 0x77,
	 .len = 1,
	 .new_id = 0x04,
	 .new_family = 0x07}, /* satellite_send_interval2_start */
	{.old_id = 0x78,
	 .len = 1,
	 .new_id = 0x05,
	 .new_family = 0x07}, /* satellite_multiple_intervals */
	{.old_id = 0x79,
	 .len = 1,
	 .new_id = 0x06,
	 .new_family = 0x07}, /* satellite_interval1_start */
	{.old_id = 0x7A,
	 .len = 1,
	 .new_id = 0x0A,
	 .new_family = 0x08}, /* outdoor_detection_enabled */
	{.old_id = 0x7B, .len = 1, .new_id = 0x0B, .new_family = 0x08}, /* outdoor_detection_tau */
	{.old_id = 0x7C,
	 .len = 12,
	 .new_id = 0x0C,
	 .new_family = 0x08}, /* outdoor_detection_parameters */
	{.old_id = 0x7D,
	 .len = 2,
	 .new_id = 0x18,
	 .new_family = 0x06}, /* ublox_cold_fix_hour_interval */
	{.old_id = 0x7E,
	 .len = 1,
	 .new_id = 0x0E,
	 .new_family = 0x08}, /* external_switch_detection_enabled */
	{.old_id = 0x7F,
	 .len = 1,
	 .new_id = 0x0F,
	 .new_family = 0x08}, /* external_switch_detection_trigger_type */
	{.old_id = 0x80,
	 .len = 2,
	 .new_id = 0x10,
	 .new_family = 0x08}, /* external_switch_detection_trigger_debounce_ms */
	{.old_id = 0x81,
	 .len = 4,
	 .new_id = 0x11,
	 .new_family = 0x08}, /* external_switch_detection_reporting_interval */
	{.old_id = 0x82,
	 .len = 1,
	 .new_id = 0x12,
	 .new_family = 0x08}, /* external_switch_send_inactivity_report */
	{.old_id = 0x83,
	 .len = 2,
	 .new_id = 0x13,
	 .new_family = 0x08}, /* external_switch_minimal_report_duration_ms */
	{.old_id = 0x84,
	 .len = 1,
	 .new_id = 0x14,
	 .new_family = 0x08}, /* external_switch_input_pull */
	{.old_id = 0x85,
	 .len = 1,
	 .new_id = 0x15,
	 .new_family = 0x08}, /* external_switch_counter_enabled */
	{.old_id = 0x86, .len = 1, .new_id = 0x16, .new_family = 0x08}, /* air_quality_enabled */
	{.old_id = 0x87, .len = 4, .new_id = 0x17, .new_family = 0x08}, /* air_quality_interval */
	{.old_id = 0x88, .len = 4, .new_id = 0x1F, .new_family = 0x05}, /* lp0_send_flag */
	{.old_id = 0x89, .len = 4, .new_id = 0x20, .new_family = 0x05}, /* lp0_tx_frequency_hz */
	{.old_id = 0x8A, .len = 4, .new_id = 0x21, .new_family = 0x05}, /* lp0_rx_frequency_hz */
	{.old_id = 0x8B,
	 .len = 4,
	 .new_id = 0x22,
	 .new_family = 0x05}, /* lp0_communication_params */
	{.old_id = 0x8C, .len = 5, .new_id = 0x23, .new_family = 0x05}, /* lp0_node_params */
	/* Runtime values were never persisted under their protocol IDs. Keep these mappings for
	 * historical/debugging purposes, but do not process them during settings migration. */
	{.old_id = 0xD0, .len = 4, .new_id = 0x00, .new_family = 0xA0}, /* reset_reason */
	{.old_id = 0xD1, .len = 4, .new_id = 0x01, .new_family = 0xA0}, /* gps_lon */
	{.old_id = 0xD2, .len = 4, .new_id = 0x02, .new_family = 0xA0}, /* gps_lat */
	{.old_id = 0xD3, .len = 4, .new_id = 0x03, .new_family = 0xA0}, /* gps_alt */
	{.old_id = 0xD4, .len = 4, .new_id = 0x04, .new_family = 0xA0}, /* lis2_acc_x */
	{.old_id = 0xD5, .len = 4, .new_id = 0x05, .new_family = 0xA0}, /* lis2_acc_y */
	{.old_id = 0xD6, .len = 4, .new_id = 0x06, .new_family = 0xA0}, /* lis2_acc_z */
	{.old_id = 0xD7, .len = 4, .new_id = 0x07, .new_family = 0xA0}, /* batt_mV */
	{.old_id = 0xD8, .len = 4, .new_id = 0x08, .new_family = 0xA0}, /* ublox_time */
	{.old_id = 0xD9, .len = 1, .new_id = 0x09, .new_family = 0xA0}, /* lr_satellites */
	{.old_id = 0xDA, .len = 4, .new_id = 0x0A, .new_family = 0xA0}, /* mcu_temp */
	{.old_id = 0xDB, .len = 4, .new_id = 0x0B, .new_family = 0xA0}, /* charge_mV */
	{.old_id = 0xDC, .len = 2, .new_id = 0x0C, .new_family = 0xA0}, /* gps_h_acc_est */
	{.old_id = 0xE8, .len = 4, .new_id = 0x0E, .new_family = 0xA0}, /* flash_nr_msg */
	{.old_id = 0xE9, .len = 4, .new_id = 0x0F, .new_family = 0xA0}, /* last_position_time */
	{.old_id = 0xEA, .len = 4, .new_id = 0x10, .new_family = 0xA0}, /* last_accel_int_time */
	{.old_id = 0xEB, .len = 1, .new_id = 0x11, .new_family = 0xA0}, /* n_mes */
	{.old_id = 0xEC, .len = 2, .new_id = 0x12, .new_family = 0xA0}, /* almanac_age */
	{.old_id = 0xED, .len = 8, .new_id = 0x13, .new_family = 0xA0}, /* factory_device_name */
	{.old_id = 0xEF, .len = 1, .new_id = 0x14, .new_family = 0xA0}, /* satellite_resend_try */
};

int settings_transfer(void)
{
	struct nvs_fs *nvs_fs = nvs_get_fs();
	int transfer_err = 0;
	size_t found = 0;
	size_t transferred = 0;
	size_t defaulted = 0;
	size_t discarded = 0;
	size_t failed = 0;

	/* Family 0x00 is permanently reserved for legacy settings. Remaining entries need to be
	 * transferred, reset to defaults, or discarded if their stored length is incompatible. */
	for (size_t i = 0; i < ARRAY_SIZE(legacy_map); i++) {
		const struct setting_transfer_item item = legacy_map[i];

		if (item.new_family == VALUES_FAMILY) {
			continue;
		}

		uint8_t value[item.len];
		uint8_t verify_value[item.len];
		uint16_t old_key = MAKE_SETTING_KEY(SETTINGS_FAMILY_LEGACY, item.old_id);
		uint16_t new_key = MAKE_SETTING_KEY(item.new_family, item.new_id);
		int err = nvs_read(nvs_fs, old_key, value, item.len);

		/* These IDs previously meant rf_scan_enabled and satellite_min_signal_strength.
		 * Their bytes cannot identify which setting was saved, even with valid ranges. */
		bool reused_id = item.old_id == 0x38 || item.old_id == 0x60;

		if (err == item.len || (err >= 0 && reused_id)) {
			found++;
			LOG_DBG("Found legacy setting 0x%04x", old_key);

			bool use_default =
				reused_id || !setting_value_in_range(item.new_family, item.new_id,
								     value, item.len);
			if (use_default) {
				/* Defaults must come from the compiled definitions, not mutable RAM
				 * values or an existing destination left by an earlier migration.
				 */
				err = get_setting_default_by_id(item.new_family, item.new_id, value,
								item.len);
				if (err != item.len) {
					LOG_ERR("Failed to get default for legacy setting 0x%04x "
						"(length: %d, expected: %d)",
						old_key, err, item.len);
					failed++;
					if (transfer_err == 0) {
						transfer_err = -EINVAL;
					}
					continue;
				}
				LOG_WRN("Resetting legacy setting 0x%04x to default: %s", old_key,
					reused_id ? "reused ID" : "value outside limits");
			}

			/* Persist and verify the new entry before deleting the only legacy copy. */
			err = nvs_write(nvs_fs, new_key, value, item.len);
			if (err != item.len && err != 0) {
				LOG_ERR("Failed to write legacy setting 0x%04x to 0x%04x (err: %d)",
					old_key, new_key, err);
				failed++;
				if (transfer_err == 0) {
					transfer_err = err < 0 ? err : -EIO;
				}
				continue;
			}

			err = nvs_read(nvs_fs, new_key, verify_value, item.len);
			if (err != item.len || memcmp(value, verify_value, item.len) != 0) {
				LOG_ERR("Failed to verify transferred setting 0x%04x at 0x%04x "
					"(err: %d)",
					old_key, new_key, err);
				failed++;
				if (transfer_err == 0) {
					transfer_err = err < 0 ? err : -EIO;
				}
				continue;
			}

			err = nvs_delete(nvs_fs, old_key);
			if (err != 0) {
				LOG_ERR("Failed to delete transferred legacy setting 0x%04x (err: "
					"%d)",
					old_key, err);
				failed++;
				if (transfer_err == 0) {
					transfer_err = err;
				}
				continue;
			}

			if (use_default) {
				defaulted++;
			} else {
				transferred++;
			}
			LOG_DBG("Migrated legacy setting 0x%04x to 0x%04x", old_key, new_key);
		} else if (err == -ENOENT) {
			LOG_DBG("No legacy entry found for setting 0x%04x", old_key);
		} else if (err >= 0) {
			found++;
			LOG_WRN("Discarding legacy setting 0x%04x with length %d (expected %d)",
				old_key, err, item.len);

			/* Remove incompatible entries so they are not retried on every boot. */
			err = nvs_delete(nvs_fs, old_key);
			if (err != 0) {
				LOG_ERR("Failed to delete incompatible legacy setting 0x%04x "
					"(err: %d)",
					old_key, err);
				failed++;
				if (transfer_err == 0) {
					transfer_err = err;
				}
				continue;
			}

			discarded++;
		} else {
			LOG_ERR("Failed to read legacy setting 0x%04x (err: %d)", old_key, err);
			failed++;
			if (transfer_err == 0) {
				transfer_err = err;
			}
		}
	}

	LOG_DBG("Legacy settings scan: found=%u transferred=%u defaulted=%u discarded=%u "
		"failed=%u",
		(unsigned int)found, (unsigned int)transferred, (unsigned int)defaulted,
		(unsigned int)discarded, (unsigned int)failed);

	if (transfer_err != 0) {
		return transfer_err;
	}

	return found == 0 ? -EALREADY : 0;
}
