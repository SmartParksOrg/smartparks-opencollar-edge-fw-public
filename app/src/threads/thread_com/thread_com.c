/** @file thread_com.c
 *
 * @brief Communication between threads.
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2020 Irnas. All rights reserved.
 */
#include "thread_com.h"

LOG_MODULE_REGISTER(thread_com); // init logging

/* message buffer to hold 4 messages */
K_MSGQ_DEFINE(main_que, sizeof(message_struct), 8, 4);
K_MSGQ_DEFINE(lr_que, sizeof(message_struct), 4, 4);
K_MSGQ_DEFINE(flash_que, sizeof(message_struct), 4, 4);
K_MSGQ_DEFINE(sensors_que, sizeof(message_struct), 4, 4);
#ifdef CONFIG_SATELLITE
K_MSGQ_DEFINE(satellite_que, sizeof(message_struct), 4, 4);
#endif // CONFIG_SATELLITE

int thread_put_message(mb_msg_dest origin, mb_msg_dest dest, mb_msg_action message_action,
		       uint8_t port, uint8_t *data, uint8_t len, uint8_t max_rsp_len)
{
	int err = 0;

	if (len <= MAX_BUF_SIZE) {
		// Create message
		message_struct message;
		// Copy data
		memcpy(message.data, data, len);
		message.len = len;
		message.port = port;
		message.origin = origin;
		message.dest = dest;
		message.message_action = message_action;
		message.max_rsp_len = max_rsp_len;
		if (message.max_rsp_len > MAX_BUF_SIZE) {
			message.max_rsp_len = MAX_BUF_SIZE;
		}
		// Put message in correct queue based on the receiver
		if (dest == MB_MSG_BT || dest == MB_MSG_DEV) {
			// If  msg needs to be send, check if connected, otherwise return error
			if (dest == MB_MSG_BT && message_action == MB_MSG_SEND) {
				if (!bt_is_connected()) {
					return -ENOTCONN;
				}
			}

			while (k_msgq_put(&main_que, &message, K_NO_WAIT) != 0) {
				/* message queue is full: purge old data & try again */
				k_msgq_purge(&main_que);
			}
			return 0;
		} else if (dest == MB_MSG_LORA) {
			while (k_msgq_put(&lr_que, &message, K_NO_WAIT) != 0) {
				/* message queue is full: purge old data & try again */
				k_msgq_purge(&lr_que);
			}
			return 0;
		} else if (dest == MB_MSG_FLASH) {
			while (k_msgq_put(&flash_que, &message, K_NO_WAIT) != 0) {
				/* message queue is full: purge old data & try again */
				k_msgq_purge(&flash_que);
			}
			return 0;
		} else if (dest == MB_MSG_SENSORS) {
			while (k_msgq_put(&sensors_que, &message, K_NO_WAIT) != 0) {
				/* message queue is full: purge old data & try again */
				k_msgq_purge(&sensors_que);
			}
			return 0;
		}
#ifdef CONFIG_SATELLITE
		else if (dest == MB_MSG_SAT) {
			while (k_msgq_put(&satellite_que, &message, K_NO_WAIT) != 0) {
				/* message queue is full: purge old data & try again */
				k_msgq_purge(&satellite_que);
			}
			return 0;
		}
#endif // CONFIG_SATELLITE
		else {
			return -EIO;
		}
	} else {
		LOG_ERR("Mailbox message to long to send!");
		err = -EMSGSIZE;
	}
	return err;
}

int thread_get_main(mb_msg_dest *origin, mb_msg_dest *dest, mb_msg_action *msg_action,
		    uint8_t *port, void *msg_data, uint8_t *max_rsp_len)
{
	int err = 0;
	message_struct message;
	/* get message */
	err = k_msgq_get(&main_que, &message, K_NO_WAIT);

	if (!err) {
		if (message.len > 0 && message.len <= MAX_BUF_SIZE) {
			/* retrieve message data */
			*origin = message.origin;
			*dest = message.dest;
			*msg_action = message.message_action;
			*port = message.port;
			*max_rsp_len = message.max_rsp_len;
			memcpy(msg_data, message.data, message.len);
			LOG_INF("REC S: ");
			for (int i = 0; i < message.len; i++) {
				LOG_DBG("%x ", message.data[i]);
			}
			/* return message length */
			err = message.len;
		} else {
			LOG_ERR("Received message length 0!");
			err = -EMSGSIZE;
		}
	}
	return err;
}

