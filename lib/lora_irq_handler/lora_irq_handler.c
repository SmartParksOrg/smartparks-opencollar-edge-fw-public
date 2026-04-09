/** @file lora_irq_handler.c
 *
 * @brief Interface for lora irq handler operations. This module is used for all custom lora irq
 * operations, when the smtc modem is (temporarily) suspended.
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2025 Irnas. All rights reserved.
 */

#include "lr11xx_board.h"
#include "lr11xx_system.h"
#include "lr11xx_system_types.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <lora_irq_handler.h>

LOG_MODULE_REGISTER(lora_irq_handler);

#define LORA_IRQ_HANDLER_STACK_SIZE 2048
#define LORA_IRQ_HANDLER_PRIORITY   5

static const struct device *prv_lora_dev;
static const struct lr11xx_irq_callbacks *prv_callbacks;

/* Semaphore controlling number of IRQ firings (up to 100) */
static K_SEM_DEFINE(lora_irq_sem, 0, 100);

/**
 * @brief Lora IRQ handler
 *
 * @param dev - device pointer to lora chip
 */
static void prv_lora_irq(const struct device *dev)
{
	LOG_DBG("prv_lora_irq");
	k_sem_give(&lora_irq_sem);
}

/**
 * @brief Process lora IRQs
 *
 * @param dev - device pointer to lora chip
 */
static void prv_lora_irq_process(const struct device *dev)
{
	lr11xx_system_irq_mask_t irq_regs;
	lr11xx_system_get_and_clear_irq_status(dev, &irq_regs);

	LOG_DBG("Interrupt flags = 0x%08X", irq_regs);

	if ((irq_regs & LR11XX_SYSTEM_IRQ_TX_DONE) == LR11XX_SYSTEM_IRQ_TX_DONE) {
		if (prv_callbacks->tx_done) {
			prv_callbacks->tx_done(dev);
		}
	}
	if ((irq_regs & LR11XX_SYSTEM_IRQ_RX_DONE) == LR11XX_SYSTEM_IRQ_RX_DONE) {
		if (prv_callbacks->rx_done) {
			prv_callbacks->rx_done(dev);
		}
	}
	if ((irq_regs & LR11XX_SYSTEM_IRQ_PREAMBLE_DETECTED) ==
	    LR11XX_SYSTEM_IRQ_PREAMBLE_DETECTED) {
		if (prv_callbacks->preamble_detected) {
			prv_callbacks->preamble_detected(dev);
		}
	}
	if ((irq_regs & LR11XX_SYSTEM_IRQ_SYNC_WORD_HEADER_VALID) ==
	    LR11XX_SYSTEM_IRQ_SYNC_WORD_HEADER_VALID) {
		if (prv_callbacks->sync_word_header_valid) {
			prv_callbacks->sync_word_header_valid(dev);
		}
	}
	if ((irq_regs & LR11XX_SYSTEM_IRQ_HEADER_ERROR) == LR11XX_SYSTEM_IRQ_HEADER_ERROR) {
		if (prv_callbacks->header_error) {
			prv_callbacks->header_error(dev);
		}
	}
	if ((irq_regs & LR11XX_SYSTEM_IRQ_CRC_ERROR) == LR11XX_SYSTEM_IRQ_CRC_ERROR) {
		if (prv_callbacks->crc_error) {
			prv_callbacks->crc_error(dev);
		}
	}
	if ((irq_regs & LR11XX_SYSTEM_IRQ_CAD_DONE) == LR11XX_SYSTEM_IRQ_CAD_DONE) {
		if (prv_callbacks->cad_done) {
			prv_callbacks->cad_done(dev);
		}
	}
	if ((irq_regs & LR11XX_SYSTEM_IRQ_CAD_DETECTED) == LR11XX_SYSTEM_IRQ_CAD_DETECTED) {
		if (prv_callbacks->cad_detected) {
			prv_callbacks->cad_detected(dev);
		}
	}
	if ((irq_regs & LR11XX_SYSTEM_IRQ_TIMEOUT) == LR11XX_SYSTEM_IRQ_TIMEOUT) {
		if (prv_callbacks->timeout) {
			prv_callbacks->timeout(dev);
		}
	}
	if ((irq_regs & LR11XX_SYSTEM_IRQ_LR_FHSS_INTRA_PKT_HOP) ==
	    LR11XX_SYSTEM_IRQ_LR_FHSS_INTRA_PKT_HOP) {
		if (prv_callbacks->fhss_intra_pkt_hop) {
			prv_callbacks->fhss_intra_pkt_hop(dev);
		}
	}
	if ((irq_regs & LR11XX_SYSTEM_IRQ_GNSS_SCAN_DONE) == LR11XX_SYSTEM_IRQ_GNSS_SCAN_DONE) {
		if (prv_callbacks->gnss_scan_done) {
			prv_callbacks->gnss_scan_done(dev);
		}
	}
	if ((irq_regs & LR11XX_SYSTEM_IRQ_WIFI_SCAN_DONE) == LR11XX_SYSTEM_IRQ_WIFI_SCAN_DONE) {
		if (prv_callbacks->wifi_scan_done) {
			prv_callbacks->wifi_scan_done(dev);
		}
	}
	if ((irq_regs & LR11XX_SYSTEM_IRQ_EOL) == LR11XX_SYSTEM_IRQ_EOL) {
		if (prv_callbacks->eol) {
			prv_callbacks->eol(dev);
		}
	}
	if ((irq_regs & LR11XX_SYSTEM_IRQ_CMD_ERROR) == LR11XX_SYSTEM_IRQ_CMD_ERROR) {
		if (prv_callbacks->cmd_error) {
			prv_callbacks->cmd_error(dev);
		}
	}
	if ((irq_regs & LR11XX_SYSTEM_IRQ_ERROR) == LR11XX_SYSTEM_IRQ_ERROR) {
		if (prv_callbacks->error) {
			prv_callbacks->error(dev);
		}
	}
	if ((irq_regs & LR11XX_SYSTEM_IRQ_FSK_LEN_ERROR) == LR11XX_SYSTEM_IRQ_FSK_LEN_ERROR) {
		if (prv_callbacks->fsk_len_error) {
			prv_callbacks->fsk_len_error(dev);
		}
	}
	if ((irq_regs & LR11XX_SYSTEM_IRQ_FSK_ADDR_ERROR) == LR11XX_SYSTEM_IRQ_FSK_ADDR_ERROR) {
		if (prv_callbacks->fsk_addr_error) {
			prv_callbacks->fsk_addr_error(dev);
		}
	}
	/* IRQs not handled: LR11XX_SYSTEM_IRQ_NONE */
}

