/** @file satellite.c
 *
 * @brief Interface for satellite communication.
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2022 Irnas. All rights reserved.
 */

#include "satellite.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>

#include "global_time.h"
#include "status.h"
#include "thread_com.h"
#include "thread_operation.h"
#include "uart_pm.h"

LOG_MODULE_REGISTER(satellite);

uint8_t sat_data[MAX_BUF_SIZE]; // Buffer for handling mailbox messages

#define SAT_UART          DT_ALIAS(rb_uart)
#define SAT_UART_BUF_SIZE 340

// Satellite module enabled flag
bool prv_rockblock_detected = false;

#if DT_NODE_EXISTS(SAT_UART)
const struct device *sat_uart_dev = DEVICE_DT_GET(SAT_UART);
#else
const struct device *sat_uart_dev;
#endif // DT_NODE_EXISTS(SAT_UART)
uint8_t sat_buf[SAT_UART_BUF_SIZE];
int sat_buf_idx = 0;
const struct uart_config sat_uart_cfg = {
	.baudrate = 19200,
	.parity = UART_CFG_PARITY_NONE,
	.stop_bits = UART_CFG_STOP_BITS_1,
	.data_bits = UART_CFG_DATA_BITS_8,
	.flow_ctrl = UART_CFG_FLOW_CTRL_NONE,
};

typedef struct {
	void *fifo_reserved;
	uint8_t data[SAT_UART_BUF_SIZE];
	uint16_t len;
} sat_uart_data_t;

// FIFO for storing incoming messages
static K_FIFO_DEFINE(sat_fifo_uart_rx_data);

#define SAT_EN_GPIO_NODE DT_NODELABEL(rb_en)
#if DT_NODE_EXISTS(SAT_EN_GPIO_NODE)
const struct gpio_dt_spec sat_en_gpio = GPIO_DT_SPEC_GET(SAT_EN_GPIO_NODE, gpios);
#endif

#define SATELLITE_SEND_TIMEOUT           10 // In seconds
#define SATELLITE_VALIDATE_TIMEOUT       5  // In seconds
#define SATELLITE_SESSION_RETRY_ATTEMPTS 10
#define SATELLITE_CARRIAGE_RETURN_HEX    0x0D
#define SATELLITE_COMMA_HEX              0x2C
#define SATELLITE_SPACE_HEX              0x0D

/* AT COMMANDS */
#define SATELLITE_AT_ENABLE_ECHO         "ATE1\r"
#define SATELLITE_AT_DISABLE_FLOW_CTRL   "AT&K0\r"
#define SATELLITE_AT_DISABLE_RING_ALERTS "AT+SBDMTA=0\r"
#define SATELLITE_AT_PING                "AT\r"
#define SATELLITE_AT_REQ_SIGNAL_STRENGTH "AT+CSQ\r"
#define SATELLITE_AT_RSP_SIGNAL_STRENGTH "+CSQ:"
#define SATELLITE_AT_REQ_NETWORK_TIME    "AT-MSSTM\r"
#define SATELLITE_AT_RSP_NETWORK_TIME    "-MSSTM:"
#define SATELLITE_AT_EXTEND_SBD_SESSION  "AT+SBDIX\r"
#define SATELLITE_AT_RSP_SBD_SESSION     "+SBDIX:"
#define SATELLITE_AT_CLEAR_MO_BUFFER     "AT+SBDD0\r"
#define SATELLITE_AT_QUEUE_BYTES_MSG     "AT+SBDWB="
#define SATELLITE_AT_RECEIVE_ASCII       "AT+SBDRT\r"
#define SATELLITE_AT_RSP_RECEIVE_ASCII   "+SBDRT"
// For some reason we cannot get rcv bytes command to work. Use Ascii
#define SATELLITE_AT_RECEIVE_BYTES       "AT+SBDRB\r"
#define SATELLITE_AT_RSP_RECEIVE_BYTES   "+SBDRB"

// Read bytes flag allows us to obtain message containing CR and NL characters
bool read_bytes = false;
typedef struct {
	uint8_t mo_status_code;
	int mo_message_number;
	uint8_t mt_status_code;
	int mt_message_number;
	int mt_length;
	int mt_queued;
	bool mo_success;
	bool mt_success;
} SBDStatus;

/** PRIVATE FUNCTIONS **/

/**
 * @brief UART RX callback
 *
 * @param dev serial device
 * @param user_data serial user data - not implemented
 */
static void satellite_uart_cb(const struct device *dev, void *user_data)
{
	static sat_uart_data_t *rx;
	ARG_UNUSED(user_data);

	uart_irq_update(dev);

	if (uart_irq_rx_ready(dev)) {
		int data_length;
		if (!rx) {
			rx = k_malloc(sizeof(*rx));
			if (rx) {
				rx->len = 0;
			} else {
				/* Disable UART interface, it will be
				 * enabled again after releasing the buffer.
				 */
				uart_irq_rx_disable(dev);

				LOG_ERR("Not able to allocate UART receive buffer\n");
				return;
			}
		}

		data_length = uart_fifo_read(dev, &rx->data[rx->len], SAT_UART_BUF_SIZE - rx->len);
		rx->len += data_length;

		if (rx->len > 0) {
			if ((rx->len == SAT_UART_BUF_SIZE) || (rx->data[rx->len - 1] == '\n') ||
			    (rx->data[rx->len - 1] == '\r')) {
				// If only new line is obtained, skip
				if (rx->len > 1 || read_bytes) {
					LOG_HEXDUMP_DBG(rx->data, rx->len, "RX: ");
					k_fifo_put(&sat_fifo_uart_rx_data, rx);
				} else {
					k_free(rx);
				}
				rx = NULL;
			}
		}
	}
}

