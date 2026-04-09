/** @file lp0_offload.h
 *
 * @brief LP0 ping and offloading functions used by LP0. This file serves to declutter LP0 of
 * offloading features and improve readability. It is not meant to be a standalone offloading
 * module and is intended to be tightly coupled with LP0.
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2026 Irnas.  All rights reserved.
 */

#ifndef LP0_OFFLOAD_H
#define LP0_OFFLOAD_H

#ifdef __cplusplus
extern "C" {
#endif

#include <zephyr/kernel.h>
#include <lr11xx_radio_types.h>

#include <lp0_common.h>

/**
 * @brief Handle received LP0 offload message
 *
 * @param[in] payload Pointer to the received payload.
 * @param[in,out] len Pointer to the length of the received payload.
 * @param[in,out] prv_mode Pointer to the current LP0 mode.
 */
void lp0_offload_handle_lp0_recv(uint8_t *payload, size_t *len, enum lp0_mode *prv_mode);

/**
 * @brief Send discovery ping to a offload station as the device that will offload data and perform
 * RX windows for discovery acknowledgment.
 */
void lp0_offload_device_discovery_mode(void);

/**
 * @brief Send ping acknowledge message in offload station mode, acknowledging the discovery of a
 * device.
 */
void lp0_offload_station_discovery_acknowledge(void);

/**
 * @brief Handle received message when in offload station mode, parsing the raw payload and saving
 * it to SDFS over UART.
 *
 * @param raw_payload Pointer to raw payload bytes received from the radio.
 * @param raw_len Pointer to length of raw payload.
 * @param pkt_type Packet type of the received message.
 * @param pkt_status Pointer to packet status structure containing metadata about the received
 * message.
 *
 * @return int 0 on success, negative error code on failure.
 */
int lp0_offload_station_data_receive(uint8_t *raw_payload, size_t *raw_len,
				     lr11xx_radio_pkt_type_t pkt_type, void *pkt_status);

/**
 * @brief As the offloading station listen for discovery pings from devices that want to offload
 * data.
 */
void lp0_offload_station_discovery(void);

#ifdef __cplusplus
}
#endif

#endif /* LP0_OFFLOAD_H */
