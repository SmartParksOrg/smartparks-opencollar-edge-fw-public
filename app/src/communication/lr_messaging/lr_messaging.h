/** @file lr_messaging.h
 *
 * @brief Interface for lora messaging
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2022 Irnas. All rights reserved.
 */

#ifndef LR_MESSAGING_H
#define LR_MESSAGING_H

#include <zephyr/kernel.h>

/**
 * @brief Store incoming LR messages in messaging system.
 *
 * @param msg - incoming message
 * @param len - length of incoming message
 *
 * @return negative error code or 0.
 */
int lr_messaging_store_incoming(uint8_t *msg, uint8_t len);

/**
 * @brief Get nr. of unread incoming messages, stored in buffer
 *
 * @return uint8_t
 */
uint8_t lr_messaging_get_nr_incoming_msg(void);

/**
 * @brief Return the oldest message in the que, composed in the form of MSG_LR_MESSAGING_ID. Remove
 * msg from que once read.
 *
 * @param msg - buffer to copy message into
 * @return int - message length or negative error
 */
int lr_messaging_read_incoming_msg(uint8_t *msg);

/**
 * @brief Store outgoing messages to send
 *
 * @param msg
 * @param len
 * @return int
 */
int lr_messaging_store_outgoing(uint8_t *msg, uint8_t len);

/**
 * @brief Get nr. of unsend outgoing messages, stored in buffer
 *
 * @return uint8_t
 */
uint8_t lr_messaging_get_nr_outgoing_msg(void);

/**
 * @brief Get next message in line to be sent and copy it in the provided buffer. Remove the message
 * from the que, if max retry count was reached.
 *
 * @param msg buffer to store message into
 * @return int length of the message to send
 */
int lr_messaging_send_outgoing_msg(uint8_t *msg);

#endif // LR_MESSAGING_H
