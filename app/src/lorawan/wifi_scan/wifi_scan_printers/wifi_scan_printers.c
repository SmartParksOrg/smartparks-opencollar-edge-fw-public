/**
 * @file wifi_scan_printers.c
 * @brief
 *
 * @copyright (c) 2023 Irnas. All rights reserved.
 *
 */

#include "wifi_scan_printers.h"

#include "lr11xx_board.h"
#include "lr11xx_system.h"
#include "lr11xx_wifi.h"

#include <zephyr/kernel.h>
#include <zephyr/types.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(wifi_scan_printers);

/**
 * @brief Print mac address
 *
 * @param[in] prefix
 * @param[in] mac
 */
static void prv_print_mac_address(const char *prefix, lr11xx_wifi_mac_address_t mac)
{
	LOG_INF("%s%02x:%02x:%02x:%02x:%02x:%02x", prefix, mac[0], mac[1], mac[2], mac[3], mac[4],
		mac[5]);
}

/**
 * @brief convert lr11xx_wifi_signal_type_result_t to string
 *
 * @param[in] value
 * @return const char*
 */
static const char *prv_wifi_signal_type_result_to_str(const lr11xx_wifi_signal_type_result_t value)
{
	switch (value) {
	case LR11XX_WIFI_TYPE_RESULT_B: {
		return (const char *)"LR11XX_WIFI_TYPE_RESULT_B";
	}

	case LR11XX_WIFI_TYPE_RESULT_G: {
		return (const char *)"LR11XX_WIFI_TYPE_RESULT_G";
	}

	case LR11XX_WIFI_TYPE_RESULT_N: {
		return (const char *)"LR11XX_WIFI_TYPE_RESULT_N";
	}

	default: {
		return (const char *)"Unknown";
	}
	}
}

/**
 * @brief Convert lr11xx_wifi_frame_type_t to string
 *
 * @param[in] value
 * @return const char*
 */
static const char *prv_wifi_frame_type_to_str(const lr11xx_wifi_frame_type_t value)
{
	switch (value) {
	case LR11XX_WIFI_FRAME_TYPE_MANAGEMENT: {
		return (const char *)"LR11XX_WIFI_FRAME_TYPE_MANAGEMENT";
	}

	case LR11XX_WIFI_FRAME_TYPE_CONTROL: {
		return (const char *)"LR11XX_WIFI_FRAME_TYPE_CONTROL";
	}

	case LR11XX_WIFI_FRAME_TYPE_DATA: {
		return (const char *)"LR11XX_WIFI_FRAME_TYPE_DATA";
	}

	default: {
		return (const char *)"Unknown";
	}
	}
}

/**
 * @brief Convert lr11xx_wifi_datarate_t to string
 *
 * @param[in] value
 * @return const char*
 */
