/** @file lr_messaging.h
 *
 * @brief Interface for lora messaging
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2022 Irnas. All rights reserved.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <sys/types.h>

#include "generated_settings.h"
#include "global_time.h"
#include "lr_messaging.h"

#define LR_MESSAGING_MAX_MSG_LEN  46
#define LR_MESSAGING_N_STORED_MSG 5
#define LR_MESSAGING_ID_IDX       0
#define LR_MESSAGING_LEN_IDX      1
#define LR_MESSAGING_DATA_LEN_IDX 2

LOG_MODULE_REGISTER(lr_messaging); // init logging

struct lr_message_struct {
	uint8_t len; // Message length
	uint8_t data[LR_MESSAGING_MAX_MSG_LEN];
	uint8_t seq_nr;
	uint8_t retry_nr;
	uint32_t timestamp;
};

struct lr_message_que {
	struct lr_message_struct que[LR_MESSAGING_N_STORED_MSG];
	uint8_t N;
};

/* message buffer to hold 4 messages */
struct lr_message_que incoming_msg_que;
struct lr_message_que outgoing_msg_que;

uint8_t outgoing_seq_nr = 0;
uint8_t send_idx = 0; // Index of next outgoing message in the buffer to be sent

/* Private functions */

/**
 * @brief Get msg seq nr.
 *
 * @param msg
 * @return uint8_t seq nr.
 */
static uint8_t get_msg_seq_nr(uint8_t *msg)
{
	return msg[LR_MESSAGING_DATA_LEN_IDX + msg[LR_MESSAGING_DATA_LEN_IDX] + 1];
}

/**
 * @brief Get msg retry nr.
 *
 * @param msg
 * @return uint8_t retry nr.
 */
static uint8_t get_msg_retry_nr(uint8_t *msg)
{
	return msg[LR_MESSAGING_DATA_LEN_IDX + msg[LR_MESSAGING_DATA_LEN_IDX] + 2];
}

/**
 * @brief Clear data from message structure
 *
 * @param msg message structure to clear
 */
static void clear_lr_message(struct lr_message_struct *msg)
{
	memset(msg->data, 0, LR_MESSAGING_MAX_MSG_LEN);
	msg->len = 0;
	msg->retry_nr = 0;
	msg->seq_nr = 0;
	msg->timestamp = 0;
}

/**
 * @brief Remove last message from structure.
 *
 * @param mq - message que structure
 * @return
 */
static void remove_last_msg(struct lr_message_que *mq)
{
	// Buffer empty
	if (mq->N == 0) {
		return;
	}
	mq->N--;
	LOG_INF("New number of outgoing messages: %d", mq->N);

	// Move messages
	for (uint8_t i = 0; i < mq->N; i++) {
		mq->que[i] = mq->que[i + 1];
	}

	clear_lr_message(&mq->que[mq->N]);

	return;
}

/**
 * @brief
 *
 * @param mq - message que to add message to
 * @param msg - message as byte array
 */
static void add_new_message(struct lr_message_que *mq, uint8_t *msg)
{
	// Check if we have space in the que, if not, clear last message
	if (mq->N >= LR_MESSAGING_MAX_MSG_LEN) {
		remove_last_msg(mq);
	}

	// Parse data from byte array
	// Message length
	mq->que[mq->N].len = msg[LR_MESSAGING_DATA_LEN_IDX];
	// Copy data
	memcpy(mq->que[mq->N].data, msg + LR_MESSAGING_DATA_LEN_IDX + 1, mq->que[mq->N].len);
	// Seq. nr.
	mq->que[mq->N].seq_nr = get_msg_seq_nr(msg);
	// retry count
	mq->que[mq->N].retry_nr = get_msg_retry_nr(msg);

	// Add timestamp
	mq->que[mq->N].timestamp = get_global_unix_time();

	LOG_INF("Add new msg of len: %d with seq.nr: %d and retry count: %d", mq->que[mq->N].len,
		mq->que[mq->N].seq_nr, mq->que[mq->N].retry_nr);

	mq->N++;

	LOG_INF("New nr. of msg: %d", mq->N);
}

/**
 * @brief
 *
 * @param msg - message as byte array
 * @return bool - true if seq nr. present and replaced
 */
static int add_incoming_msg(uint8_t *msg)
{
	uint8_t seq = get_msg_seq_nr(msg);
	LOG_INF("Add incoming msg with seq. nr.: %d", seq);

	// Find in message with sequence nr. in already stored
	for (uint8_t i = 0; i < incoming_msg_que.N; i++) {
		// If sequence is found
		if (incoming_msg_que.que[i].seq_nr == seq) {
			// Move it to the last place and replace the message
			incoming_msg_que.N--;
			for (uint8_t j = i; j < incoming_msg_que.N; j++) {
				incoming_msg_que.que[j] = incoming_msg_que.que[j + 1];
			}
			break;
		}
	}

	// Add message
	add_new_message(&incoming_msg_que, msg);

	return 0;
}

/**
 * @brief Add new data to buffer to send via LR
 *
 * @param msg - outgoing msg data
 * @param len / data len
 * @return int
 */