/**
 * @brief Lora IRQ processing thread function
 *
 * @param p1 - unused
 * @param p2 - unused
 * @param p3 - unused
 */
static void prv_lora_irq_process_thread_fn(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (1) {
		k_sem_take(&lora_irq_sem, K_FOREVER);
		prv_lora_irq_process(prv_lora_dev);
	}
}

K_THREAD_DEFINE(lora_irq, LORA_IRQ_HANDLER_STACK_SIZE, prv_lora_irq_process_thread_fn, NULL, NULL,
		NULL, LORA_IRQ_HANDLER_PRIORITY, 0, 1000);

int lr11xx_irq_thread_start_processing(const struct device *dev,
				       const struct lr11xx_irq_callbacks *callbacks,
				       lr11xx_system_irq_mask_t irq_mask)
{
	int err;

	__ASSERT_NO_MSG(dev);
	__ASSERT_NO_MSG(callbacks);

	prv_lora_dev = dev;
	prv_callbacks = callbacks;

	err = lr11xx_system_set_dio_irq_params(dev, irq_mask, LR11XX_SYSTEM_IRQ_NONE);
	if (err != 0) {
		LOG_ERR("lr11xx_set_dio_irq_params, err: %d", err);
		return -EIO;
	}

	err = lr11xx_system_clear_irq_status(dev, LR11XX_SYSTEM_IRQ_ALL_MASK);
	if (err != 0) {
		LOG_ERR("lr11xx_clear_irq_status, err: %d", err);
		return -EIO;
	}

	lr11xx_board_attach_interrupt(dev, prv_lora_irq);
	lr11xx_board_enable_interrupt(dev);

	return 0;
}

void lr11xx_irq_thread_stop_processing(const struct device *dev)
{
	int err = lr11xx_system_clear_irq_status(dev, LR11XX_SYSTEM_IRQ_ALL_MASK);
	if (err != 0) {
		LOG_ERR("lr11xx_clear_irq_status, err: %d", err);
	}

	lr11xx_board_disable_interrupt(dev);
	lr11xx_board_attach_interrupt(dev, NULL);
}