static const char *prv_wifi_datarate_to_str(const lr11xx_wifi_datarate_t value)
{
	switch (value) {
	case LR11XX_WIFI_DATARATE_1_MBPS: {
		return (const char *)"LR11XX_WIFI_DATARATE_1_MBPS";
	}

	case LR11XX_WIFI_DATARATE_2_MBPS: {
		return (const char *)"LR11XX_WIFI_DATARATE_2_MBPS";
	}

	case LR11XX_WIFI_DATARATE_6_MBPS: {
		return (const char *)"LR11XX_WIFI_DATARATE_6_MBPS";
	}

	case LR11XX_WIFI_DATARATE_9_MBPS: {
		return (const char *)"LR11XX_WIFI_DATARATE_9_MBPS";
	}

	case LR11XX_WIFI_DATARATE_12_MBPS: {
		return (const char *)"LR11XX_WIFI_DATARATE_12_MBPS";
	}

	case LR11XX_WIFI_DATARATE_18_MBPS: {
		return (const char *)"LR11XX_WIFI_DATARATE_18_MBPS";
	}

	case LR11XX_WIFI_DATARATE_24_MBPS: {
		return (const char *)"LR11XX_WIFI_DATARATE_24_MBPS";
	}

	case LR11XX_WIFI_DATARATE_36_MBPS: {
		return (const char *)"LR11XX_WIFI_DATARATE_36_MBPS";
	}

	case LR11XX_WIFI_DATARATE_48_MBPS: {
		return (const char *)"LR11XX_WIFI_DATARATE_48_MBPS";
	}

	case LR11XX_WIFI_DATARATE_54_MBPS: {
		return (const char *)"LR11XX_WIFI_DATARATE_54_MBPS";
	}

	case LR11XX_WIFI_DATARATE_6_5_MBPS: {
		return (const char *)"LR11XX_WIFI_DATARATE_6_5_MBPS";
	}

	case LR11XX_WIFI_DATARATE_13_MBPS: {
		return (const char *)"LR11XX_WIFI_DATARATE_13_MBPS";
	}

	case LR11XX_WIFI_DATARATE_19_5_MBPS: {
		return (const char *)"LR11XX_WIFI_DATARATE_19_5_MBPS";
	}

	case LR11XX_WIFI_DATARATE_26_MBPS: {
		return (const char *)"LR11XX_WIFI_DATARATE_26_MBPS";
	}

	case LR11XX_WIFI_DATARATE_39_MBPS: {
		return (const char *)"LR11XX_WIFI_DATARATE_39_MBPS";
	}

	case LR11XX_WIFI_DATARATE_52_MBPS: {
		return (const char *)"LR11XX_WIFI_DATARATE_52_MBPS";
	}

	case LR11XX_WIFI_DATARATE_58_MBPS: {
		return (const char *)"LR11XX_WIFI_DATARATE_58_MBPS";
	}

	case LR11XX_WIFI_DATARATE_65_MBPS: {
		return (const char *)"LR11XX_WIFI_DATARATE_65_MBPS";
	}

	case LR11XX_WIFI_DATARATE_7_2_MBPS: {
		return (const char *)"LR11XX_WIFI_DATARATE_7_2_MBPS";
	}

	case LR11XX_WIFI_DATARATE_14_4_MBPS: {
		return (const char *)"LR11XX_WIFI_DATARATE_14_4_MBPS";
	}

	case LR11XX_WIFI_DATARATE_21_7_MBPS: {
		return (const char *)"LR11XX_WIFI_DATARATE_21_7_MBPS";
	}

	case LR11XX_WIFI_DATARATE_28_9_MBPS: {
		return (const char *)"LR11XX_WIFI_DATARATE_28_9_MBPS";
	}

	case LR11XX_WIFI_DATARATE_43_3_MBPS: {
		return (const char *)"LR11XX_WIFI_DATARATE_43_3_MBPS";
	}

	case LR11XX_WIFI_DATARATE_57_8_MBPS: {
		return (const char *)"LR11XX_WIFI_DATARATE_57_8_MBPS";
	}

	case LR11XX_WIFI_DATARATE_65_2_MBPS: {
		return (const char *)"LR11XX_WIFI_DATARATE_65_2_MBPS";
	}

	case LR11XX_WIFI_DATARATE_72_2_MBPS: {
		return (const char *)"LR11XX_WIFI_DATARATE_72_2_MBPS";
	}

	default: {
		return (const char *)"Unknown";
	}
	}
}

void wifi_scan_print_basic_basic_mac_type_channel_results(
	lr11xx_wifi_basic_mac_type_channel_result_t *results, uint8_t nb_results)
{

	for (uint8_t idx = 0; idx < nb_results; idx++) {
		lr11xx_wifi_mac_origin_t mac_origin = LR11XX_WIFI_ORIGIN_BEACON_FIX_AP;
		lr11xx_wifi_channel_t channel = LR11XX_WIFI_NO_CHANNEL;
		bool rssi_validity = false;
		lr11xx_wifi_parse_channel_info(results[idx].channel_info_byte, &channel,
					       &rssi_validity, &mac_origin);

		LOG_INF("Result %u/%u", idx + 1, nb_results);
		prv_print_mac_address("  -> MAC address: ", results[idx].mac_address);
		LOG_INF("  -> Channel: %d", channel);
		LOG_INF("  -> MAC origin: %s",
			(rssi_validity ? "From gateway" : "From end device"));
		LOG_INF("  -> RSSI: %d", results[idx].rssi);
		LOG_INF("  -> Signal type: %s\n",
			prv_wifi_signal_type_result_to_str(
				lr11xx_wifi_extract_signal_type_from_data_rate_info(
					results[idx].data_rate_info_byte)));
	}
}

