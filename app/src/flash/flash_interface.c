/** @file flash_interface.c
 *
 * @brief Interface for flash and data logging
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2020 Irnas. All rights reserved.
 */
#include <stdint.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>

#include <pm_config.h>

#include "common_functions.h"
#include "flash_ext_partitions.h"
#include "generated_settings.h"
#include "global_time.h"
#include "led.h"
#include "lp0.h"
#include "lp0_communication.h"
#include "nvs_storage.h"
#include "thread_com.h"
#include "thread_operation.h"
#include "thread_watchdog.h"

#include "flash_interface.h"

#define FLASH_DEVICE DT_ALIAS(external_flash)

/* The config below gets set if we're using zephyr-spi-flash-en25-driver */
#ifdef CONFIG_SPI_NOR_FLASH_LAYOUT_PAGE_SIZE
#define FLASH_BLOCK_SIZE CONFIG_SPI_NOR_FLASH_LAYOUT_PAGE_SIZE
#else
#define FLASH_BLOCK_SIZE DT_PROP(DT_ALIAS(external_flash), erase_sector_size)
#endif

#define FLASH_PAGE_SIZE 256 /* Largest chunk that is saved at a time into flash */

#define FLASH_BUFFER_SIZE          FLASH_PAGE_SIZE
#define FLASH_HEAD_SIZE            MESSAGE_HEAD_LEN_BT
#define MIN_FLASH_WRITE_BLOCK_SIZE 4

/* Flash read command offsets */
#define FLASH_READ_PORT_OFFSET                    2
#define FLASH_READ_START_OFFSET                   6
#define FLASH_READ_NUM_OF_MESSAGES_TO_READ_OFFSET 10
#define FLASH_READ_NUM_OF_MESSAGES_PER_BATCH      6

#define FLASH_READ_CONTINUE_MSG_LEN 12

LOG_MODULE_REGISTER(FLASH_INTERFACE, 3); /* init logging */

/* Flash enabled status */
static bool prv_flash_enable = false;

/* Private buffer to store send/receive thread messages into */
static uint8_t prv_flash_payload[MAX_BUF_SIZE];

/* Private buffer to store messages before writing to flash */
static uint8_t prv_flash_buffer[FLASH_BUFFER_SIZE];
/* Private buffer index */
static int prv_buffer_idx = 0;
/* Number of messages stored in private buffer */
static uint8_t prv_n_msg_buffer = 0;

/* Flash write offset */
static off_t prv_flash_offset = 0;
/* Flash start offset - if wraparound this will be different from 0 */
static off_t prv_flash_start_offset = 0;
/* Current flash block offset */
static off_t prv_block_offset = 0;
/* Left space in current block */
static uint32_t prv_left_block_size = 0;

/* Last flash status message send */
static uint32_t prv_last_flash_status_send = 0;

const struct device *flash_dev = DEVICE_DT_GET(FLASH_DEVICE);

/**
 * @brief Erase flash or part of it.
 *
 * @param offset - flash offset
 * @param size - size
 *
 * @return negative integer error code or 0 if ok.
 */
static int prv_erase_from_flash(uint32_t offset, uint32_t size)
{
	int rc = -ENXIO;
	if (flash_dev) {
		if (k_mutex_lock(&spi_mutex, K_MSEC(SPI_MUTEX_TIMEOUT)) == 0) {
#if IS_ENABLED(CONFIG_PM_DEVICE)
			int err = pm_device_action_run(flash_dev, PM_DEVICE_ACTION_RESUME);
			if (err) {
				LOG_DBG("Failed to resume device: %d, manually sending wakeup "
					"command.",
					err);
			}

#endif /* IS_ENABLED(CONFIG_PM_DEVICE) */
			rc = flash_erase(flash_dev, offset, size);
#if IS_ENABLED(CONFIG_PM_DEVICE)
			err = pm_device_action_run(flash_dev, PM_DEVICE_ACTION_SUSPEND);
			if (err) {
				LOG_DBG("Failed to suspend device: %d, manually sending suspend "
					"command.",
					err);
			}
#endif /* IS_ENABLED(CONFIG_PM_DEVICE) */
			k_mutex_unlock(&spi_mutex);
		} else {
			LOG_DBG("SPI mutex timeout!");
			rc = -EAGAIN;
		}
		if (rc != 0) {
			LOG_ERR("Flash erase failed! %d", rc);
		} else {
			LOG_DBG("Flash erase succeeded!");
		}
	}

	return rc;
}

/**
 * @brief Write to external flash
 *
 * @param offset - flash offset
 * @param buf - buffer to write
 * @param size - nr. of bytes to write
 *
 * @return negative integer error code or 0 if ok.
 */
static int prv_write_to_flash(uint32_t offset, uint8_t *buf, uint32_t len)
{
	int rc = -ENXIO;
	if (flash_dev) {
		if (k_mutex_lock(&spi_mutex, K_MSEC(SPI_MUTEX_TIMEOUT)) == 0) {
#if IS_ENABLED(CONFIG_PM_DEVICE)
			int err = pm_device_action_run(flash_dev, PM_DEVICE_ACTION_RESUME);
			if (err) {
				LOG_DBG("Failed to resume device: %d, manually sending wakeup "
					"command.",
					err);
			}

#endif /* IS_ENABLED(CONFIG_PM_DEVICE) */
			rc = flash_write(flash_dev, offset, buf, len);
#if IS_ENABLED(CONFIG_PM_DEVICE)
			err = pm_device_action_run(flash_dev, PM_DEVICE_ACTION_SUSPEND);
			if (err) {
				LOG_DBG("Failed to suspend device: %d, manually sending suspend "
					"command.",
					err);
			}
#endif /* IS_ENABLED(CONFIG_PM_DEVICE) */
			k_mutex_unlock(&spi_mutex);
		} else {
			LOG_DBG("SPI mutex timeout!");
			rc = -EAGAIN;
		}
		if (rc != 0) {
			LOG_ERR("Flash write failed! %d\n", rc);
		} else {
			LOG_DBG("Flash write succeeded!");
		}
	}

	return rc;
}

/**
 * @brief Read from external flash and lock mutex
 *
 * @param offset - flash offset
 * @param buf - buffer to read into
 * @param size - nr. of bytes to read
 *
 * @return negative integer error code or 0 if ok.
 */
static int prv_read_from_flash(uint32_t offset, uint8_t *buf, uint32_t len)
{
	int rc = 0;
	if (flash_dev) {
		if (k_mutex_lock(&spi_mutex, K_MSEC(SPI_MUTEX_TIMEOUT)) == 0) {
#if IS_ENABLED(CONFIG_PM_DEVICE)
			int err = pm_device_action_run(flash_dev, PM_DEVICE_ACTION_RESUME);
			if (err) {
				LOG_DBG("Failed to resume device: %d, manually sending wakeup "
					"command.",
					err);
			}

#endif /* IS_ENABLED(CONFIG_PM_DEVICE) */
			rc = flash_read(flash_dev, offset, buf, len);
#if IS_ENABLED(CONFIG_PM_DEVICE)
			err = pm_device_action_run(flash_dev, PM_DEVICE_ACTION_SUSPEND);
			if (err) {
				LOG_DBG("Failed to suspend device: %d, manually sending suspend "
					"command.",
					err);
			}
#endif /* IS_ENABLED(CONFIG_PM_DEVICE) */
			k_mutex_unlock(&spi_mutex);
		} else {
			LOG_DBG("SPI mutex timeout!");
			rc = -EAGAIN;
		}
		if (rc != 0) {
			LOG_ERR("Flash read failed! %d\n", rc);
		} else {
			LOG_DBG("Flash read succeeded!");
		}
	}
	return rc;
}

/**
 * @brief Calculate used flash space in percentage.
 *
 * @return uint8_t used space in percentage
 */
static uint8_t prv_calculate_used_flash_percentage(void)
{
	uint8_t percentage = 0;

	size_t flash_size = 0;
	int err = flash_ext_get_partition_size(FLASH_EXT_PARTITION_MESSAGE_STORAGE, &flash_size);
	if (err) {
		LOG_ERR("Failed to get flash size: %d", err);
		return 0;
	}

	if (flash_size == 0) {
		LOG_ERR("Flash size is 0! Can not calculate percentage!");
		return 0;
	}

	/* We have reached wraparound */
	if (prv_flash_start_offset > 0) {
		percentage = 100;
	} else {
		if (prv_flash_offset > 0) {

			percentage = (uint8_t)((prv_flash_offset * 100) / flash_size);
		}
	}

	LOG_DBG("Flash start offset: %ld, flash offset: %ld, flash size: %d, percentage: %d",
		prv_flash_start_offset, prv_flash_offset, flash_size, percentage);
	return percentage;
}