/**
 * @brief Empty UART RX buffer.
 *
 */
static void satellite_empty_fifo(void)
{
	while (!k_fifo_is_empty(&sat_fifo_uart_rx_data)) {
		LOG_DBG("RX FIFO is not empty!");
		sat_uart_data_t *buf = k_fifo_get(&sat_fifo_uart_rx_data, K_NO_WAIT);
		k_free(buf);
		k_sleep(K_MSEC(100));
	}
}

/**
 * @brief Read next response line and validate OK response
 *
 * @return true
 * @return false
 */
static bool satellite_read_and_validate_rsp_ok(void)
{
	// Wait for OK
	bool validate = false;
	sat_uart_data_t *buf =
		k_fifo_get(&sat_fifo_uart_rx_data, K_SECONDS(SATELLITE_VALIDATE_TIMEOUT));
	if (buf) {
		if (buf->len >= sizeof("OK") - 1) {
			if (!strncmp(buf->data, "OK", sizeof("OK") - 1)) {
				validate = true;
			}
		}
		k_free(buf);
	}

	return validate;
}

/**
 * @brief Read next response line and validate command name echo
 *
 * @param cmd command char array
 * @param cmd_len command length
 * @return true
 * @return false
 */
static bool satellite_read_and_validate_echo(char *cmd, int cmd_len)
{
	// Wait for cmd name
	bool validate = false;
	sat_uart_data_t *buf =
		k_fifo_get(&sat_fifo_uart_rx_data, K_SECONDS(SATELLITE_VALIDATE_TIMEOUT));
	if (buf) {
		if (buf->len >= cmd_len) {
			if (!strncmp(buf->data, cmd, cmd_len)) {
				validate = true;
			}
		}
		k_free(buf);
	}
	return validate;
}

/**
 * @brief Write command to satellite module.
 *
 * @param cmd command char array
 * @param cmd_len command length
 * @return int - 0 on success or negative error code
 */
static int satellite_write_cmd(char *cmd, int cmd_len)
{
	int err = 0;
	if (sat_uart_dev) {
		for (int i = 0; i < cmd_len; i++) {
			uart_poll_out(sat_uart_dev, cmd[i]);
		}

		LOG_HEXDUMP_DBG(cmd, cmd_len, "TX: ");
	} else {
		LOG_ERR("UART device not initialized!");
		err = -ENXIO;
	}

	return err;
}

/**
 * @brief Write byte array to satellite module.
 *
 * @param msg msg byte array
 * @param msg_len msg length
 * @return int - 0 on success or negative error code
 */
static int satellite_write_bytes(uint8_t *msg, int msg_len)
{
	int err = 0;
	if (sat_uart_dev) {
		for (int i = 0; i < msg_len; i++) {
			uart_poll_out(sat_uart_dev, msg[i]);
		}

		LOG_HEXDUMP_DBG(msg, msg_len, "TX: ");
	} else {
		LOG_ERR("UART device not initialized!");
		err = -ENXIO;
	}

	return err;
}

/**
 * @brief Write command to satellite module and validate echo.
 *
 * @param cmd command char array
 * @param cmd_len command length
 * @return true
 * @return false
 */
static bool satellite_write_cmd_and_validate(char *cmd, int cmd_len)
{
	// Empty RX buffer
	satellite_empty_fifo();

	int rc = satellite_write_cmd(cmd, cmd_len);
	if (rc) {
		return false;
	}

	// Wait for cmd name
	return satellite_read_and_validate_echo(cmd, cmd_len);
}

/**
 * @brief Write command and validate OK response.
 *
 * @param cmd command char array
 * @param cmd_len command length
 * @return true
 * @return false
 */
static bool satellite_write_command_and_validate_ok(char *cmd, int cmd_len)
{

	bool validate = satellite_write_cmd_and_validate(cmd, cmd_len);
	if (!validate) {
		return validate;
	}

	// Wait for OK
	validate = satellite_read_and_validate_rsp_ok();

	return validate;
}

/**
 * @brief Return success or failure based on the mo status code. Display error message.
 *
 * @param mo_status_code
 * @return true
 * @return false
 */
