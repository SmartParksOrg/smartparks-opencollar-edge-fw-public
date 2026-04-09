/** @file thread_com.h
 *
 * @brief Communication between threads.
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2022 Irnas. All rights reserved.
 */

#ifndef THREAD_COM_H
#define THREAD_COM_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>

#include "bt_con.h" //Needed to check BT connection
#include "definitions.h"
#include "generated_settings.h"

#include <zephyr/logging/log.h>

/* mailbox message definitions, origin, destination */
typedef enum {
	MB_MSG_DEV = 0x00,     // Message from/to device
	MB_MSG_LORA = 0x01,    // Message from/to Lora
	MB_MSG_BT = 0x02,      // Message from/to BT
	MB_MSG_FLASH = 0x03,   // Message from/to flash
	MB_MSG_SENSORS = 0x04, // Message from/to sensors and external modules
	MB_MSG_SAT = 0x05,     // Message from/to satellite module
	MB_MSG_LP0 = 0x06,     // Message from LP0 module (messaging LP0 is one way - to LP0)
} mb_msg_dest;

/* mailbox message actions */
typedef enum {
	MB_MSG_EXECUTE = 0x01,                     /* Execute message */
	MB_MSG_SEND = 0x02,                        /* Send message */
	MB_MSG_STORE = 0x03,                       /* Store message */
	MB_MSG_EXECUTE_FLASH_READ_CONTINUE = 0x04, /* Continue reading from flash */
} mb_msg_action;

/**
 * @brief Structure containing message data
 *
 */
typedef struct {
	mb_msg_dest origin; // Message sender
	mb_msg_dest dest;   // Message receiver
	mb_msg_action message_action;
	uint8_t len;         // Message length
	uint8_t port;        // Sending port
	uint8_t max_rsp_len; // Max length of the response
	uint8_t data[MAX_BUF_SIZE];
} message_struct;

/**
 * @brief Send message to respective thread queue.
 *
 * @param origin Message author thread.
 * @param dest  Message receiver thread.
 * @param message_action Message action.
 * @param port Port of Message.
 * @param msg_data Pointer to Message binary array.
 * @param len Message length.
 * @param max_rsp_len Max length of rsp message.
 *
 * @retval 0 Message sent.
 * @retval -ENOTCONN No BT connection.
 * @retval -EIO Invalid destination.
 * @retval -EMSGSIZE message size bigger then buffer length MAX_BUF_SIZE.
 */
int thread_put_message(mb_msg_dest origin, mb_msg_dest dest, mb_msg_action message_action,
		       uint8_t port, uint8_t *msg_data, uint8_t len, uint8_t max_rsp_len);

/**
 * @brief Get message from main thread queue. No-wait protocol.
 *
 * @param origin Sending message author thread.
 * @param dest Sending message destination. Can be MB_MSG_DEV or MB_MSG_BT in the case of main
 * thread queue.
 * @param msg_action Message action.
 * @param port Pointer to port number.
 * @param msg_data Pointer to Message binary array.
 * @param max_rsp_len Max length of rsp message.
 *
 * @return int - return message length.
 * @retval -ENOMSG Returned without waiting.
 */
int thread_get_main(mb_msg_dest *origin, mb_msg_dest *dest, mb_msg_action *msg_action,
		    uint8_t *port, void *msg_data, uint8_t *max_rsp_len);

/**
 * @brief Get message from lr thread queue. No-wait protocol.
 *
 * @param origin Sending message author thread.
 * @param msg_action Message action.
 * @param port Pointer to port number.
 * @param msg_data Pointer to Message binary array.
 * @param max_rsp_len Max length of rsp message.
 *
 * @return int - return message length.
 * @retval -ENOMSG Returned without waiting.
 */
int thread_get_lr(mb_msg_dest *origin, mb_msg_action *msg_action, uint8_t *port, void *msg_data,
		  uint8_t *max_rsp_len);

/**
 * @brief Get message from flash thread queue. No-wait protocol.
 *
 * @param origin Sending message author thread.
 * @param msg_action Message action.
 * @param port Pointer to port number.
 * @param msg_data Pointer to Message binary array.
 * @param max_rsp_len Max length of rsp message.
 *
 * @return int - return message length.
 * @retval -ENOMSG Returned without waiting.
 */
int thread_get_flash(mb_msg_dest *origin, mb_msg_action *msg_action, uint8_t *port, void *msg_data,
		     uint8_t *max_rsp_len);

/**
 * @brief Get message from sensor thread queue. Wait forever for new message protocol.
 *
 * @param origin Sending message author thread.
 * @param msg_action Message action.
 * @param port Pointer to port number.
 * @param msg_data Pointer to Message binary array.
 * @param max_rsp_len Max length of rsp message.
 *
 * @return int - return message length.
 * @retval -ENOMSG Returned without waiting.
 */
int thread_get_sensors(mb_msg_dest *origin, mb_msg_action *msg_action, uint8_t *port,
		       void *msg_data, uint8_t *max_rsp_len);

#ifdef CONFIG_SATELLITE
/**
 * @brief Get message from satellite thread queue. Wait forever for new message protocol.
 *
 * @param origin Sending message author thread.
 * @param msg_action Message action.
 * @param port Pointer to port number.
 * @param msg_data Pointer to Message binary array.
 * @param max_rsp_len Max length of rsp message.
 *
 * @return int - return message length.
 * @retval -ENOMSG Returned without waiting.
 */
int thread_get_satellite(mb_msg_dest *origin, mb_msg_action *msg_action, uint8_t *port,
			 void *msg_data, uint8_t *max_rsp_len);
#endif // CONFIG_SATELLITE

/*!
 * @brief Compose generic response msg
 *
 * @param[in] uint8_t *msg - buffer with msg data
 * @param[in] int err - latest error code of the execution
 * @param pointer to port
 *
 * @return msg size
 */
uint8_t compose_response_msg(uint8_t *msg, int err, uint8_t *port);

#endif /* THREAD_COM_H */

/*** end of file ***/