/**
 * @brief Compose flash status message.
 *
 * @return uint8_t message size
 */
static uint8_t prv_compose_flash_status_msg(void)
{
	/* Populate payload message */
	prv_flash_payload[0] = Main_messages.msg_flash_status->id;
	prv_flash_payload[1] = Main_messages.msg_flash_status->length;
	prv_flash_payload[2] = prv_calculate_used_flash_percentage();
	memcpy(prv_flash_payload + 3, &Main_values.flash_nr_msg->def_val,
	       sizeof(Main_values.flash_nr_msg->def_val));

	return MESSAGE_HEAD_LEN + Main_messages.msg_flash_status->length;
}

/**
 * @brief Get nr. of messages from specified block.
 *
 * @param block_off - block start offset
 * @param read_port - read only messages from specific port, if 0 read all
 *
 * @return negative integer error code or nr. of messages if ok.
 */
static int prv_get_nr_msg_in_block(off_t _block_off, uint8_t read_port)
{
	if (!prv_flash_enable) {
		LOG_ERR("Flash not active!");
		return -EIO;
	}

	uint8_t port;
	uint8_t len;

	int off = _block_off;
	uint32_t N = 0;

	/* Determine end of block */
	off_t block_end = _block_off + FLASH_BLOCK_SIZE;
	/* If reading last block, read only written part */
	if (_block_off == prv_block_offset) {
		block_end = prv_flash_offset;
	}

	LOG_DBG("Determine nr. of messages in block with start offset: %ld and end: %ld. Start at "
		": %d",
		_block_off, block_end, off);
	/* Read port */
	/* loop over messages in block */
	while (off < block_end) {
		prv_read_from_flash(off, &port, 1);
		if (port > 32) {
			LOG_DBG("Invalid port: %d", port);
			break;
		}
		off += 2;
		/* Read len */
		prv_read_from_flash(off, &len, 1);
		/* Go to the end of the message */
		off += (len + TIMESTAMP_SIZE + 1);
		/* Check if valid */
		if (off <= block_end) {
			/* Check port */
			if (port == read_port || read_port == 0) {
				N++;
			}
		}
	}

	return N;
}

/**
 * @brief Check if flash payload buffer needs to be send before reading new message.
 *
 * @param msg - payload buffer
 * @param msg_len - current length
 * @param sender - of the message, i.e LR, BT
 * @param max_rsp_len new max payload size
 *
 * @return negative error code
 */
static int prv_send_payload(uint8_t *msg, uint16_t msg_len, mb_msg_dest sender,
			    uint8_t *max_rsp_len)
{
	int err = 0;

	/* If send via. LR, wait for rsp message with new max len */
	if (sender == MB_MSG_DEV) {
		LOG_DBG("Flash test read: ");
		for (int i = 0; i < msg_len; i++) {
			LOG_DBG("%x ", msg[i]);
		}
	} else if (sender == MB_MSG_LORA) {
		/* Put message to respective thread */
		LOG_DBG("Forward flash msg to lr thread!");
		err = thread_put_message(MB_MSG_FLASH, sender, MB_MSG_SEND,
					 Main_messages.msg_read_flash->port, msg, msg_len, 0);
		/* Check if new message */
		mb_msg_dest msg_origin = 0;
		mb_msg_action msg_action = 0;
		uint8_t port = 0;
		int msg_size = 0;
		*max_rsp_len = MESSAGE_LR_MAX_LEN;
		uint64_t start_time = k_uptime_get();
		LOG_DBG("Wait for rsp!");
		while (msg_size <= 0) {
			/* Sleep to allow for other process operation */
			k_sleep(K_MSEC(thread_sleep));
			/* Check timeout (i.e. allow for cold fix to finish) */
			if ((uint32_t)((k_uptime_get() - start_time) / 1000) >
			    Main_settings.cold_fix_timeout->def_val) {
				break;
			}

			msg_size = thread_get_flash(&msg_origin, &msg_action, &port,
						    prv_flash_payload, max_rsp_len);
			/* Check if response */
			if (msg_origin != sender) {
				msg_size = 0;
				continue;
			}
			if (port != Main_messages.msg_cmd_confirm->port) {
				msg_size = 0;
				continue;
			}
			if (prv_flash_payload[0] != Main_messages.msg_cmd_confirm->id) {
				msg_size = 0;
				continue;
			}
			if (prv_flash_payload[2] != Main_messages.msg_read_flash->id) {
				msg_size = 0;
				continue;
			}
			LOG_DBG("Got rsp! New max payload size: %d", *max_rsp_len);
			break;
		}
	} else {
		/* Put message to respective thread */
		err = thread_put_message(MB_MSG_FLASH, sender, MB_MSG_SEND,
					 Main_messages.msg_read_flash->port, msg, msg_len, 0);
		*max_rsp_len = MESSAGE_BT_MAX_LEN;
		k_sleep(K_MSEC(thread_sleep));
	}

	/* Report to watchdog to avoid timeout */
	flash_thread_report();

	return err;
}

/**
 * @brief Check if flash payload buffer needs to be sent before reading new message.
 *
 * @param msg - payload buffer
 * @param msg_len - current length
 * @param new_len - length of the next message
 * @param sender - of the message, i.e LR, BT
 * @param max_rsp_len - max payload size of response message
 *
 * @return uint16_t new buffer idx or negative error code
 */
static int prv_check_payload_send(uint8_t *msg, uint16_t msg_len, uint16_t new_len, uint8_t sender,
				  uint8_t *max_rsp_len)
{
	LOG_DBG("Buf size: %d, new msg: %d max size: %d", msg_len,
		new_len + TIMESTAMP_SIZE + FLASH_HEAD_SIZE, *max_rsp_len);
	/* Validate max length */
	if (sender == MB_MSG_BT && *max_rsp_len > MESSAGE_BT_MAX_LEN) {
		*max_rsp_len = MESSAGE_BT_MAX_LEN;
	} else if (sender == MB_MSG_LORA && *max_rsp_len > MESSAGE_LR_MAX_LEN) {
		*max_rsp_len = MESSAGE_LR_MAX_LEN;
	}

	/* Check if payload needs to be send */
	if (msg_len + new_len + TIMESTAMP_SIZE + FLASH_HEAD_SIZE > *max_rsp_len) {
		LOG_DBG("Send buffer!");
		int err = prv_send_payload(msg, msg_len, sender, max_rsp_len);

		return err; /* If 0 - buffer idx is reset to 0, otherwise we get error */
	}

	return msg_len;
}

/**
 * @brief Read messages from specified block from - to specified message.
 *
 * @param [in] block_off - block start offset
 * @param [in] start - first message in block to read
 * @param [in] end - end message in block to read
 * @param [in] read_port - read only messages from specific port, if 0 read all
 * @param [in] sender - of the message, i.e LR, BT
 * @param [inout] max_rsp_len - max payload size of response message
 * @param [out] busy - if no space in outgoing sender queue, set to `true` signals to stop reading
 * upstream
 *
 * @return negative integer error code or nr. of msg if ok
 */