static bool satellite_mo_status_message(uint8_t mo_status_code)
{
	if (mo_status_code >= 5 && mo_status_code <= 9) {
		LOG_INF("Reserved, but indicate MO session success if used.");
		return true;
	}
	if (mo_status_code >= 19 && mo_status_code <= 31) {
		LOG_INF("Reserved, but indicate MO session failure if used.");
		return false;
	}

	switch (mo_status_code) {
	case 0: {
		LOG_INF("MO message, if any, transferred successfully.");
		return true;
	}
	case 1: {
		LOG_INF("MO message, if any, transferred successfully, but the MT message in the "
			"queue was too big to be transferred.");
		return true;
	}
	case 2: {
		LOG_INF("MO message, if any, transferred successfully, but the requested Location "
			"Update was not accepted.");
		return true;
	}
	case 3:
	case 4: {
		LOG_INF("Reserved, but indicate MO session success if used.");
		return true;
	}
	case 10: {
		LOG_INF("Gateway reported that the call did not complete in the allowed time.");
		break;
	}
	case 11: {
		LOG_INF("MO message queue at the Gateway is full.");
		break;
	}
	case 12: {
		LOG_INF("MO message has too many segments.");
		break;
	}
	case 13: {
		LOG_INF("Gateway reported that the session did not complete.");
		break;
	}
	case 14: {
		LOG_INF("Invalid segment size.");
		break;
	}
	case 15: {
		LOG_INF("Access is denied.");
		break;
	}
	case 16: {
		LOG_INF("Transceiver has been locked and may not make SBD calls (see +CULK "
			"command).");
		break;
	}
	case 17: {
		LOG_INF("Gateway not responding (local session timeout).");
		break;
	}
	case 18: {
		LOG_INF("Connection lost (RF drop).");
		break;
	}
	case 32: {
		LOG_INF("No network service, unable to initiate call.");
		break;
	}
	case 33: {
		LOG_INF("No network service, unable to initiate call.");
		break;
	}
	case 34: {
		LOG_INF("Radio is disabled, unable to initiate call (see *Rn command).");
		break;
	}
	case 35: {
		LOG_INF("Transceiver is busy, unable to initiate call (typically performing "
			"auto-registration).");
		break;
	}
	case 36: {
		LOG_INF("Reserved, but indicate failure if used.");
		break;
	}
	default: {
		LOG_INF("Unknown code {self.mo_status_code}.");
		break;
	}
	}

	return false;
}

/**
 * @brief Return success or failure based on the mt status code. Displaying error message not yet
 * implemented.
 *
 * @param mt_status_message
 * @return true
 * @return false
 */
static bool satellite_mt_status_message(uint8_t mt_status_message)
{
	// We don't have error descriptions yet!
	if (mt_status_message <= 1) {
		return true;
	}

	return false;
}

/**
 * @brief Clear MO buffer
 *
 * @return true
 * @return false
 */
static bool satellite_clear_mo_buffer(void)
{
	bool validate = satellite_write_cmd_and_validate(SATELLITE_AT_CLEAR_MO_BUFFER,
							 sizeof(SATELLITE_AT_CLEAR_MO_BUFFER) - 1);
	if (!validate) {
		return validate;
	}

	// Validate 0 rsp
	validate = satellite_read_and_validate_echo("0", sizeof("0") - 1);
	if (!validate) {
		return validate;
	}

	// we did not get response yet
	validate = satellite_read_and_validate_rsp_ok();
	return validate;
}

static bool satellite_receive(int len)
{
	// Send receive command and validate echo
	bool validate = satellite_write_cmd_and_validate(SATELLITE_AT_RECEIVE_ASCII,
							 sizeof(SATELLITE_AT_RECEIVE_ASCII) - 1);
	if (!validate) {
		LOG_ERR("Did not get recv validation echo");
		return validate;
	}

	LOG_INF("Wait for satellite message of expected length: %d", len);
	validate = false;
	int head_len = sizeof(SATELLITE_AT_RSP_RECEIVE_ASCII) - 1;
	sat_uart_data_t *buf =
		k_fifo_get(&sat_fifo_uart_rx_data, K_SECONDS(SATELLITE_SEND_TIMEOUT));
	if (buf) {
		if (buf->len >= head_len) {
			if (!strncmp(buf->data, SATELLITE_AT_RSP_RECEIVE_ASCII, head_len)) {
				read_bytes = true;
				k_free(buf);
				// We went into byte read mode - we will get CR return symbol -
				// discard first message
				buf = k_fifo_get(&sat_fifo_uart_rx_data,
						 K_SECONDS(SATELLITE_SEND_TIMEOUT));
				if (buf) {
					LOG_DBG("Got CR!");
					k_free(buf);
				}

				// Got header, wait for message
				LOG_DBG("Got recv header!");
				uint8_t rcv_buf[SAT_UART_BUF_SIZE];
				int rcv_len = 0;

				while (!validate) {
					buf = k_fifo_get(&sat_fifo_uart_rx_data,
							 K_SECONDS(SATELLITE_SEND_TIMEOUT));
					if (buf) {
						LOG_HEXDUMP_DBG(buf->data, buf->len, "Received: ");

						// Copy message and length
						memcpy(rcv_buf + rcv_len, buf->data, buf->len);
						rcv_len += buf->len;

						// Check if we got the whole message
						if (rcv_len >= len) {
							LOG_HEXDUMP_INF(rcv_buf, len,
									"MT Message: ");

							thread_put_message(
								MB_MSG_SAT, MB_MSG_DEV,
								MB_MSG_EXECUTE, rcv_buf[0],
								rcv_buf + 1,
								len - 1, // Subtract one for port
								MAX_BUF_SIZE);
							validate = true;
						}
					} else {
						LOG_WRN("We did not get the message...");
						validate = true;
					}
				}
			} else {
				LOG_ERR("Failed to get header!");
			}
		}
		k_free(buf);
	}

	// If we got message, validate ok command
	read_bytes = false;
	if (validate) {
		validate = satellite_read_and_validate_rsp_ok();
		LOG_INF("We got the message and validated command: %d", validate);
	} else {
		LOG_WRN("We did not receive the message to validate.");
	}

	return validate;
}

/**
 * @brief Randomly generate session retry delays between sending retries.
 *
 * @param retry retry number
 * @return int delay in seconds.
 */
static int satellite_get_session_retry_delays(uint8_t retry)
{
	if (retry <= 3) {
		return rand() % 5 + 1;
	} else if (retry <= 6) {
		return (rand() % 15 + 5);
	} else {
		return (rand() % 20 + 20);
	}
}

