/** @file communication.h
 *
 * @brief Scheduling interface for communication module.
 * Handles time-dependent communication tasks, including:
 * - lorawan join
 * - lora gps
 * - status message send
 * - wifi scan and aggregated data send
 * - bt scan and aggregated data send
 * - ublox gps
 * - lr messaging
 * - satellite send - external module
 * - fence - external module
 *
 * It also handles commands for direct execution of tasks, coming from other threads.
 *
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2023 Irnas. All rights reserved.
 */

#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include <zephyr/kernel.h>

/**
 * @brief Check if any new message is received in communication thread. Parse and execute message.
 *
 */
void handle_communication_thread_messages(void);

/**
 * @brief Periodically attempt join process.
 *
 */
void handle_lr_join(void);

/**
 * @brief Periodically obtain lr position.
 *
 */
void handle_lr_gps(void);

/**
 * @brief Periodically send status.
 *
 */
void handle_send_status(void);

/**
 * @brief Periodically perform wifi scan.
 *
 */
void handle_wifi_scan(void);

/**
 * @brief Periodically send wifi scan aggregated data
 *
 */
void handle_wifi_scan_aggregated(void);

/**
 * @brief Periodically request BT scan from BT module.
 *
 */
void handle_bt_scan(void);

/**
 * @brief Periodically request BT module to send/store message containing aggregated BT scan if
 * appropriate.
 *
 */
void handle_bt_scan_aggregated(void);

/**
 * @brief Periodically handle BT cmdq operation.
 *
 */
void handle_bt_cmdq(void);

/**
 * @brief Periodically acquire GPS fix.
 *
 */
void handle_ublox_gps(void);

/**
 * @brief Check if gps data needs to be resent.
 *
 */
void handle_resend_ublox_gps();

/**
 * @brief Check if there are any outgoing messages in lr messaging que to be send. If yes, send
 * them.
 *
 */
void handle_lr_messaging(void);

/**
 * @brief Check if we need to send status and position message via lr s-band.
 *
 */
void handle_s_band_send(void);

/**
 * @brief Periodically send satellite messages if HW supported.
 *
 */
void handle_satellite_send(void);

/**
 * @brief Periodically handle vhf burst.
 *
 */
void handle_vhf_burst(void);

/**
 * @brief Periodically measure fence pulses if HW supported.
 *
 */
void handle_fence(void);

/**
 * @brief Periodically check external switch state and report status.
 *
 */
void handle_external_switch(void);

/**
 * @brief Periodically check if latest air quality data needs to be sent.
 */
void handle_air_quality(void);

/**
 * @brief Start lorawan module. Call before attempting any other communication with module.
 *
 */
void lr_start(void);

/**
 * @brief Re-Initialize LR module.
 *
 */
int lr_reset(void);

/**
 * @brief Handle received messages. Pass handler to lr interface module.
 *
 * @param[in] message - message
 * @param[in] size - message size
 * @param[in] port - message port
 */
void handle_lora_recv(const uint8_t *message, uint8_t size, uint8_t port);

/**
 * @brief Initialize GPS module.
 *
 * @retval 0 if ok
 * @retval -ENXIO - GPS module not supported in dts
 * @retval -EIO - cannot init communication peripheral
 */
int init_gps_module(void);

#endif
/*** end of file ***/