static int prv_read_block_messages(off_t _block_off, uint32_t start, uint32_t end,
				   uint8_t read_port, uint8_t sender, uint8_t *max_rsp_len,
				   bool *busy)
{
	int rs = 0;

	if (!prv_flash_enable) {
		LOG_ERR("Flash not active!");
		return -EIO;
	}

	int off = _block_off;

	uint8_t port = 0;
	uint8_t id = 0;
	uint8_t len = 0;
	uint16_t payload_idx = 0;
	uint32_t N = 0;           /* Number of msg */
	uint32_t msg_counter = 0; /* Number of read msg */

	/* Determine end of block */
	off_t block_end = _block_off + FLASH_BLOCK_SIZE;
	/* If reading last block, read only written part */
	if (_block_off == prv_block_offset) {
		LOG_DBG("Read last block, up to offset: %ld", prv_flash_offset);
		block_end = prv_flash_offset;
	}
	/* loop over messages in block until all required messages are read or end of block is
	 * reached */
	while (off < block_end && N < end) {
		if (sender == MB_MSG_LORA) {
			/* Break if there is no more space in sender queue */
			extern struct k_msgq event_que;
			if (k_msgq_num_free_get(&event_que) < 2) {
				LOG_WRN("No space left in sender queue, stop reading!");
				*busy = true;
				break;
			}
		} else if (sender == MB_MSG_SAT) {
#ifdef CONFIG_SATELLITE
			/* Break if there is no more space in sender queue */
			extern struct k_msgq satellite_que;
			if (k_msgq_num_free_get(&satellite_que) < 2) {
				LOG_WRN("No space left in sender queue, stop reading!");
				*busy = true;
				break;
			}
#endif
		}

		/* Read port */
		if (prv_read_from_flash(off, &port, 1)) {
			return -EIO;
		}
		if (port > 32) {
			LOG_ERR("Invalid port: %d, stop!", port);
			break;
		}
		N++;
		off++;

		/* Read mess id */
		if (prv_read_from_flash(off, &id, 1)) {
			return -EIO;
		}
		off++;

		/* Read len */
		prv_read_from_flash(off, &len, 1);
		off++;

		/* Check if message is relevant to us */
		/* Check port */
		if (port != read_port && read_port != 0) {
			LOG_DBG("Skip message on port: %d, read only port: %d", port, read_port);
			off += (len + TIMESTAMP_SIZE);
			N--;
			continue;
		} else if (N > end) {
			/* Exit read loop */
			LOG_DBG("All messages are read, exit read loop!");
			break;
		} else if (N < start) {
			/* Skip */
			/* Go to the end of the message */
			LOG_DBG("Skip msg nr.: %d", N);
			off += (len + TIMESTAMP_SIZE);
			continue;
		}

		rs = prv_check_payload_send(prv_flash_payload, payload_idx, (uint16_t)len, sender,
					    max_rsp_len);
		if (rs < 0) {
			return rs;
		}
		payload_idx = rs;

		/* Read message */
		prv_flash_payload[payload_idx + 0] = port;
		prv_flash_payload[payload_idx + 1] = id;
		prv_flash_payload[payload_idx + 2] = len;
		payload_idx += FLASH_HEAD_SIZE;

		LOG_DBG("Read msg on port: %d, with id: %d of length: %d", port, id, len);
		if (prv_read_from_flash(off, prv_flash_payload + payload_idx, (uint32_t)len)) {
			return -EIO;
		}
		off += len;

		payload_idx += len;

		if (prv_read_from_flash(off, prv_flash_payload + payload_idx, TIMESTAMP_SIZE)) {
			return -EIO;
		}
		off += TIMESTAMP_SIZE;
		LOG_DBG("Message timestamp: %d",
			bytes_to_uint32_t(prv_flash_payload + payload_idx));
		payload_idx += TIMESTAMP_SIZE;

		msg_counter++;
	}

	/* Send buffer if non empty */
	if (payload_idx > 0) {
		rs = prv_send_payload(prv_flash_payload, payload_idx, sender, max_rsp_len);
		if (rs) {
			return rs;
		}
	}

	return msg_counter;
}

K_SEM_DEFINE(flash_send_lp0, 0, 1);
K_SEM_DEFINE(flash_offload_start_lp0, 0, 1);
K_SEM_DEFINE(flash_offload_done_lp0, 0, 1);

/**
 * @brief Read messages from specified block and return number of messages in that block (from that
 * specific port). If @p sender is set to MB_MSG_BT, also send messages to BT thread.
 *
 * Set port number to 0 to read all messages.
 *
 * @param block_off - block start offset
 * @param read_port - read only messages from specific port, if 0 read all
 * @param sender - of the message, i.e LR, BT
 * @param max_rsp_len - max payload size of response message
 *
 * @return negative integer error code or nr. of msg in block if ok
 */
static int prv_read_block(off_t _block_off, uint8_t read_port, uint8_t sender, uint8_t *max_rsp_len)
{
	int rs = 0;

	if (!prv_flash_enable) {
		LOG_ERR("Flash not active!");
		return -EIO;
	}

	int off = _block_off;

	uint8_t port = 0;
	uint8_t id = 0;
	uint8_t len = 0;
	uint16_t payload_idx = 0;
	uint32_t N = 0; /* Number of msg */

	/* Determine end of block */
	off_t block_end = _block_off + FLASH_BLOCK_SIZE;
	/* If reading last block, read only written part */
	if (_block_off == prv_block_offset) {
		LOG_DBG("Read last block, up to offset: %ld", prv_flash_offset);
		block_end = prv_flash_offset;
	}
	/* loop over messages in block */
	while (off < block_end) {
		/* Read port */
		if (prv_read_from_flash(off, &port, 1)) {
			return -EIO;
		}

		if (port > 32) {
			LOG_WRN("Invalid port: %d, stop reading block!", port);
			break;
		}
		N++;
		off++;

		/* Read mess id */
		if (prv_read_from_flash(off, &id, 1)) {
			return -EIO;
		}
		off++;

		/* Read len */
		prv_read_from_flash(off, &len, 1);
		off++;

		/* Check port */
		if (port != read_port && read_port != 0) {
			LOG_DBG("Skip message on port: %d, read only port: %d", port, read_port);
			off += (len + TIMESTAMP_SIZE);
			N--;
			continue;
		}

		/* Check if we need to send the payload if message originates from Bluetooth. Else
		 * reset payload index because we are only counting the number of messages
		 */
		if (sender == MB_MSG_BT) {
			rs = prv_check_payload_send(prv_flash_payload, payload_idx, (uint16_t)len,
						    sender, max_rsp_len);
			if (rs < 0) {
				return rs;
			}
			payload_idx = rs;
		} else if (sender == MB_MSG_LP0 && payload_idx > 0) {
			/* LP0 offload: If payload fits add it to the message, if not, send it */
			if (payload_idx + len + TIMESTAMP_SIZE + FLASH_HEAD_SIZE <=
			    LP0_MAX_BUF_SIZE) {
				LOG_DBG("Payload can fit another message %d/%d",
					payload_idx + len + TIMESTAMP_SIZE + FLASH_HEAD_SIZE,
					LP0_MAX_BUF_SIZE);
			} else {
				/* Send payload over LP0 */
				static const struct device *prv_lora_context =
					DEVICE_DT_GET(DT_NODELABEL(lr11xx));

				LOG_INF("Payload can't fit another message %d/255. Sending "
					"buffer...",
					payload_idx + len + TIMESTAMP_SIZE + FLASH_HEAD_SIZE);
				LOG_HEXDUMP_DBG(prv_flash_payload, payload_idx,
						"Offload Flash payload prior to LP0 send:");

				led_turn_on(LED_M);
				k_sem_reset(&flash_send_lp0);
				lp0_prepare_and_send_message(
					prv_lora_context, prv_flash_payload, payload_idx, 29, false,
					Main_settings.lp0_tx_frequency_hz->def_val, false, true,
					LR11XX_RADIO_LORA_IQ_STANDARD);

				payload_idx = 0;
				k_sem_take(&flash_send_lp0, K_FOREVER);

				/* Delay preventing sending too quickly */
				k_sleep(K_MSEC(2000));
			}

		} else {
			payload_idx = 0;
		}

		/* Read message */
		prv_flash_payload[payload_idx + 0] = port;
		prv_flash_payload[payload_idx + 1] = id;
		prv_flash_payload[payload_idx + 2] = len;
		payload_idx += FLASH_HEAD_SIZE;

		LOG_DBG("Read msg on port: %d, with id: %d of length: %d read "
			"offset: %d",
			port, id, len, off);

		/* Read message */
		if (prv_read_from_flash(off, prv_flash_payload + payload_idx, (uint32_t)len)) {
			return -EIO;
		}
		off += len;

		payload_idx += len;

		if (prv_read_from_flash(off, prv_flash_payload + payload_idx, TIMESTAMP_SIZE)) {
			return -EIO;
		}
		off += TIMESTAMP_SIZE;
		LOG_DBG("Message nr: %d with timestamp: %d", N,
			bytes_to_uint32_t(prv_flash_payload + payload_idx));
		payload_idx += TIMESTAMP_SIZE;
	}

	/* Send buffer if buffer has data and sender is MB_MSG_BT (Bluetooth) */
	if (payload_idx > 0 && sender == MB_MSG_BT) {
		rs = prv_send_payload(prv_flash_payload, payload_idx, sender, max_rsp_len);
		if (rs) {
			return rs;
		}
	} else if (sender == MB_MSG_LP0 && payload_idx > 0) {
		/* Send payload over LP0 */
		static const struct device *prv_lora_context = DEVICE_DT_GET(DT_NODELABEL(lr11xx));

		LOG_HEXDUMP_DBG(prv_flash_payload, payload_idx,
				"Offload Flash payload prior to LP0 send:");

		k_sem_reset(&flash_send_lp0);
		lp0_prepare_and_send_message(prv_lora_context, prv_flash_payload, payload_idx, 29,
					     false, Main_settings.lp0_tx_frequency_hz->def_val,
					     false, true, LR11XX_RADIO_LORA_IQ_STANDARD);
		k_sem_take(&flash_send_lp0, K_FOREVER);
		payload_idx = 0;
	}

	return N;
}