/**
 * @brief Print content of SBD status.
 *
 * @param status SBD status.
 */
static void satellite_print_sbd_status(SBDStatus *status)
{
	LOG_INF("mo_status_code: %d, mo_message_number: %d, mo_success: %d, mt_status_code: %d, \
    mt_message_number: %d,  mt_length: %d, mt_queued: %d, mt_success: %d",
		status->mo_status_code, status->mo_message_number, status->mo_success,
		status->mt_status_code, status->mt_message_number, status->mt_length,
		status->mt_queued, status->mt_success);
}

/**
 * @brief Wait for SBD status response. Parse into struct.
 *
 * @param status - pointer to empty SBD structure.
 * @return true
 * @return false
 */
static bool satellite_read_and_validate_sbd_status(SBDStatus *status)
{
	LOG_INF("Wait for SBD status");
	bool validate = false;
	int head_len = sizeof(SATELLITE_AT_RSP_SBD_SESSION) - 1;
	sat_uart_data_t *buf =
		k_fifo_get(&sat_fifo_uart_rx_data, K_SECONDS(2 * SATELLITE_SEND_TIMEOUT));
	if (buf) {
		if (buf->len >= head_len) {
			if (!strncmp(buf->data, SATELLITE_AT_RSP_SBD_SESSION, head_len)) {
				// Read status values
				int idx = head_len;
				int start_val_idx = head_len;
				uint8_t val_idx = 0;
				int sbd_status_val[6];

				// Loop over rx buffer
				while (idx < buf->len) {
					// When space is detected skip it
					if (buf->data[idx] == SATELLITE_SPACE_HEX) {
						start_val_idx++;
					}
					// Comma marks new value
					else if (buf->data[idx] == SATELLITE_COMMA_HEX) {
						sbd_status_val[val_idx] =
							atoi(buf->data + start_val_idx);
						val_idx++;
						start_val_idx = idx + 1;
					}
					idx++;
				}
				// Parse last value
				if (start_val_idx < idx) {
					sbd_status_val[val_idx] = atoi(buf->data + start_val_idx);
					val_idx++;
				}
				// If we got all values, compose SBD struct
				if (val_idx == 6) {
					status->mo_status_code = sbd_status_val[0];
					status->mo_message_number = sbd_status_val[1];
					status->mt_status_code = sbd_status_val[2];
					status->mt_message_number = sbd_status_val[3];
					status->mt_length = sbd_status_val[4];
					status->mt_queued = sbd_status_val[5];
					status->mo_success =
						satellite_mo_status_message(sbd_status_val[0]);
					status->mt_success =
						satellite_mt_status_message(sbd_status_val[2]);
					validate = true;
				}
			}
		}
		k_free(buf);
	}

	// Print
	satellite_print_sbd_status(status);

	return validate;
}

/**
 * @brief Try to send / receive messages
 *
 * @return true
 * @return false
 */
bool satellite_extended_sbd_session(SBDStatus *status)
{

	bool validate = satellite_write_cmd_and_validate(
		SATELLITE_AT_EXTEND_SBD_SESSION, sizeof(SATELLITE_AT_EXTEND_SBD_SESSION) - 1);
	if (!validate) {
		return validate;
	}

	// Wait for SBD status
	validate = satellite_read_and_validate_sbd_status(status);
	if (!validate) {
		LOG_ERR("Failed to get SBD status response.");
		return validate;
	}

	// Wait for OK
	validate = satellite_read_and_validate_rsp_ok();
	if (!validate) {
		LOG_ERR("Failed to get OK response.");
		return validate;
	}

	// Check if we succeeded
	if (status->mo_success) {
		satellite_clear_mo_buffer();
	}

	return status->mo_success;
}

/**
 * @brief Enter sending/receiving sequence.
 *
 * @return true
 * @return false
 */
bool satellite_try_extended_sbd_session(SBDStatus *status)
{
	int delay = 0;
	for (uint8_t i = 0; i < Main_settings.satellite_retry->def_val; i++) {
		LOG_INF("Trying to create extended SBD session, attempt: %d out of %d", i + 1,
			Main_settings.satellite_retry->def_val);
		if (satellite_extended_sbd_session(status)) {
			Main_values.satellite_resend_try->def_val = i + 1;
			return true;
		} else {
			if (i == Main_settings.satellite_retry->def_val - 1) {
				Main_values.satellite_resend_try->def_val = i + 1;
				return false;
			}

			// Get new delay
			delay = satellite_get_session_retry_delays(i + 1);
			LOG_INF("No success trying to create extended SBD session, retry in %d "
				"seconds",
				delay);

			k_sleep(K_SECONDS(delay));
		}
	}

	return false;
}

/**
 * @brief Add byte array message to sending sequence.
 *
 * @param buf - msg
 * @param buf_len - msg length
 * @return true
 * @return false
 */