void wifi_scan_print_basic_complete_results(lr11xx_wifi_basic_complete_result_t *results,
					    uint8_t nb_results)
{
	for (uint8_t idx = 0; idx < nb_results; idx++) {
		lr11xx_wifi_mac_origin_t mac_origin = LR11XX_WIFI_ORIGIN_BEACON_FIX_AP;
		lr11xx_wifi_channel_t channel = LR11XX_WIFI_NO_CHANNEL;
		bool rssi_validity = false;
		lr11xx_wifi_parse_channel_info(results[idx].channel_info_byte, &channel,
					       &rssi_validity, &mac_origin);

		lr11xx_wifi_frame_type_t frame_type = LR11XX_WIFI_FRAME_TYPE_MANAGEMENT;
		lr11xx_wifi_frame_sub_type_t frame_sub_type = 0;
		bool to_ds = false;
		bool from_ds = false;
		lr11xx_wifi_parse_frame_type_info(results[idx].frame_type_info_byte, &frame_type,
						  &frame_sub_type, &to_ds, &from_ds);

		LOG_INF("Result %u/%u", idx + 1, nb_results);
		prv_print_mac_address("  -> MAC address: ", results[idx].mac_address);
		LOG_INF("  -> Channel: %d", channel);
		LOG_INF("  -> MAC origin: %s",
			(rssi_validity ? "From gateway" : "From end device"));
		LOG_INF("  -> RSSI: %d", results[idx].rssi);
		LOG_INF("  -> Signal type: %s\n",
			prv_wifi_signal_type_result_to_str(
				lr11xx_wifi_extract_signal_type_from_data_rate_info(
					results[idx].data_rate_info_byte)));
		LOG_INF("  -> Frame type: %s", prv_wifi_frame_type_to_str(frame_type));
		LOG_INF("  -> Frame sub-type: 0x%02X", frame_sub_type);
		LOG_INF("  -> FromDS/ToDS: %s / %s", ((from_ds == true) ? "true" : "false"),
			((to_ds == true) ? "true" : "false"));
		LOG_INF("  -> Phi Offset: %i", results[idx].phi_offset);
		LOG_INF("  -> Timestamp: %llu us", results[idx].timestamp_us);
		LOG_INF("  -> Beacon period: %u TU\n", results[idx].beacon_period_tu);
	}
}

void wifi_scan_print_extended_full_results(lr11xx_wifi_extended_full_result_t *results,
					   uint8_t nb_results)
{
	for (uint8_t idx = 0; idx < nb_results; idx++) {
		lr11xx_wifi_mac_origin_t mac_origin = LR11XX_WIFI_ORIGIN_BEACON_FIX_AP;
		lr11xx_wifi_channel_t channel = LR11XX_WIFI_NO_CHANNEL;
		bool rssi_validity = false;
		lr11xx_wifi_parse_channel_info(results[idx].channel_info_byte, &channel,
					       &rssi_validity, &mac_origin);

		lr11xx_wifi_signal_type_result_t wifi_signal_type = {0};
		lr11xx_wifi_datarate_t wifi_data_rate = {0};
		lr11xx_wifi_parse_data_rate_info(results[idx].data_rate_info_byte,
						 &wifi_signal_type, &wifi_data_rate);

		LOG_INF("Result %u/%u", idx + 1, nb_results);
		prv_print_mac_address("  -> MAC address: ", results[idx].mac_address_1);
		prv_print_mac_address("  -> MAC address: ", results[idx].mac_address_2);
		prv_print_mac_address("  -> MAC address: ", results[idx].mac_address_3);
		LOG_INF("  -> Country code: %c%c", (uint8_t)(results[idx].country_code[0]),
			(uint8_t)(results[idx].country_code[1]));
		LOG_INF("  -> Channel: %d", channel);
		LOG_INF("  -> MAC origin: %s",
			(rssi_validity ? "From gateway" : "From end device"));
		LOG_INF("  -> RSSI: %d", results[idx].rssi);
		LOG_INF("  -> Signal type: %s\n",
			prv_wifi_signal_type_result_to_str(
				lr11xx_wifi_extract_signal_type_from_data_rate_info(
					results[idx].data_rate_info_byte)));
		LOG_INF("  -> RSSI: %i dBm", results[idx].rssi);
		LOG_INF("  -> Rate index: 0x%02x", results[idx].rate);
		LOG_INF("  -> Service: 0x%04x", results[idx].service);
		LOG_INF("  -> Length: %u", results[idx].length);
		LOG_INF("  -> Frame control: 0x%04X", results[idx].frame_control);
		LOG_INF("  -> Data rate: %s", prv_wifi_datarate_to_str(wifi_data_rate));
		LOG_INF("  -> MAC origin: %s",
			(rssi_validity ? "From gateway" : "From end device"));
		LOG_INF("  -> Phi Offset: %i", results[idx].phi_offset);
		LOG_INF("  -> Timestamp: %llu us", results[idx].timestamp_us);
		LOG_INF("  -> Beacon period: %u TU", results[idx].beacon_period_tu);
		LOG_INF("  -> Sequence control: 0x%04x", results[idx].seq_control);
		LOG_INF("  -> IO regulation: 0x%02x", results[idx].io_regulation);
		LOG_INF("  -> Current channel: %d", results[idx].current_channel);
		LOG_INF("  -> FCS status:\n    - %s", (results[idx].fcs_check_byte.is_fcs_checked)
							      ? "Is present"
							      : "Is not present");
		LOG_INF("    - %s",
			(results[idx].fcs_check_byte.is_fcs_ok) ? "Valid" : "Not valid");
	}
}