/**
 * @brief Get offset of the previous block, given reference offset.
 *
 * @param off_t off - block start offset on reference block
 *
 * @return offset of new block or negative error code. Also return error code if we wanted to go
 * past flash start!
 */
static off_t prv_get_block_prev_offset(off_t off)
{
	if (off == prv_flash_start_offset) {
		LOG_ERR("Got to relative start of the flash, return error!");
		return -EIO;
	}
	off_t new_off = off - FLASH_BLOCK_SIZE;
	/* Check if wraparound */
	if (new_off < 0) {
		size_t flash_size = 0;
		int err = flash_ext_get_partition_size(FLASH_EXT_PARTITION_MESSAGE_STORAGE,
						       &flash_size);
		if (err) {
			LOG_ERR("Failed to get flash size: %d", err);
			return 0;
		}

		new_off = flash_size - FLASH_BLOCK_SIZE;
		LOG_INF("New offset less than 0, wraparound to: %ld", new_off);
	}

	return new_off;
}

/**
 * @brief Get offset of the next block, given reference offset.
 *
 * @param off_t off - block start offset on reference block
 *
 * @return offset of new block or negative error code
 */
static off_t prv_get_block_next_offset(off_t off)
{
	off_t new_off = off + FLASH_BLOCK_SIZE;

	size_t flash_size = 0;
	int err = flash_ext_get_partition_size(FLASH_EXT_PARTITION_MESSAGE_STORAGE, &flash_size);
	if (err) {
		LOG_ERR("Failed to get flash size: %d", err);
	}

	/* Check if wraparound */
	if (new_off >= flash_size) {
		new_off = 0;
	}

	return new_off;
}

/**
 * @brief Go to next flash block.
 *
 * Update write offset, block offset and erase block.
 *
 * @return negative integer error code or 0 if ok.
 */
static int prv_go_to_next_block(void)
{
	if (!prv_flash_enable) {
		LOG_ERR("Flash not active!");
		return -EIO;
	}

	int err = 0;

	/* Go to block */
	prv_flash_offset += prv_left_block_size;

	size_t flash_size = 0;
	err = flash_ext_get_partition_size(FLASH_EXT_PARTITION_MESSAGE_STORAGE, &flash_size);
	if (err) {
		LOG_ERR("Failed to get flash size: %d", err);
		return err;
	}

	/* Check if wraparound */
	if (prv_flash_offset >= flash_size) {
		prv_flash_offset = 0;
	}

	/* Store new offset */
	nvs_storage_write(STORAGE_flash_offset, &prv_flash_offset, sizeof(prv_flash_offset));

	/* Store new block offset */
	prv_block_offset = prv_flash_offset;
	nvs_storage_write(STORAGE_flash_block_offset, &prv_block_offset, sizeof(prv_block_offset));

	/* Check if start offset needs to be moved */
	/* In this case we will erase oldest block - we need to adjust total number of messages */
	if (prv_flash_start_offset == prv_flash_offset) {
		prv_flash_start_offset = prv_get_block_next_offset(prv_flash_start_offset);
		nvs_storage_write(STORAGE_flash_start_offset, &prv_flash_start_offset,
				  sizeof(prv_flash_start_offset));

		/* Get number of messages in the oldest block */
		uint32_t N = prv_get_nr_msg_in_block(prv_flash_start_offset, 0);

		/* Update total number of messages */
		if (N <= Main_values.flash_nr_msg->def_val) {
			Main_values.flash_nr_msg->def_val -= N;
			nvs_storage_write(STORAGE_flash_n_messages,
					  &Main_values.flash_nr_msg->def_val,
					  sizeof(Main_values.flash_nr_msg->def_val));
		}
	}

	/* Erase block */
	prv_erase_from_flash(prv_flash_offset, FLASH_BLOCK_SIZE);

	/* Reset block size */
	prv_left_block_size = FLASH_BLOCK_SIZE;
	nvs_storage_write(STORAGE_flash_block_left, &prv_left_block_size,
			  sizeof(prv_left_block_size));

	LOG_DBG("Go to next block, flash offset: %ld, start flash offset: %ld, left block size: %d",
		prv_flash_offset, prv_flash_start_offset, prv_left_block_size);

	return err;
}

/**
 * @brief Write buffer to flash.
 *
 * @return negative integer error code or 0 if ok.
 */
static int prv_write_buffer_to_flash(void)
{
	if (!prv_flash_enable) {
		LOG_ERR("Flash not active!");
		return -EIO;
	}

	int err = 0;

	/* Check left block size */
	if (prv_buffer_idx > prv_left_block_size) {
		/* If not enough space is left go to next block */
		prv_go_to_next_block();
	}

	/* Write buffer */
	err = prv_write_to_flash(prv_flash_offset, prv_flash_buffer, prv_buffer_idx);

	/* In the case of successful write, update data */
	if (!err) {
		prv_flash_offset += prv_buffer_idx;
		nvs_storage_write(STORAGE_flash_offset, &prv_flash_offset,
				  sizeof(prv_flash_offset));

		prv_left_block_size -= prv_buffer_idx;
		nvs_storage_write(STORAGE_flash_block_left, &prv_left_block_size,
				  sizeof(prv_left_block_size));

		Main_values.flash_nr_msg->def_val += prv_n_msg_buffer;
		nvs_storage_write(STORAGE_flash_n_messages, &Main_values.flash_nr_msg->def_val,
				  sizeof(Main_values.flash_nr_msg->def_val));
	} else {
		LOG_ERR("Write buffer to flash failed with code: %d", err);
	}

	/* Reset buffer */
	memset(prv_flash_buffer, 0, sizeof(prv_flash_buffer));
	prv_buffer_idx = 0;
	prv_n_msg_buffer = 0;

	LOG_DBG("Written buffer to flash: %d", err);

	return err;
}

/**
 * @brief Write single byte to local buffer.
 *
 * @param data - new data byte
 * @return negative integer error code or 0 if ok.
 */
static int prv_add_byte_to_buffer(uint8_t data)
{
	if (!prv_flash_enable) {
		LOG_ERR("Flash not active!");
		return -EIO;
	}

	int err = 0;
	prv_flash_buffer[prv_buffer_idx] = data;
	prv_buffer_idx++;

	/* Check if buffer is full */
	if (prv_buffer_idx == FLASH_BUFFER_SIZE) {
		err = prv_write_buffer_to_flash();
	}

	return err;
}

/**
 * @brief Store incoming messages.
 *
 * @param message - pointer to message
 * @param message_length - message length
 * @param port - port of the message
 *
 * @return negative integer error code or 0 if ok.
 */
static int prv_flash_store_message(uint8_t *message, uint8_t message_length, uint8_t port)
{
	if (!prv_flash_enable) {
		LOG_ERR("Flash not active!");
		return -EIO;
	}

	int err = 0;
	LOG_DBG("Store msg on port: %d, of length: %d", port, message_length);
	LOG_DBG("Flash start offset: %ld, left block size: %d buff idx: %d", prv_flash_offset,
		prv_left_block_size, prv_buffer_idx);

	/* Check if we will cross block with new message */
	if (prv_buffer_idx + message_length + TIMESTAMP_SIZE + 1 > prv_left_block_size) {
		/* Write current buffer to flash and go to new block */
		LOG_DBG("Message to long for this block, write buffer and go to next block!");

		err = prv_write_buffer_to_flash();
		if (err) {
			LOG_ERR("Write buffer to flash failed with code: %d. Discard buffer and "
				"store new message!",
				err);
		} else {
			prv_go_to_next_block();
		}
		LOG_DBG("Flash start offset: %ld, left block size: %d buff idx: %d",
			prv_flash_offset, prv_left_block_size, prv_buffer_idx);
	}
	/* Check if message needs to be divided - write buffer to flash to avoid partial data loss
	in the case of reboot! */
	else if (prv_buffer_idx + message_length + TIMESTAMP_SIZE + 1 > FLASH_BUFFER_SIZE) {
		LOG_DBG("New message to long for buffer! Write buffer to flash!");
		err = prv_write_buffer_to_flash();
		if (err) {
			LOG_ERR("Write buffer to flash failed with code: %d. Discard buffer and "
				"store new message!",
				err);
		}
		LOG_DBG("Flash start offset: %ld, left block size: %d buff idx: %d",
			prv_flash_offset, prv_left_block_size, prv_buffer_idx);
	}

	/* Write port */
	err = prv_add_byte_to_buffer(port);
	if (err) {
		return err;
	}

	/* Write message */
	for (uint8_t i = 0; i < message_length; i++) {
		err = prv_add_byte_to_buffer(message[i]);
		if (err) {
			return err;
		}
	}

	/* Get timestamp */
	uint8_t timestamp[TIMESTAMP_SIZE];
	uint32_t_to_bytes(timestamp, get_global_unix_time());

	/* Write timestamp to buffer */
	for (uint8_t i = 0; i < TIMESTAMP_SIZE; i++) {
		err = prv_add_byte_to_buffer(timestamp[i]);
		if (err) {
			return err;
		}
	}

	/* Increase msg nr. */
	prv_n_msg_buffer++;

	LOG_DBG("Msg nr in flash: %d msg nr. in buf: %d flash end offset: %ld, block start offset: "
		"%ld left block size: %d buff idx: %d",
		Main_values.flash_nr_msg->def_val, prv_n_msg_buffer, prv_flash_offset,
		prv_block_offset, prv_left_block_size, prv_buffer_idx);
	return err;
}