static bool satellite_queue_bytes_message(uint8_t *buf, int buf_len)
{
	satellite_clear_mo_buffer();
	if (buf_len > SAT_UART_BUF_SIZE) {
		LOG_ERR("Max satellite message size should be <= %d, but was: %d",
			SAT_UART_BUF_SIZE, buf_len);
		return false;
	}

	// Send queue bytes command and msg length
	char cmd[15];
	memcpy(cmd, SATELLITE_AT_QUEUE_BYTES_MSG, sizeof(SATELLITE_AT_QUEUE_BYTES_MSG) - 1);
	int len_cmd = sprintf(cmd + sizeof(SATELLITE_AT_QUEUE_BYTES_MSG) - 1, "%d", buf_len) +
		      sizeof(SATELLITE_AT_QUEUE_BYTES_MSG);
	cmd[len_cmd - 1] = SATELLITE_CARRIAGE_RETURN_HEX;

	bool validate = satellite_write_cmd_and_validate(cmd, len_cmd);
	if (!validate) {
		LOG_ERR("Failed to echo queue bytes command.");
		return validate;
	}

	validate = satellite_read_and_validate_echo("READY", sizeof("READY") - 1);

	// Queue message
	if (validate) {
		// Construct checksum
		uint16_t checksum = 0;
		for (int i = 0; i < buf_len; i++) {
			checksum += buf[i];
		}
		char checksum_bytes[2];
		checksum_bytes[0] = checksum >> 8;
		checksum_bytes[1] = checksum & 0xFF;

		satellite_write_bytes(buf, buf_len);
		satellite_write_bytes(checksum_bytes, 2);
	} else {
		LOG_ERR("Failed to get READY on queue bytes command. Do not send the message");
	}

	// Validate send status
	validate = false;
	sat_uart_data_t *rc = k_fifo_get(&sat_fifo_uart_rx_data, K_MSEC(1000));
	if (rc) {
		if (!strncmp(rc->data, "0", 1)) {
			LOG_INF("Got SBD message confirmation.");
			validate = satellite_read_and_validate_rsp_ok();
			if (!validate) {
				LOG_ERR("Validation of error status failed.");
			}
		} else if (!strncmp(rc->data, "1", 1)) {
			LOG_ERR("SBD message write timeout. An insufficient number of bytes were transferred to \
                    ISU during the transfer period of 60 seconds.");
		} else if (!strncmp(rc->data, "2", 1)) {
			LOG_ERR("SBD message checksum sent from DTE does not match the checksum calculated at \
                    the ISU.");
		} else if (!strncmp(rc->data, "3", 1)) {
			LOG_ERR("SBD message size is not correct. The maximum mobile originated SBD message \
                    length is 340 bytes. The minimum mobile originated SBD message length is 1 \
                    byte.");
		} else {
			LOG_ERR("Unknown status writing binary message");
		}
		k_free(rc);
	}
	if (!validate) {
		LOG_ERR("Failed to get send status validation.");
		return -EIO;
	}

	return validate;
}

static bool satellite_enter_send_receive(void)
{
	// ATM set status messages to some error in the beginning?
	SBDStatus status = {.mo_status_code = 32,
			    .mo_message_number = 0,
			    .mt_status_code = 2,
			    .mt_message_number = 0,
			    .mt_length = 0,
			    .mt_queued = 0,
			    .mo_success = false,
			    .mt_success = false};

	// Enter fist send
	bool send_success = satellite_try_extended_sbd_session(&status);

	// Success, clear satellite send buffer
	if (send_success) {
		sat_buf_idx = 0;
	}

	// While there are unread messages, read them, try to receive more MT messages
	// for a maximum of 3 times.
	int receive_attempts = 1;
	while (send_success && status.mt_status_code == 1 && receive_attempts <= 3) {
		LOG_INF("Try to receive message, attempt: %d", receive_attempts);
		if (!satellite_receive(status.mt_length)) {
			break;
		}

		if (status.mt_queued > 0) {
			LOG_INF("Check mailbox, %d messages in queue", status.mt_queued);
			if (!satellite_try_extended_sbd_session(&status)) {
				break;
			}
		} else {
			break;
		}

		receive_attempts++;
		k_sleep(K_SECONDS(1));
	}

	return send_success;
}

/**
 * @brief Send bytes
 *
 * @param buf
 * @param buf_len
 * @return true
 * @return false
 */
static bool satellite_send_bytes(char *buf, int buf_len)
{
	if (!satellite_queue_bytes_message(buf, buf_len)) {
		return false;
	}

	return satellite_enter_send_receive();
}

/**
 * @brief Enable echo command.
 *
 * @return true
 * @return false
 */
static bool satellite_enable_echo(void)
{
	bool rs = satellite_write_command_and_validate_ok(SATELLITE_AT_ENABLE_ECHO,
							  sizeof(SATELLITE_AT_ENABLE_ECHO) - 1);
	if (rs) {
		LOG_INF("Satellite enable echo done");
	} else {
		LOG_ERR("Satellite enable echo failed");
	}

	return rs;
}

/**
 * @brief Disable flow control command.
 *
 * @return true
 * @return false
 */
static bool satellite_disable_flow_control(void)
{
	bool rs = satellite_write_command_and_validate_ok(
		SATELLITE_AT_DISABLE_FLOW_CTRL, sizeof(SATELLITE_AT_DISABLE_FLOW_CTRL) - 1);
	if (rs) {
		LOG_INF("Satellite disable flow control done");
	} else {
		LOG_ERR("Satellite disable flow control failed");
	}

	return rs;
}

/**
 * @brief Disable ring alerts command.
 *
 * @return true
 * @return false
 */
static bool satellite_disable_ring_alerts(void)
{
	bool rs = satellite_write_command_and_validate_ok(
		SATELLITE_AT_DISABLE_RING_ALERTS, sizeof(SATELLITE_AT_DISABLE_RING_ALERTS) - 1);
	if (rs) {
		LOG_INF("Satellite disable ring alerts done");
	} else {
		LOG_ERR("Satellite disable ring alerts failed");
	}

	return rs;
}