static int add_outgoing_msg(uint8_t *msg, uint8_t len)
{
	// Check if to many messages are stored in buffer
	if (outgoing_msg_que.N >= LR_MESSAGING_MAX_MSG_LEN) {
		remove_last_msg(&outgoing_msg_que);
	}

	// Increase sequence nr
	outgoing_seq_nr++;

	// Add data
	outgoing_msg_que.que[outgoing_msg_que.N].len = len;
	memcpy(outgoing_msg_que.que[outgoing_msg_que.N].data, msg, len);
	outgoing_msg_que.que[outgoing_msg_que.N].retry_nr = 0;
	outgoing_msg_que.que[outgoing_msg_que.N].seq_nr = outgoing_seq_nr;
	outgoing_msg_que.que[outgoing_msg_que.N].timestamp = get_global_unix_time();

	outgoing_msg_que.N++;

	return 0;
}

/**
 * @brief
 *
 * @param msg - incoming message
 * @param len - length of incoming message
 * @return true
 * @return false
 */
static bool validate_msg(uint8_t *msg, uint8_t len)
{
	// Invalid length - we need space for id, len, msg, retry nr. and seq. nr
	if (len < 4) {
		return false;
	}

	// Check msg ID
	uint8_t id = msg[LR_MESSAGING_ID_IDX];
	if (id != MSG_LR_MESSAGING_ID) {
		return false;
	}

	// Check if error in length
	uint8_t msg_len = msg[LR_MESSAGING_LEN_IDX];
	if (msg_len + 2 != len) {
		return false;
	}

	return true;
}

/* PUBLIC FUNCTIONS */

int lr_messaging_store_incoming(uint8_t *msg, uint8_t len)
{
	// Validate message
	if (!validate_msg(msg, len)) {
		LOG_ERR("Invalid LR message format!");
		return -EIO;
	}

	// Add to buffer
	return add_incoming_msg(msg);
}

uint8_t lr_messaging_get_nr_incoming_msg(void)
{
	return incoming_msg_que.N;
}

int lr_messaging_read_incoming_msg(uint8_t *msg)
{
	// Check if there is any message left in the incoming que
	if (incoming_msg_que.N == 0) {
		return 0;
	}

	// Compose message
	uint8_t idx = 0;
	// Msg ID
	msg[idx] = MSG_LR_MESSAGING_TIMESTAMP_ID;
	idx++;
	// Msg length
	msg[idx] = incoming_msg_que.que[0].len + 7;
	idx++;
	// Data len
	msg[idx] = incoming_msg_que.que[0].len;
	idx++;
	// Data
	memcpy(msg + idx, incoming_msg_que.que[0].data, incoming_msg_que.que[0].len);
	idx += incoming_msg_que.que[0].len;
	// Seq nr.
	msg[idx] = incoming_msg_que.que[0].seq_nr;
	idx++;
	// Retry nr.
	msg[idx] = incoming_msg_que.que[0].retry_nr;
	idx++;
	// Timestamp
	memcpy(msg + idx, &incoming_msg_que.que[0].timestamp, 4);
	idx += 4;

	LOG_INF("Read msg seq: %d and remove it from que.", incoming_msg_que.que[0].seq_nr);
	remove_last_msg(&incoming_msg_que);

	return idx;
}

int lr_messaging_store_outgoing(uint8_t *msg, uint8_t len)
{
	// Validate?

	return add_outgoing_msg(msg, len);
}

uint8_t lr_messaging_get_nr_outgoing_msg(void)
{
	return outgoing_msg_que.N;
}

int lr_messaging_send_outgoing_msg(uint8_t *msg)
{
	if (outgoing_msg_que.N == 0) {
		return 0;
	}

	// Compose message
	uint8_t idx = 0;
	// Msg id
	msg[idx] = MSG_LR_MESSAGING_ID;
	idx++;
	// Message length
	msg[idx] = outgoing_msg_que.que[send_idx].len + 3;
	// Message data length
	idx++;
	msg[idx] = outgoing_msg_que.que[send_idx].len;
	idx++;
	// Data
	memcpy(msg + idx, outgoing_msg_que.que[send_idx].data, outgoing_msg_que.que[send_idx].len);
	idx += outgoing_msg_que.que[send_idx].len;
	// seq nr.
	msg[idx] = outgoing_msg_que.que[send_idx].seq_nr;
	idx++;
	// retry nr
	outgoing_msg_que.que[send_idx].retry_nr++;          // Increase
	msg[idx] = outgoing_msg_que.que[send_idx].retry_nr; // Increase
	idx++;

	LOG_INF("Send message with seq. nr: %d, retry: %d", outgoing_msg_que.que[send_idx].seq_nr,
		outgoing_msg_que.que[send_idx].retry_nr);
	// Check if max retry count is reached
	if (outgoing_msg_que.que[send_idx].retry_nr >=
	    Main_settings.lr_messaging_retry_count->def_val) {
		// Remove message from the que - it must be the last one
		LOG_INF("Remove msg: %d from outgoing que.", outgoing_msg_que.que[send_idx].seq_nr);
		remove_last_msg(&outgoing_msg_que);
	} else {
		// If message is not removed update send sequence idx
		send_idx++;
	}

	// Check for wraparound
	if (send_idx >= outgoing_msg_que.N) {
		send_idx = 0;
	}

	return idx;
}