/**
 * @brief Read all messages from defined port.
 *
 * @param uint8_t read_port - port to read from
 * @param uint8_t sender - of the message, i.e LR, BT
 *
 * @return uint32_t number of messages read
 */
static int prv_read_all_messages(uint8_t read_port, mb_msg_dest sender, uint8_t *max_rsp_len)
{
	/* Empty buffer to flash */
	prv_write_buffer_to_flash();

	if (sender == MB_MSG_LP0) {
		LOG_WRN("Wait for LP0 to go into data transfer mode");
		k_sem_take(&flash_offload_start_lp0, K_FOREVER);
	}

	uint32_t n_msg = 0;
	off_t _block_offset = prv_block_offset; /* Set offset of block to read to latest block */
	bool read = true;

	/* If sending data to LR, lock other functionality! */
	set_com_thread_operation(THREAD_DISABLED);
	uint32_t old_sleep = thread_sleep;
	thread_sleep = THREAD_CONNECTED_SLEEP;

	while (read) {
		/* Report to watchdog to avoid timeout */
		flash_thread_report();

		/* Read block */
		LOG_INF("Read block with offset: %ld on port: %d and send to: %d", _block_offset,
			read_port, sender);
		int rs = prv_read_block(_block_offset, read_port, sender, max_rsp_len);
		if (rs < 0) {
			LOG_ERR("Error reading flash log. Terminate operation! (%d)", rs);
			break;
		}
		n_msg += rs;

		/* Go to prev block */
		if (_block_offset != prv_flash_start_offset) {
			_block_offset = prv_get_block_prev_offset(_block_offset);
		} else {
			read = false;
		}
	}

	if (sender == MB_MSG_LP0) {
		k_sem_give(&flash_offload_done_lp0);
		LOG_WRN("Flash offload complete semaphore given");
	}

	/* Unlock LR functionality since we are done sending. */
	set_com_thread_operation(THREAD_NORMAL);
	thread_sleep = old_sleep;

	return n_msg;
}

/**
 * @brief Count the number of all messages from port.
 *
 * If port is set to 0, all messages are read.
 *
 * @param uint8_t read_port - port to read from
 *
 * @return uint32_t number of messages read
 */
static uint32_t prv_count_all_messages(uint8_t read_port, mb_msg_dest sender, uint8_t *max_rsp_len)
{
	/* Empty buffer to flash */
	prv_write_buffer_to_flash();

	uint32_t n_msg = 0;
	off_t _block_offset = prv_block_offset; /* Set offset of block to read to latest block */
	bool read = true;

	while (read) {
		/* Read block */
		LOG_INF("Read block with offset: %ld on port: %d", _block_offset, read_port);
		int rs = prv_read_block(_block_offset, read_port, sender, max_rsp_len);
		if (rs < 0) {
			LOG_ERR("Error reading flash log. Terminate operation!");
			break;
		}
		n_msg += rs;

		/* Go to prev block */
		if (_block_offset != prv_flash_start_offset) {
			_block_offset = prv_get_block_prev_offset(_block_offset);
		} else {
			read = false;
		}
	}

	return n_msg;
}

/**
 * @brief Read messages from flash.
 *
 * Used specifically for continuing flash read commands originating from
 * CMD_FLASH_GET_FROM_HEAD or CMD_FLASH_GET_ALL, after outgoing
 * message queue clears.
 *
 * Read flash messages and pass them to the @p sender thread.
 *
 * @param [inout] start - start message (from flash head)
 * @param [in] n_msg_read - nr of msg to read (if set to 0, all messages are read)
 * @param [in] read_port - port to read from
 * @param [in] sender - of the message, i.e LR, BT
 * @param [inout] max_rsp_len - max payload size of the response
 *
 * @return int number of messages read, negative error code on failure.
 * @retval -EBUSY if there is no space in outgoing sender queue.
 * @retval -EIO if flash is not active.
 */
static int prv_read_messages_from_head(uint32_t start, uint32_t n_msg_read, uint8_t read_port,
				       mb_msg_dest sender, uint8_t *max_rsp_len)
{
	if (!prv_flash_enable) {
		LOG_ERR("Flash not active!");
		return -EIO;
	}

	/* Empty buffer to flash */
	prv_write_buffer_to_flash();

	if (sender == MB_MSG_LORA) {
		extern struct k_msgq event_que;
		/* If outgoing event_que is still full or almost full, add event again into flash
		 * message queue. We leave two outgoing queue spaces for other board processes. */
		if ((k_msgq_num_used_get(&event_que) >= event_que.max_msgs - 2)) {
			LOG_WRN("Outgoing message queue is full, stop reading!");
			return -EBUSY;
		}
	} else if (sender == MB_MSG_SAT) {
#ifdef CONFIG_SATELLITE
		extern struct k_msgq satellite_que;
		/* If outgoing satellite_que is still full or almost full, add event again into
		 * flash message queue. We leave two outgoing queue spaces for other board
		 * processes.
		 */
		if ((k_msgq_num_used_get(&satellite_que) >= satellite_que.max_msgs - 2)) {
			LOG_WRN("Outgoing message queue is full, stop reading!");
			return -EBUSY;
		}
#endif
	}

	off_t read_block_off = prv_block_offset; /* Set read block offset to last block */
	int n_msg_block = 0;                     /* Number of messages in last block */

	uint32_t block_start_read = 0; /* First message to read from start of block */
	uint32_t block_end_read = 0;   /* Last message to read from start of block */

	/* Check if we have at least start msgs in flash */
	if (start > Main_values.flash_nr_msg->def_val) {
		LOG_ERR("Number of stored messages is :%d cannot start read from message "
			"-%d",
			Main_values.flash_nr_msg->def_val, start);
		start = Main_values.flash_nr_msg->def_val;
	}
	if (n_msg_read > Main_values.flash_nr_msg->def_val) {
		n_msg_read = start;
	}

	/* Message counter */
	uint32_t msg_to_read_count = n_msg_read;

	LOG_DBG("We have to read %d messages, starting from: %d", n_msg_read, start);

	/* Determine first block to read from */
	uint32_t skip_msg = 0; /* Number of messages to skip before start reading from head */
	uint8_t read = 0;
	while (!read) {
		n_msg_block = prv_get_nr_msg_in_block(
			read_block_off, read_port); /* Number of messages in last block */
		if (n_msg_block < 0) {
			LOG_ERR("Error when reading nr. of messages in block!");
			return n_msg_block;
		}
		LOG_DBG("N msg in current block with offset %ld : %d", read_block_off, n_msg_block);

		/* Check if we can start read in this block */
		if (skip_msg + n_msg_block > start) {
			LOG_DBG("Start read in this block with offset: %ld", read_block_off);
			block_end_read =
				n_msg_block -
				(start - skip_msg); /* Determine last msg to read from block */
			read = 1;
		}
		/* If not go to next block */
		else {
			skip_msg += n_msg_block; /* Skip block */
			read_block_off =
				prv_get_block_prev_offset(read_block_off); /* Go one block back */
			/* Check if we came to the relative start of the flash, return error */
			if (read_block_off < 0) {
				LOG_ERR("We went past flash start! Note enough messages of "
					"this "
					"type are written in flash: %d, to skip first: %d "
					"messages!",
					skip_msg, start);
				return read_block_off;
			}
			LOG_DBG("Skip this block, check block with offset: %ld", read_block_off);
		}

		/* Report to watchdog to avoid timeout */
		flash_thread_report();
	}

	/* If sending data to LR, lock other functionality! */
	set_com_thread_operation(THREAD_DISABLED);
	uint32_t old_sleep = thread_sleep;
	thread_sleep = THREAD_CONNECTED_SLEEP;

	/* Read messages */
	while (msg_to_read_count > 0) {
		/* Check if there are any messages to read in new block */
		if (n_msg_block > 0) {
			/* Check if should adjust start spot in block, i.e. we don't want to
			read all messages in the block */
			if (msg_to_read_count < block_end_read) {
				block_start_read = block_end_read - msg_to_read_count + 1;
			}
			/* if not, read from start */
			else {
				block_start_read = 1;
			}

			LOG_DBG("Read messages from: %d to %d out of %d messages in block "
				"with "
				"offset: %ld",
				block_start_read, block_end_read, n_msg_block, read_block_off);

			bool busy_out = false;
			int rs = prv_read_block_messages(read_block_off, block_start_read,
							 block_end_read, read_port, sender,
							 max_rsp_len, &busy_out);
			if (rs < 0) {
				LOG_ERR("Error reading messages from block with offset: "
					"%ld",
					read_block_off);
				break;
			}
			msg_to_read_count -= rs;
			LOG_DBG("Read %d messages, %d left to read!", rs, msg_to_read_count);

			if (busy_out) {
				LOG_WRN("Outgoing message queue is full, stop reading!");
				break;
			}
		}

		/* Go to next block */
		if (msg_to_read_count > 0) {
			read_block_off =
				prv_get_block_prev_offset(read_block_off); /* Go one block back */
			if (read_block_off < 0) {
				LOG_ERR("Came to end of the flash, terminate read "
					"procedure!");
				break;
			}
			/* Read nr. of messages in block */
			n_msg_block = prv_get_nr_msg_in_block(
				read_block_off, read_port); /* Number of messages in last block */
			if (n_msg_block < 0) {
				LOG_ERR("Error when reading nr. of messages in block!");
				break;
			}
			block_end_read = n_msg_block;
			LOG_DBG("Go to new block with offset: %ld and number of messages: "
				"%d",
				read_block_off, n_msg_block);
		}
	}
	/* Enable other operations */
	set_com_thread_operation(THREAD_NORMAL);
	thread_sleep = old_sleep;

	return (n_msg_read - msg_to_read_count);
}