/**
 * @brief Ping satellite module.
 *
 * @return true
 * @return false
 */
static bool satellite_ping(void)
{
	bool rs = satellite_write_command_and_validate_ok(SATELLITE_AT_PING,
							  sizeof(SATELLITE_AT_PING) - 1);
	if (rs) {
		LOG_INF("Satellite ping done");
	} else {
		LOG_ERR("Satellite ping failed");
	}

	return rs;
}

static void prv_update_satellite_feature_state(void)
{
	sys_features.satellite_com = prv_rockblock_detected;
}

/**
 * @brief After power up, check if communication with Rock Block can be established.
 * Current procedure: sleep for 5 s, then attempt pinging module for 5 times. With 5s timeout for
 * each ping, that gives us 20 s wakeup time.
 *
 * @return true
 * @return false
 */
static bool satellite_check_communication(void)
{
	bool rs = false;

	// Sleep for 5 s after powerup
	k_sleep(K_SECONDS(5));

	// Ping attempt 5 - times
	int attempt = 0;

	while (!rs && attempt < 5) {
		rs = satellite_ping();
		k_sleep(K_SECONDS(1));
		attempt++;
	}

	prv_rockblock_detected = rs;
	prv_update_satellite_feature_state();

	return rs;
}

/**
 * @brief Send configuration commands
 *
 * @return true
 * @return false
 */
static bool satellite_configure_port(void)
{
	// Configure port
	bool rs = satellite_enable_echo() && satellite_disable_flow_control() &&
		  satellite_disable_ring_alerts() && satellite_ping();
	if (rs) {
		LOG_INF("Configure port done");
	} else {
		LOG_ERR("Configure port failed");
	}

	return rs;
}

/**
 * @brief Request signal strength
 *
 * @return int negative error code or signal val.
 */
__attribute__((unused)) static int satellite_request_signal_strength(void)
{
	// Empty RX buffer
	satellite_empty_fifo();

	bool validate = satellite_write_cmd_and_validate(
		SATELLITE_AT_REQ_SIGNAL_STRENGTH, sizeof(SATELLITE_AT_REQ_SIGNAL_STRENGTH) - 1);
	if (!validate) {
		LOG_ERR("Request signal strength fail.");
		return -EIO;
	}

	// Wait for rsp header
	int signal = 0;

	int head_len = sizeof(SATELLITE_AT_RSP_SIGNAL_STRENGTH) - 1;
	sat_uart_data_t *buf = k_fifo_get(&sat_fifo_uart_rx_data, K_MSEC(10000));
	if (buf) {
		if (buf->len >= head_len) {
			if (!strncmp(buf->data, SATELLITE_AT_RSP_SIGNAL_STRENGTH, head_len)) {
				// Read signal strength
				if (buf->len - head_len - 1 > 0) {
					signal = atoi(buf->data + head_len);
					LOG_INF("Got signal strength: %d", signal);
				}

				validate = true;
			}
		}
		k_free(buf);
	}
	if (!validate) {
		LOG_ERR("Read signal strength fail.");
		return -EIO;
	}

	// Wait for OK
	validate = satellite_read_and_validate_rsp_ok();
	if (!validate) {
		LOG_ERR("Read and validate response fail.");
		return -EIO;
	}

	return signal;
}

/**
 * @brief Request network time. Not functional yet!
 *
 * @return int
 */
__attribute__((unused)) static int satellite_network_time(void)
{
	// Empty RX buffer
	satellite_empty_fifo();

	bool validate = satellite_write_cmd_and_validate(SATELLITE_AT_REQ_NETWORK_TIME,
							 sizeof(SATELLITE_AT_REQ_NETWORK_TIME) - 1);
	if (!validate) {
		LOG_ERR("Failed to get satellite network time.");
		return -EIO;
	}

	// Wait for rsp header
	int head_len = sizeof(SATELLITE_AT_RSP_NETWORK_TIME) - 1;
	sat_uart_data_t *buf = k_fifo_get(&sat_fifo_uart_rx_data, K_MSEC(1000));
	if (buf) {
		if (buf->len >= head_len) {
			if (!strncmp(buf->data, SATELLITE_AT_RSP_NETWORK_TIME, head_len)) {
				// Read msg
				if (buf->len - head_len - 1 > 0) {
					if (!strncmp(buf->data + head_len, " no network service",
						     sizeof(" no network service") - 1)) {
						LOG_INF("No network service!");
					} else {
						// EvaTODO what should we do with this???
						LOG_INF("Got time: %s", buf->data + head_len);
					}
				}

				validate = true;
			}
		}
		k_free(buf);
	}
	if (!validate) {
		LOG_ERR("Failed to get satellite network time.");
		return -EIO;
	}

	// Wait for OK
	validate = satellite_read_and_validate_rsp_ok();
	if (!validate) {
		LOG_ERR("Failed to get satellite network time.");
		return -EIO;
	}

	return 0;
}

/**
 * @brief Remove oldest message from the satellite buffer
 *
 */
