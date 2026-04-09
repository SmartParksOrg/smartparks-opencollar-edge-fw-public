/** @file lora_irq_handler.h
 *
 * @brief Interface for lora irq handler operations
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2025 Irnas.  All rights reserved.
 */

#ifndef LORA_IRQ_HANDLER_H
#define LORA_IRQ_HANDLER_H

#include "lr11xx_system_types.h"
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <zephyr/device.h>

/**
 * @brief Structure holding LR11xx IRQ event callbacks.
 *
 * Each function pointer corresponds to a specific LR11xx interrupt event.
 * All callbacks are optional; if a callback pointer is NULL, that event will be ignored.
 */
struct lr11xx_irq_callbacks {
	/**
	 * @brief Called when a TX operation completes successfully.
	 *
	 * Triggered by the LR11XX_SYSTEM_IRQ_TX_DONE event.
	 */
	void (*tx_done)(const struct device *dev);

	/**
	 * @brief Called when a packet has been received successfully.
	 *
	 * Triggered by the LR11XX_SYSTEM_IRQ_RX_DONE event.
	 */
	void (*rx_done)(const struct device *dev);

	/**
	 * @brief Called when a radio preamble is detected.
	 *
	 * Triggered by the LR11XX_SYSTEM_IRQ_PREAMBLE_DETECTED event.
	 */
	void (*preamble_detected)(const struct device *dev);

	/**
	 * @brief Called when a valid sync word or header is detected.
	 *
	 * Triggered by the LR11XX_SYSTEM_IRQ_SYNC_WORD_HEADER_VALID event.
	 */
	void (*sync_word_header_valid)(const struct device *dev);

	/**
	 * @brief Called when a header CRC or format error occurs.
	 *
	 * Triggered by the LR11XX_SYSTEM_IRQ_HEADER_ERROR event.
	 */
	void (*header_error)(const struct device *dev);

	/**
	 * @brief Called when a CRC error is detected in a received packet.
	 *
	 * Triggered by the LR11XX_SYSTEM_IRQ_CRC_ERROR event.
	 */
	void (*crc_error)(const struct device *dev);

	/**
	 * @brief Called when a CAD (Channel Activity Detection) operation completes.
	 *
	 * Triggered by the LR11XX_SYSTEM_IRQ_CAD_DONE event.
	 */
	void (*cad_done)(const struct device *dev);

	/**
	 * @brief Called when a CAD operation detects activity on the channel.
	 *
	 * Triggered by the LR11XX_SYSTEM_IRQ_CAD_DETECTED event.
	 */
	void (*cad_detected)(const struct device *dev);

	/**
	 * @brief Called when a configured RX or TX timeout expires.
	 *
	 * Triggered by the LR11XX_SYSTEM_IRQ_TIMEOUT event.
	 */
	void (*timeout)(const struct device *dev);

	/**
	 * @brief Called when a frequency hop occurs during an ongoing FHSS packet.
	 *
	 * Triggered by the LR11XX_SYSTEM_IRQ_LR_FHSS_INTRA_PKT_HOP event.
	 */
	void (*fhss_intra_pkt_hop)(const struct device *dev);

	/**
	 * @brief Called when a GNSS scan operation completes.
	 *
	 * Triggered by the LR11XX_SYSTEM_IRQ_GNSS_SCAN_DONE event.
	 */
	void (*gnss_scan_done)(const struct device *dev);

	/**
	 * @brief Called when a Wi-Fi scan operation completes.
	 *
	 * Triggered by the LR11XX_SYSTEM_IRQ_WIFI_SCAN_DONE event.
	 */
	void (*wifi_scan_done)(const struct device *dev);

	/**
	 * @brief Called when an End-Of-List event occurs.
	 *
	 * Triggered by the LR11XX_SYSTEM_IRQ_EOL event, typically signaling the end of
	 * a scheduled list or operation sequence.
	 */
	void (*eol)(const struct device *dev);

	/**
	 * @brief Called when a command execution error occurs.
	 *
	 * Triggered by the LR11XX_SYSTEM_IRQ_CMD_ERROR event.
	 */
	void (*cmd_error)(const struct device *dev);

	/**
	 * @brief Called when a general error condition is reported by the device.
	 *
	 * Triggered by the LR11XX_SYSTEM_IRQ_ERROR event.
	 */
	void (*error)(const struct device *dev);

	/**
	 * @brief Called when an FSK packet length error is detected.
	 *
	 * Triggered by the LR11XX_SYSTEM_IRQ_FSK_LEN_ERROR event.
	 */
	void (*fsk_len_error)(const struct device *dev);

	/**
	 * @brief Called when an FSK address filtering error occurs.
	 *
	 * Triggered by the LR11XX_SYSTEM_IRQ_FSK_ADDR_ERROR event.
	 */
	void (*fsk_addr_error)(const struct device *dev);
};

/**
 * @brief Start lora IRQ processing thread

 * @param dev - device pointer to lora chip
 * @param callbacks - pointer to lora irq callbacks structure
 *
 * @return int 0 on success, negative error code otherwise
 */
int lr11xx_irq_thread_start_processing(const struct device *dev,
				       const struct lr11xx_irq_callbacks *callbacks,
				       lr11xx_system_irq_mask_t irq_mask);

/**
 * @brief Stop lora IRQ processing.
 *
 * @param dev - device pointer to lora chip
 *
 * This needs to be called before returning control to smtc.
 */
void lr11xx_irq_thread_stop_processing(const struct device *dev);

#ifdef __cplusplus
}
#endif

#endif /* LORA_IRQ_HANDLER_H */