/**
 * @brief Parse incoming messages.
 *
 * @param message - pointer to message
 * @param message_length - message length
 * @param message_origin - message origin
 * @param message_action - message action
 * @param port - port
 * @param max_rsp_len - max rsp len
 *
 * @return negative integer error code or 0 if ok.
 */
static int prv_flash_parse_message(uint8_t *message, uint8_t message_length,
				   mb_msg_dest message_origin, mb_msg_action message_action,
				   uint8_t port, uint8_t *max_rsp_len)
{
	int err = 0;

	/* Command */
	if (message_action == MB_MSG_EXECUTE) {
		uint8_t id, len;
		id = message[0];
		len = message[1];
		switch (id) {
		case CMD_FLASH_CLEAR: {
			LOG_INF("Clear flash data");
			err = clear_flash_data();
			len = compose_response_msg(message, err, &port);
			return thread_put_message(MB_MSG_FLASH, message_origin, MB_MSG_SEND, port,
						  message, len, 0);
		}
		case CMD_FLASH_GET_FROM_HEAD: {
			if (len != 12) {
				err = -EIO;
				len = compose_response_msg(message, err, &port);
				return thread_put_message(MB_MSG_FLASH, message_origin, MB_MSG_SEND,
							  port, message, len, 0);
			} else {
				bt_con_ignore_disconnect_period_set(true); /* disable bluetooth
							  connection timeout while streaming */
				uint8_t _port = (uint8_t)(bytes_to_uint32_t(
					message + FLASH_READ_PORT_OFFSET));
				/* All messages to be read */
				uint32_t _nr_mes = bytes_to_uint32_t(
					message + FLASH_READ_NUM_OF_MESSAGES_TO_READ_OFFSET);

				uint32_t _start =
					bytes_to_uint32_t(message + FLASH_READ_START_OFFSET);

				LOG_INF("Read %d msgs, starting from: %d on port: %d", _nr_mes,
					_start, _port);
				err = prv_read_messages_from_head(_start, _nr_mes, _port,
								  message_origin, max_rsp_len);

				/* Send next batch */
				if (err > 0 && err < _nr_mes) {
					len = FLASH_READ_CONTINUE_MSG_LEN;
					message[0] = id;
					message[1] = len;
					uint32_t_to_bytes(&message[FLASH_READ_PORT_OFFSET], _port);
					uint32_t_to_bytes(&message[FLASH_READ_START_OFFSET],
							  _start - err);
					uint32_t_to_bytes(
						&message[FLASH_READ_NUM_OF_MESSAGES_TO_READ_OFFSET],
						_nr_mes - err);

					thread_put_message(message_origin, MB_MSG_FLASH,
							   MB_MSG_EXECUTE_FLASH_READ_CONTINUE, port,
							   message, len, *max_rsp_len);
					return 0;

				} else if (err == -EBUSY) {
					/* If unable to add messages to outgoing queue, add event
					 * back to flash queue */

					LOG_DBG("Outgoing event_que is too full, adding event back "
						"into "
						"flash "
						"message queue");
					len = FLASH_READ_CONTINUE_MSG_LEN;
					message[0] = id;
					message[1] = len;
					uint32_t_to_bytes(&message[FLASH_READ_PORT_OFFSET],
							  (uint32_t)_port);
					uint32_t_to_bytes(&message[FLASH_READ_START_OFFSET],
							  _start);
					uint32_t_to_bytes(
						&message[FLASH_READ_NUM_OF_MESSAGES_TO_READ_OFFSET],
						_nr_mes);

					thread_put_message(message_origin, MB_MSG_FLASH,
							   MB_MSG_EXECUTE_FLASH_READ_CONTINUE, port,
							   message, len, *max_rsp_len);
					return 0;
				}
			}
			message[0] = id;
			len = compose_response_msg(message, err, &port);
			return thread_put_message(MB_MSG_FLASH, message_origin, MB_MSG_SEND, port,
						  message, len, 0);
		}
		case CMD_FLASH_GET_ALL: {
			if (len != 1) {
				err = -EIO;
				len = compose_response_msg(message, err, &port);
				return thread_put_message(MB_MSG_FLASH, message_origin, MB_MSG_SEND,
							  port, message, len, 0);
			} else {
				uint8_t _port = message[FLASH_READ_PORT_OFFSET];
				if (message_origin == MB_MSG_LORA || message_origin == MB_MSG_SAT) {
					/* If message originates from LoRa or Satellite we need to
					 * limit the transmission speed to avoid dropping messages
					 * due to outgoing message que being too full.
					 *
					 * We do that by reading messages in batches and adding the
					 * event back to the flash queue if there is no space in
					 * the outgoing queue
					 */

					uint32_t num_of_msgs =
						prv_count_all_messages(_port, message_origin,
								       max_rsp_len) -
						1;

					LOG_INF("Read all messages on port: %d, Number of all "
						"messages: %d",
						_port, num_of_msgs);

					len = FLASH_READ_CONTINUE_MSG_LEN;
					message[0] = id;
					message[1] = len;
					uint32_t_to_bytes(&message[FLASH_READ_PORT_OFFSET], _port);
					uint32_t_to_bytes(&message[FLASH_READ_START_OFFSET],
							  num_of_msgs);
					uint32_t_to_bytes(
						&message[FLASH_READ_NUM_OF_MESSAGES_TO_READ_OFFSET],
						num_of_msgs);
					thread_put_message(message_origin, MB_MSG_FLASH,
							   MB_MSG_EXECUTE_FLASH_READ_CONTINUE, port,
							   message, len, *max_rsp_len);
					return 0;
				} else if (message_origin == MB_MSG_BT ||
					   message_origin == MB_MSG_LP0) {
					LOG_INF("Read all messages on port: %d", _port);
					if (message_origin == MB_MSG_LP0) {
						_port = 0;
					} else {
						/* If message originates from Bluetooth we don't
						 * need to limit the transmission speed */
						bt_con_ignore_disconnect_period_set(
							true); /* disable bluetooth connection
								  timeout while streaming */
					}
					err = prv_read_all_messages(_port, message_origin,
								    max_rsp_len);
				}
				message[0] = id;
				len = compose_response_msg(message, err, &port);
				return thread_put_message(MB_MSG_FLASH, message_origin, MB_MSG_SEND,
							  port, message, len, 0);
			}
		}
		case CMD_DISABLE_FLASH_TH: {
			disable_flash();
			break;
		}
		case CMD_GET_FLASH_STATUS: {
			/* Populate payload message */
			len = prv_compose_flash_status_msg();
			/* Set port */
			port = Main_messages.msg_flash_status->port;
			/* Send message */
			return thread_put_message(MB_MSG_FLASH, message_origin, MB_MSG_SEND, port,
						  prv_flash_payload, len, 0);
			break;
		}
		}
	}
	/* Store message */
	else if (message_action == MB_MSG_STORE) {
		extern struct k_sem lp0_flash_save_sem;
		k_sem_give(&lp0_flash_save_sem); /* Release flash semaphore to allow flash write */
		LOG_INF("Store flash message of length: %d", message_length);
		err = prv_flash_store_message(message, message_length, port);
	}

	/* Continue reading from flash */
	else if (message_action == MB_MSG_EXECUTE_FLASH_READ_CONTINUE) {
		/* Used specifically for continuing flash read originating from MB_MSG_LORA and
		 * MB_MSG_BT, from CMD_FLASH_GET_FROM_HEAD or CMD_FLASH_GET_ALL commands
		 *
		 * DEV-NOTE: Command structure
		 * [command ID as 1 byte] 0C [port_nr encoded as 4 bytes array] [start msg  nr.
		 * (from head) - 4 bytes] [nr. of messages - 4 bytes] [nr- of messages already read
		 * - 4 bytes]
		 */

		uint8_t id, len;
		id = message[0];
		len = message[1];

		if (len != FLASH_READ_CONTINUE_MSG_LEN) { /* Error guard */
			err = -EIO;
			len = compose_response_msg(message, err, &port);
			LOG_ERR("Invalid message length for CMD_FLASH_GET_CONTINUE: %d", len);
			return thread_put_message(MB_MSG_FLASH, message_origin, MB_MSG_SEND, port,
						  message, len, 0);
		}

		bt_con_ignore_disconnect_period_set(true); /* disable bluetooth connection
								 timeout while streaming */

		uint8_t _port = (uint8_t)(bytes_to_uint32_t(message + FLASH_READ_PORT_OFFSET));
		uint32_t _start = bytes_to_uint32_t(message + FLASH_READ_START_OFFSET);
		uint32_t _nr_mes =
			bytes_to_uint32_t(message + FLASH_READ_NUM_OF_MESSAGES_TO_READ_OFFSET);

		LOG_INF("Read %d msgs, starting from: %d on port: %d", _nr_mes, _start, _port);

		uint32_t _nr_mes_per_batch = FLASH_READ_NUM_OF_MESSAGES_PER_BATCH; /* Number of
								 messages to read per batch */

		err = prv_read_messages_from_head(_start, _nr_mes_per_batch, _port, message_origin,
						  max_rsp_len);

		LOG_INF("Continue reading messages on port: %u, start: %u, _nr_mes: %u/%u", _port,
			_start, _nr_mes_per_batch, _nr_mes);

		if (_nr_mes > Main_values.flash_nr_msg->def_val) {
			_nr_mes = Main_values.flash_nr_msg->def_val - _start;
		}

		/* If there are still messages to read, put event with updated values back
		 * into queue */
		if (err > 0 && err < _nr_mes) {

			len = FLASH_READ_CONTINUE_MSG_LEN;
			message[0] = id;
			message[1] = len;
			uint32_t_to_bytes(&message[FLASH_READ_PORT_OFFSET], (uint32_t)_port);
			uint32_t_to_bytes(&message[FLASH_READ_START_OFFSET], _start - err);
			uint32_t_to_bytes(&message[FLASH_READ_NUM_OF_MESSAGES_TO_READ_OFFSET],
					  _nr_mes - err);

			thread_put_message(message_origin, MB_MSG_FLASH,
					   MB_MSG_EXECUTE_FLASH_READ_CONTINUE, port, message, len,
					   *max_rsp_len);
			return 0;
		} else if (err > (int)_nr_mes_per_batch) {
			LOG_ERR("Something went wrong when reading messages from flash! "
				"Read more "
				"messages than expected!");

		} else if (err == -EBUSY) {
			/* If unable to add messages to outgoing queue, add event back to
			 * flash queue */

			LOG_DBG("Outgoing event_que is too full, adding event back into "
				"flash "
				"message queue");
			len = FLASH_READ_CONTINUE_MSG_LEN;
			message[0] = id;
			message[1] = len;
			uint32_t_to_bytes(&message[FLASH_READ_PORT_OFFSET], (uint32_t)_port);
			uint32_t_to_bytes(&message[FLASH_READ_START_OFFSET], _start);
			uint32_t_to_bytes(&message[FLASH_READ_NUM_OF_MESSAGES_TO_READ_OFFSET],
					  _nr_mes);

			thread_put_message(message_origin, MB_MSG_FLASH,
					   MB_MSG_EXECUTE_FLASH_READ_CONTINUE, port, message, len,
					   *max_rsp_len);
			return 0;
		}

		/* Confirm message - when all messages were read */
		message[0] = id;
		len = compose_response_msg(message, err, &port);
		return thread_put_message(MB_MSG_FLASH, message_origin, MB_MSG_SEND, port, message,
					  len, 0);
	}

	/* Else */
	else {
		err = -EIO;
		LOG_ERR("Message action not supported!");
	}
	return err;
}