int thread_get_lr(mb_msg_dest *origin, mb_msg_action *msg_action, uint8_t *port, void *msg_data,
		  uint8_t *max_rsp_len)
{
	int err = 0;
	message_struct message;
	/* get message */
	err = k_msgq_get(&lr_que, &message, K_NO_WAIT);

	if (!err) {
		if (message.len > 0 && message.len <= MAX_BUF_SIZE) {
			/* retrieve message data */
			*origin = message.origin;
			*msg_action = message.message_action;
			*port = message.port;
			*max_rsp_len = message.max_rsp_len;
			memcpy(msg_data, message.data, message.len);
			LOG_INF("REC S: ");
			for (int i = 0; i < message.len; i++) {
				LOG_DBG("%x ", message.data[i]);
			}
			/* return message length */
			err = message.len;
		} else {
			LOG_ERR("Received message length 0!");
			err = -EMSGSIZE;
		}
	}
	return err;
}

int thread_get_flash(mb_msg_dest *origin, mb_msg_action *msg_action, uint8_t *port, void *msg_data,
		     uint8_t *max_rsp_len)
{
	int err = 0;
	message_struct message;
	/* get message */
	err = k_msgq_get(&flash_que, &message, K_NO_WAIT);

	if (!err) {
		if (message.len > 0 && message.len <= MAX_BUF_SIZE) {
			/* retrieve message data */
			*origin = message.origin;
			*msg_action = message.message_action;
			*port = message.port;
			*max_rsp_len = message.max_rsp_len;
			memcpy(msg_data, message.data, message.len);
			LOG_INF("REC S: ");
			for (int i = 0; i < message.len; i++) {
				LOG_DBG("%x ", message.data[i]);
			}
			/* return message length */
			err = message.len;
		} else {
			LOG_ERR("Received message length 0!");
			err = -EMSGSIZE;
		}
	}
	return err;
}

int thread_get_sensors(mb_msg_dest *origin, mb_msg_action *msg_action, uint8_t *port,
		       void *msg_data, uint8_t *max_rsp_len)
{
	int err = 0;
	message_struct message;
	/* get message */
	err = k_msgq_get(&sensors_que, &message, K_NO_WAIT);

	if (!err) {
		if (message.len > 0 && message.len <= MAX_BUF_SIZE) {
			/* retrieve message data */
			*origin = message.origin;
			*msg_action = message.message_action;
			*port = message.port;
			*max_rsp_len = message.max_rsp_len;
			memcpy(msg_data, message.data, message.len);
			LOG_INF("REC S: ");
			for (int i = 0; i < message.len; i++) {
				LOG_DBG("%x ", message.data[i]);
			}
			/* return message length */
			err = message.len;
		} else {
			LOG_ERR("Received message length 0!");
			err = -EMSGSIZE;
		}
	}
	return err;
}

#ifdef CONFIG_SATELLITE
int thread_get_satellite(mb_msg_dest *origin, mb_msg_action *msg_action, uint8_t *port,
			 void *msg_data, uint8_t *max_rsp_len)
{
	int err = 0;
	message_struct message;
	/* get message */
	err = k_msgq_get(&satellite_que, &message, K_FOREVER);

	if (!err) {
		if (message.len > 0 && message.len <= MAX_BUF_SIZE) {
			/* retrieve message data */
			*origin = message.origin;
			*msg_action = message.message_action;
			*port = message.port;
			*max_rsp_len = message.max_rsp_len;
			memcpy(msg_data, message.data, message.len);
			LOG_INF("REC S: ");
			for (int i = 0; i < message.len; i++) {
				LOG_DBG("%x ", message.data[i]);
			}
			/* return message length */
			err = message.len;
		} else {
			LOG_ERR("Received message length 0!");
			err = -EMSGSIZE;
		}
	}
	return err;
}
#endif // CONFIG_SATELLITE

uint8_t compose_response_msg(uint8_t *msg, int err, uint8_t *port)
{
	// Add status of cmd execution
	if (err < 0) {
		msg[3] = 0;
	} else {
		msg[3] = 1;
	}
	msg[2] = msg[0]; // CMD id
	msg[1] = Main_messages.msg_cmd_confirm->length;
	msg[0] = Main_messages.msg_cmd_confirm->id;

	*port = Main_messages.msg_cmd_confirm->port;
	return Main_messages.msg_cmd_confirm->length + MESSAGE_HEAD_LEN;
}

/*** end of file ***/