static void satellite_remove_first_msg_from_buffer(void)
{
	// Calculate total message length. Add: head length with port, msg length and timestamp.
	int first_msg_len = MESSAGE_HEAD_LEN_BT + sat_buf[MESSAGE_HEAD_LEN_BT - 1] + TIMESTAMP_SIZE;
	// Determine remaining buffer size
	int other_msg_len = sat_buf_idx - first_msg_len;

	LOG_DBG("Remove first message of length: %d. Total buffer len: %d, remaining part: %d",
		first_msg_len, sat_buf_idx, other_msg_len);

	// Overwrite
	memmove(sat_buf, sat_buf + first_msg_len, other_msg_len);

	// Update idx
	sat_buf_idx = other_msg_len;
}

/** END PRIVATE FUNCTIONS **/

/** PUBLIC FUNCTIONS **/

int satellite_init(void)
{
	int err = 0;
	sys_features.satellite_com = false;

	// Configure GPIO
#if DT_NODE_EXISTS(SAT_EN_GPIO_NODE)
	LOG_INF("Set SAT EN GPIO to inactive.");
	err = gpio_pin_configure_dt(&sat_en_gpio, GPIO_OUTPUT_INACTIVE);
#else
	LOG_INF("SAT activation not supported on HW version!");
	return -EIO;
#endif

	// Init UART connection
#if DT_NODE_EXISTS(SAT_UART)
	if (!sat_uart_dev) {
		LOG_ERR("Failed to init satellite UART!");
		return -EIO;
	}

	LOG_INF("Satellite UART binding done!");
	err = uart_configure(sat_uart_dev, &sat_uart_cfg);
	if (err) {
		LOG_ERR("Failed to configure satellite UART!");
		uart_pm_disable(sat_uart_dev->name);
		k_sleep(K_MSEC(100));
		return -EIO;
	}
	// Ad RX callback
	uart_irq_callback_set(sat_uart_dev, satellite_uart_cb);
	// Disable UART
	uart_irq_rx_disable(sat_uart_dev);
	uart_pm_disable(sat_uart_dev->name);
	k_sleep(K_MSEC(100));
#else
	LOG_INF("Satellite UART not supported on HW version!");
	return -EIO;
#endif

	if (!err) {
		LOG_INF("Sat init completed, settings flag: %d",
			Main_settings.satellite_enabled->def_val);

		if (Main_settings.satellite_enabled->def_val) {
			satellite_test();
		}

		prv_update_satellite_feature_state();

		LOG_INF("Satellite module testing completed. Status: %d",
			sys_features.satellite_com);
	}

	return err;
}

uint8_t satellite_get_current_time_interval(void)
{
	uint8_t interval = 1;
	// Check if switching intervals is active
	if (!Main_settings.satellite_multiple_intervals->def_val) {
		return 1;
	}
	// Get latest unix time
	uint8_t hours = (get_global_unix_time() / 3600) % 24;

	/* NOTE: If both intervals are set to the same hour, interval 1 is selected */
	if (hours == Main_settings.satellite_send_interval2_start->def_val) {
		interval = 2;
	}
	if (hours == Main_settings.satellite_interval1_start->def_val) {
		interval = 1;
		return interval;
	}

	/* Check if we are outside the two intervals (circadianly - in the same day). */
	if ((hours < Main_settings.satellite_interval1_start->def_val &&
	     hours < Main_settings.satellite_send_interval2_start->def_val) ||
	    (hours > Main_settings.satellite_interval1_start->def_val &&
	     hours > Main_settings.satellite_send_interval2_start->def_val)) {
		/* Select subsequent interval */
		if (Main_settings.satellite_interval1_start->def_val >
		    Main_settings.satellite_send_interval2_start->def_val) {
			interval = 1;
		} else {
			interval = 2;
		}
		return interval;
	}

	/* Check if we are in-between the two intervals (circadianly - in the same day) */
	if (hours > Main_settings.satellite_interval1_start->def_val &&
	    hours < Main_settings.satellite_send_interval2_start->def_val) {
		interval = 1;
	}
	if (hours < Main_settings.satellite_interval1_start->def_val &&
	    hours > Main_settings.satellite_send_interval2_start->def_val) {
		interval = 2;
	}
	LOG_DBG("Current hour: %d, we are in interval: %d", hours, interval);

	return interval;
}

bool satellite_is_supported(void)
{
	return prv_rockblock_detected && Main_settings.satellite_enabled->def_val;
}

int satellite_enable(void)
{
	// Turn on power
#if DT_NODE_EXISTS(SAT_EN_GPIO_NODE)
	LOG_DBG("Set SAT EN GPIO to active.");
	gpio_pin_set_dt(&sat_en_gpio, 1);
#else
	LOG_INF("SAT activation not supported on HW version!");
	return -EIO;
#endif

	// Enable irq uart
	if (sat_uart_dev) {
		uart_pm_enable(sat_uart_dev->name);
		uart_irq_rx_enable(sat_uart_dev);
		LOG_INF("Enable satellite UART.");
		k_sleep(K_SECONDS(2));
	} else {
		LOG_ERR("Satellite UART not initialized!");
		// Turn off pwr
		gpio_pin_set_dt(&sat_en_gpio, 0);
		return -EIO;
	}

	return 0;
}

int satellite_disable(void)
{
	// Disable irq uart
	if (sat_uart_dev) {
		uart_irq_rx_disable(sat_uart_dev);
		uart_pm_disable(sat_uart_dev->name);
		k_sleep(K_MSEC(100));
	} else {
		LOG_ERR("Satellite UART not initialized!");
	}

	// Turn off power
#if DT_NODE_EXISTS(SAT_EN_GPIO_NODE)
	LOG_INF("Set SAT EN GPIO to inactive.");
	gpio_pin_set_dt(&sat_en_gpio, 0);
#else
	LOG_INF("SAT activation not supported on HW version!");
	return -EIO;
#endif

	return 0;
}