/**
 * @brief Check if we need to send flash status message.
 *
 * @return true
 * @return false
 */
static bool prv_flash_check_send_status_interval(void)
{
	if (Main_settings.flash_status_interval->def_val > 0) {
		if ((uint32_t)((k_uptime_get() - prv_last_flash_status_send) / 1000) >
		    Main_settings.flash_status_interval->def_val) {
			LOG_INF("Send flash status message.");
			prv_last_flash_status_send = k_uptime_get();
			return true;
		}
	}
	return false;
}

int init_flash(void)
{
	int err = 0;

	/* Flash init */
	if (!flash_dev) {
		LOG_WRN("DT binding for flash not provided!");
		return -ENODEV;
	}

	if (!device_is_ready(flash_dev)) {
		LOG_ERR("Flash device %s is not ready", flash_dev->name);
		return -ENODEV;
	} else {
		prv_flash_enable = true;
	}

	LOG_INF("Flash min write size: %d", flash_get_write_block_size(flash_dev));
	LOG_INF("Nr. of pages in flash: %d", flash_get_page_count(flash_dev));

#if IS_ENABLED(CONFIG_PM_DEVICE)
	pm_device_action_run(flash_dev, PM_DEVICE_ACTION_SUSPEND);
#endif /* IS_ENABLED(CONFIG_PM_DEVICE) */

	size_t flash_size = 0;
	err = flash_ext_get_partition_size(FLASH_EXT_PARTITION_MESSAGE_STORAGE, &flash_size);
	if (err) {
		LOG_ERR("Failed to get flash size: %d", err);
		prv_flash_enable = false;
		return err;
	}

	/* Read current flash offset from nvs */
	if (nvs_storage_read(STORAGE_flash_offset, &prv_flash_offset, sizeof(prv_flash_offset)) !=
	    sizeof(prv_flash_offset)) {
		prv_flash_offset = 0;
		nvs_storage_write(STORAGE_flash_offset, &prv_flash_offset,
				  sizeof(prv_flash_offset));
		LOG_INF("Flash offset not set yet, store: %ld", prv_flash_offset);
	} else {
		LOG_INF("Flash offset read from storage: %ld total flash size: %d",
			prv_flash_offset, flash_size);
	}

	/* Read start flash offset from nvs */
	if (nvs_storage_read(STORAGE_flash_start_offset, &prv_flash_start_offset,
			     sizeof(prv_flash_start_offset)) != sizeof(prv_flash_offset)) {
		prv_flash_offset = 0;
		nvs_storage_write(STORAGE_flash_start_offset, &prv_flash_start_offset,
				  sizeof(prv_flash_start_offset));
		LOG_INF("Flash start offset not set yet, store: %ld", prv_flash_start_offset);
	} else {
		LOG_INF("Flash start offset read from storage: %ld", prv_flash_start_offset);
	}

	/* If offset is 0, delete first block */
	if (prv_flash_offset == 0) {
		err = prv_erase_from_flash(prv_flash_offset, FLASH_BLOCK_SIZE);
		if (err) {
			LOG_ERR("Flash not operating as expected!");
			prv_flash_enable = false;
		}
	}

	/* Get left block size */
	if (nvs_storage_read(STORAGE_flash_block_left, &prv_left_block_size,
			     sizeof(prv_left_block_size)) != sizeof(prv_left_block_size)) {
		prv_left_block_size = FLASH_BLOCK_SIZE;
		nvs_storage_write(STORAGE_flash_block_left, &prv_left_block_size,
				  sizeof(prv_left_block_size));
		LOG_INF("Flash left block size not set yet, store: %d", prv_left_block_size);
	} else {
		LOG_INF("Flash left block size read from storage: %d", prv_left_block_size);
	}

	/* Get last block offset */
	if (nvs_storage_read(STORAGE_flash_block_offset, &prv_block_offset,
			     sizeof(prv_block_offset)) != sizeof(prv_block_offset)) {
		prv_block_offset = 0;
		nvs_storage_write(STORAGE_flash_block_offset, &prv_block_offset,
				  sizeof(prv_block_offset));
		LOG_INF("Flash last block offset not yet stored in nvs, store: %ld",
			prv_block_offset);
	} else {
		LOG_INF("Flash last block offset read from storage: %ld", prv_block_offset);
	}

	/* Solve previous error with flash size */
	if (prv_flash_offset >= flash_size) {
		prv_go_to_next_block();
	}

	/* Set local buffer index */
	prv_buffer_idx = 0;
	prv_n_msg_buffer = 0;
	/* Init buffer */
	memset(prv_flash_buffer, 0, sizeof(prv_flash_buffer));

	/* Get written nr. of messages */
	if (nvs_storage_read(STORAGE_flash_n_messages, &Main_values.flash_nr_msg->def_val,
			     sizeof(Main_values.flash_nr_msg->def_val)) !=
	    sizeof(Main_values.flash_nr_msg->def_val)) {
		Main_values.flash_nr_msg->def_val = 0;
		nvs_storage_write(STORAGE_flash_n_messages, &Main_values.flash_nr_msg->def_val,
				  sizeof(Main_values.flash_nr_msg->def_val));
		LOG_INF("Flash nr. of messages not yet stored in nvs, store: %d",
			Main_values.flash_nr_msg->def_val);
	} else {
		LOG_INF("Flash nr. of messages read from storage: %d",
			Main_values.flash_nr_msg->def_val);
	}

	/* Test write and read reboot msg */
	err = test_flash();
	if (err) {
		prv_go_to_next_block();
		err = test_flash();
		if (err) {
			LOG_ERR("Test write read failed! disable flash!");
			prv_flash_enable = false;
			return err;
		}
	}
	LOG_INF("Test write read successful!");

	return err;
}

