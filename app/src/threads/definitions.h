/** @file definitions.h
 *
 * @brief A description of the module’s purpose.
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2020 Irnas.  All rights reserved.
 */

#ifndef DEFINITIONS_H
#define DEFINITIONS_H

/* BLE */
#define BLE_RADIO_TX_POWER_DBM 8

#define MAX_BUF_SIZE     255 // Max length of message and payload buffers
#define MESSAGE_HEAD_LEN 2   // Length of response message header: msg_id + msg_len
#define MESSAGE_HEAD_LEN_BT                                                                        \
	3 // Length of response message header for BT and flash messages, containing port field
#define MESSAGE_LR_MAX_LEN MAX_BUF_SIZE // MAx supported LR message length
#define MESSAGE_DATA_MAX_LEN                                                                       \
	MESSAGE_LR_MAX_LEN - MESSAGE_HEAD_LEN_BT // Max data length, allowing for long header
#define MESSAGE_BT_MAX_LEN MIN(CONFIG_BT_NUS_BUFFER_SIZE - 4, MAX_BUF_SIZE)
#define TIMESTAMP_SIZE     4

/* other defines */
#define CHARGING_THRESHOLD_V           5000
#define BATTERY_READ_INTERVAL_MS       60000
#define CHARGING_READ_INTERVAL_MS      60000
#define CHARGING_READ_INTERVAL_FAST_MS 1000 // Read interval in te case of charging
#define ADV_DATA_UPDATE_INTERVAL       60000
#define SENSORS_READ_INTERVAL          ADV_DATA_UPDATE_INTERVAL
#define TEMP_READ_INTERVAL             ADV_DATA_UPDATE_INTERVAL
#define REED_CHECK_INTERVAL            10000

#ifdef CONFIG_BOARD_RHINOEDGE_NRF52840
#define FULL_BATTERY_LEVEL     3600
#define LOW_BATTERY_LEVEL      1900
#define CRITICAL_BATTERY_LEVEL 1800
#elif CONFIG_BOARD_RANGEREDGE_NRF52840 || CONFIG_BOARD_RANGEREDGE_AIRQ_NRF52840
#define FULL_BATTERY_LEVEL     4150
#define LOW_BATTERY_LEVEL      3100
#define CRITICAL_BATTERY_LEVEL 2900
#elif CONFIG_BOARD_RHINOPUCK_NRF52840
#define FULL_BATTERY_LEVEL     3600
#define LOW_BATTERY_LEVEL      1900
#define CRITICAL_BATTERY_LEVEL 1800
#elif CONFIG_BOARD_RHINOPUCK35_NRF52840
#define FULL_BATTERY_LEVEL     3600
#define LOW_BATTERY_LEVEL      1900
#define CRITICAL_BATTERY_LEVEL 1800
#elif CONFIG_BOARD_COLLAREDGE_NRF52840
#define FULL_BATTERY_LEVEL     3600
#define LOW_BATTERY_LEVEL      2800
#define CRITICAL_BATTERY_LEVEL 2500
#elif CONFIG_BOARD_FREEEDGE_NRF52840
#define FULL_BATTERY_LEVEL     3600
#define LOW_BATTERY_LEVEL      2800
#define CRITICAL_BATTERY_LEVEL 2500
#else
#define FULL_BATTERY_LEVEL     3600
#define LOW_BATTERY_LEVEL      2800
#define CRITICAL_BATTERY_LEVEL 2500
#endif

#define BATTERY_THRESHOLD 25 // Avoid jumping to and from states

#define THREAD_LP_SLEEP        2000 // Idle sleep time
#define THREAD_CONNECTED_SLEEP 100  // Sleep time when in BT connected mode

/* Wifi and BT scan */
#define BT_SCAN_MAX_RES 20

#endif /* DEFINITIONS_H */

/*** end of file ***/