bool satellite_test(void)
{
	// Test communication
	satellite_enable();

	bool test = satellite_check_communication();

	satellite_disable();

	return test;
}

void prv_add_status_message(void)
{
	LOG_INF("Buffer is empty, add status message!");
	/* Get status */
	int len = status_get_message(sat_buf + MESSAGE_HEAD_LEN_BT, MAX_BUF_SIZE);
	/* Move and add ID and length */
	sat_buf[0] = PORT_STATUS;
	sat_buf[1] = MSG_STATUS_ID;
	sat_buf[2] = len;

	sat_buf_idx = len + MESSAGE_HEAD_LEN_BT;

	/* Get timestamp */
	uint8_t timestamp[TIMESTAMP_SIZE];
	uint32_t_to_bytes(timestamp, get_global_unix_time());
	memcpy(sat_buf + sat_buf_idx, timestamp, TIMESTAMP_SIZE);
	sat_buf_idx += TIMESTAMP_SIZE;
}

void satellite_handle_commands(void)
{
	// Check if new message
	mb_msg_dest msg_origin;
	mb_msg_action msg_action;
	uint8_t msg_port;
	uint8_t msg_max_rsp_len = 0;

	// Forever wait for new message
	int msg_size = thread_get_satellite(&msg_origin, &msg_action, &msg_port, sat_data,
					    &msg_max_rsp_len);

	LOG_INF("Got message in satellite thread. Size: %d, port: %d, action: %d, origin: %d",
		msg_size, msg_port, msg_action, msg_origin);

	// Proceed if user has enabled satellite
	if (msg_size > 0 && Main_settings.satellite_enabled->def_val) {
		// Parse message
		// Check if we need to send message
		if (msg_action == MB_MSG_SEND) {
			// Add message to send buffer and check if send
			satellite_add_message_to_send_buffer(sat_data, msg_size, msg_port);
		} else if (msg_action == MB_MSG_EXECUTE) {
			uint8_t id = sat_data[0];
			if (id == Main_settings.satellite_enabled->id) {
				LOG_INF("Satellite enabled command received");
				if (!prv_rockblock_detected) {
					satellite_test();
				}
			} else if (id == CMD_SEND_SAT_BUFFER) {
				if (sat_buf_idx > 0) {
					satellite_send_buffer();
				} else {
					if (msg_origin != MB_MSG_DEV) {
						// When send command is given from BT, or LORA, make
						// sure to always send some data so append the
						// status message since the buffer is empty.
						prv_add_status_message();
						satellite_send_buffer();
					} else {
						LOG_INF("Buffer is empty, do nothing");
					}
				}
			}
		}
	}
}

void satellite_add_message_to_send_buffer(uint8_t *msg, int msg_size, uint8_t msg_port)
{
	LOG_DBG("Idx: %d new msg size: %d", sat_buf_idx, msg_size);
	if (msg_size + TIMESTAMP_SIZE + 1 > SAT_UART_BUF_SIZE || msg_size <= 0) {
		LOG_ERR("Invalid message size: %d", msg_size);
		return;
	}

	uint8_t loop_count = 0;
	/* Check if new message is to long for remaining space in the buffer */
	while (sat_buf_idx + msg_size + TIMESTAMP_SIZE + 1 > SAT_UART_BUF_SIZE) {
		LOG_INF("Satellite buffer too full, remove oldest message!");
		satellite_remove_first_msg_from_buffer();

		/* Make sure not to get stuck inside the loop */
		if (loop_count > 50) {
			// Could be that satellite buffer is corrupt, clear the buffer
			LOG_ERR("Max loop count reached, clear buffer!");
			sat_buf_idx = 0;
			break;
		}

		loop_count++;
	}

	/* Add port nr */
	sat_buf[sat_buf_idx] = msg_port;
	sat_buf_idx++;

	/* Copy message in the buffer */
	memcpy(sat_buf + sat_buf_idx, msg, msg_size);
	sat_buf_idx += msg_size;

	/* Get timestamp */
	uint8_t timestamp[TIMESTAMP_SIZE];
	uint32_t_to_bytes(timestamp, get_global_unix_time());
	memcpy(sat_buf + sat_buf_idx, timestamp, TIMESTAMP_SIZE);
	sat_buf_idx += TIMESTAMP_SIZE;

	LOG_INF("Added message of size %d on port %d to satellite buffer of new size: %d", msg_size,
		msg_port, sat_buf_idx);
}

int satellite_send_buffer(void)
{
	set_com_thread_operation(THREAD_DISABLED);

	int err = satellite_enable();
	if (!err) {
		/* Check satellite communication */
		if (!satellite_check_communication()) {
			LOG_ERR("Satellite communication not supported!");
			err = -EIO;
		} else {
			if (!satellite_configure_port()) {
				err = -EIO;
				LOG_ERR("Satellite port configuration failed!");
				k_sleep(K_MSEC(100));
			} else {
				LOG_HEXDUMP_INF(sat_buf, sat_buf_idx,
						"Trying to send satellite message: ");
				if (!satellite_send_bytes(sat_buf, sat_buf_idx)) {
					LOG_ERR("Failed to send satellite message!");
					err = -EIO;
				}
			}
		}
	}

	satellite_disable();
	set_com_thread_operation(THREAD_NORMAL);

	return err;
}