int test_flash(void)
{
	/* Generate sample flash reboot message */
	uint8_t tmp_msg[4];
	tmp_msg[0] = Main_messages.msg_cmd_confirm->id;
	tmp_msg[1] = Main_messages.msg_cmd_confirm->length;
	tmp_msg[2] = 0; /* Indicate reboot */
	tmp_msg[3] = Main_values.reset_reason->def_val & 0x0000000F;
	uint8_t tmp_len = Main_messages.msg_cmd_confirm->length + MESSAGE_HEAD_LEN;
	uint8_t tmp_port = Main_messages.msg_cmd_confirm->port;

	/* Add to write buffer */
	int err = prv_flash_store_message(tmp_msg, tmp_len, tmp_port);
	if (err) {
		LOG_ERR("Failed to write test message!");
		return err;
	}
	/* Write buffer to flash */
	err = prv_write_buffer_to_flash();
	if (err) {
		LOG_ERR("Failed to write test buffer message!");
		return err;
	}

	/* Read back last message */
	uint8_t read_len = tmp_len + 1 + TIMESTAMP_SIZE;
	off_t tmp_offset = prv_flash_offset - read_len;
	uint8_t tmp[read_len];
	/* Read port */
	if (prv_read_from_flash(tmp_offset, tmp, read_len)) {
		return -EIO;
	}
	if (tmp[0] != tmp_port) {
		LOG_ERR("Invalid port: %d, should be: %d", tmp[0], tmp_port);
		return -EIO;
	}
	LOG_INF("Test msg port: %d, should be: %d", tmp[0], tmp_port);

	for (uint8_t i = 0; i < tmp_len; i++) {
		if (tmp[i + 1] != tmp_msg[i]) {
			LOG_ERR("Read: %d, should be: %d on idx: %d", tmp[i + 1], tmp_msg[i], i);
			return -EIO;
		}
		LOG_INF("Read: %d, should be: %d on idx: %d", tmp[i + 1], tmp_msg[i], i);
	}

	return 0;
}

void disable_flash(void)
{
	LOG_INF("Disable flash");
	prv_flash_enable = false;
	LOG_INF("Disable flash done!");
}

bool get_flash_status(void)
{
	return prv_flash_enable;
}

int clear_flash_data(void)
{
	if (!prv_flash_enable) {
		LOG_ERR("Flash not active!");
		return -EIO;
	}
	int err = 0;

	/* Reset all variables */
	prv_flash_offset = 0;
	nvs_storage_write(STORAGE_flash_offset, &prv_flash_offset, sizeof(prv_flash_offset));
	LOG_DBG("Flash offset reset to: %ld, block erased!", prv_flash_offset);

	prv_flash_start_offset = 0;
	nvs_storage_write(STORAGE_flash_start_offset, &prv_flash_start_offset,
			  sizeof(prv_flash_start_offset));
	LOG_DBG("Start flash offset reset to: %ld", prv_flash_start_offset);

	prv_left_block_size = FLASH_BLOCK_SIZE;
	nvs_storage_write(STORAGE_flash_block_left, &prv_left_block_size,
			  sizeof(prv_left_block_size));
	LOG_DBG("Left block size reset to: %d", prv_left_block_size);

	prv_block_offset = 0;
	nvs_storage_write(STORAGE_flash_block_offset, &prv_block_offset, sizeof(prv_block_offset));
	LOG_DBG("Block offset reset to: %ld", prv_block_offset);

	Main_values.flash_nr_msg->def_val = 0;
	nvs_storage_write(STORAGE_flash_n_messages, &Main_values.flash_nr_msg->def_val,
			  sizeof(Main_values.flash_nr_msg->def_val));
	LOG_DBG("Nr. of messages reset to: %d", Main_values.flash_nr_msg->def_val);

	err = prv_erase_from_flash(prv_flash_offset, FLASH_BLOCK_SIZE);

	/* Set local buffer index */
	prv_buffer_idx = 0;
	prv_n_msg_buffer = 0;
	/* Init buffer */
	memset(prv_flash_buffer, 0, sizeof(prv_flash_buffer));

	return err;
}

int handle_flash_thread_messages(void)
{
	int err = 0;
	/* Check if new message */
	mb_msg_dest msg_origin = 0;
	mb_msg_action msg_action = 0;
	uint8_t port = 0;
	uint8_t msg_max_rsp_len = 0;
	int msg_size = thread_get_flash(&msg_origin, &msg_action, &port, prv_flash_payload,
					&msg_max_rsp_len);
	if (msg_size > 0) {
		LOG_INF("Got message in flash thread from: %d, length: %d port: %d!", msg_origin,
			msg_size, port);
		LOG_HEXDUMP_INF(prv_flash_payload, msg_size,
				"Flash message-------------------------------------------:");
		err = prv_flash_parse_message(prv_flash_payload, (uint8_t)(msg_size), msg_origin,
					      msg_action, port, &msg_max_rsp_len);
	}

	return err;
}

int handle_flash_status()
{
	int err = 0;
	/* Check if status send interval has passed and send status */
	if (prv_flash_check_send_status_interval()) {
		/* Populate payload message */
		uint8_t len = prv_compose_flash_status_msg();
		/* Set port */
		uint8_t port = Main_messages.msg_flash_status->port;

		/* Check if message needs to be stored to flash */
		if (check_flash_store_flag(port)) {
			prv_flash_store_message(prv_flash_payload, len, port);
		}
		/* Check if message needs to be sent to LR */
		if (check_lr_send_flag(port)) {
			thread_put_message(MB_MSG_FLASH, MB_MSG_LORA, MB_MSG_SEND, port,
					   prv_flash_payload, len, 0);
		}
		/* Check if message needs to be sent via sat */
		if (check_sat_send_flag(port)) {
			thread_put_message(MB_MSG_FLASH, MB_MSG_SAT, MB_MSG_SEND, port,
					   prv_flash_payload, len, 0);
		}
		if (check_lp0_send_flag(port)) {
			/* Add event to LP0 que */
			lp0_add_message_to_send_queue(prv_flash_payload, len, port, false, false);
		}
	}

	return err;
}
