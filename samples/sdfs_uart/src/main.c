/*
 * Copyright (c) 2020 Irnas d.o.o.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>

#include <sdfs_uart_handler.h>

LOG_MODULE_REGISTER(sdfs_uart_sample);

/* The sample saves a message of size MSG_SIZE followed by a message of size MSG_SIZE containing
 * the counter and new line character */
#define MSG_SIZE 32
static char *prv_data = "Hello world! Counter: ";

void main(void)
{
	LOG_INF("SDFS UART example");
	int ret;

	/* For testing we set an arbitrary timestamp. The timestamp should show as
	 * November 5, 2025 3:59:24 PM  */
	time_t timestamp = 1762354752;
	sdfs_uart_set_time_from_global_unix_timestamp(&timestamp);

	char counter_buf[MSG_SIZE];
	int counter = 0;

	while (1) {
		counter++;
		sprintf(counter_buf, "%d\n", counter);

		char full_msg[MSG_SIZE * 2];
		sprintf(full_msg, "%s", prv_data);
		strcat(full_msg, counter_buf);

		/* Write data to SDFS over UART */
		ret = sdfs_uart_file_append("\\test1.txt", full_msg, strlen(full_msg));
		if (ret < 0) {
			LOG_ERR("SDFS UART write failed: %d", ret);
		} else {
			LOG_INF("Wrote %d bytes to SDFS UART", ret);
		}

		sdfs_uart_pause_write(K_SECONDS(10));
	}
}
