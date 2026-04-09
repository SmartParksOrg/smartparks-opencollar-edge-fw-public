/*
 * Copyright (c) 2020 Irnas d.o.o.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/types.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>

#define LED_NODE    DT_ALIAS(led_r)
#define LED         DT_GPIO_LABEL(LED_NODE, gpios)
#define PIN         DT_GPIO_PIN(LED_NODE, gpios)
#define STATE_SLEEP 1000

#define RF_UART  DT_ALIAS(gsm_uart)
#define BUF_SIZE 255

const struct device *uart_dev;
const struct uart_config uart_cfg = {
	.baudrate = 115200,
	.parity = UART_CFG_PARITY_NONE,
	.stop_bits = UART_CFG_STOP_BITS_1,
	.data_bits = UART_CFG_DATA_BITS_8,
	.flow_ctrl = UART_CFG_FLOW_CTRL_NONE,
};

// struct to keep the message when it is put in the fifo queue, where it waits
// to be sent
typedef struct {
	void *fifo_reserved;
	uint8_t data[BUF_SIZE];
	uint16_t len;
} uart_data_t;

static K_FIFO_DEFINE(fifo_uart_rx_data);

static void uart_cb(const struct device *dev, void *user_data)
{
	static uart_data_t *rx;
	ARG_UNUSED(user_data);

	uart_irq_update(dev);
	printk("Uart CB\n");

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

				printk("Not able to allocate UART receive buffer\n");
				return;
			}
		}

		data_length = uart_fifo_read(dev, &rx->data[rx->len], BUF_SIZE - rx->len);
		rx->len += data_length;
		printk("RECEIVED BUFFER: %s, LEN: %d\n", rx->data, rx->len);

		if (rx->len > 0) {
			if ((rx->len == BUF_SIZE) || (rx->data[rx->len - 1] == '\n') ||
			    (rx->data[rx->len - 1] == '\r')) {
				printk("Data put in rx buffer!\n");
				k_fifo_put(&fifo_uart_rx_data, rx);
				rx = NULL;
			}
		}
	}

	// TX is done via poll_out, so nothing to be handled here
}

void main(void)
{
	printk("UART 1 example\n");
	const struct device *dev;
	bool led_is_on = true;
	int ret;

	k_sleep(K_MSEC(1000));

	dev = device_get_binding(LED);
	if (dev == NULL) {
		printk("GPIO device get binding failed!\n");
		return;
	}
	printk("GPIO LED device get binding done\n");

	ret = gpio_pin_configure(dev, PIN, GPIO_OUTPUT_LOW);
	if (ret < 0) {
		printk("LED pin configure failed!\n");
		return;
	}
	printk("LED pin configure done\n");

#if DT_NODE_EXISTS(RF_UART)
	uart_dev = device_get_binding(DT_LABEL(RF_UART));
	if (!uart_dev) {
		printk("%s error UART 1\n", DT_LABEL(RF_UART));
		ret = -EIO;
	} else {
		// Configure
		printk("%s Init OK UART 1\n", DT_LABEL(RF_UART));
		// ret = uart_configure(uart_dev, &uart_cfg);
		uart_irq_callback_set(uart_dev, uart_cb);
		k_sleep(K_MSEC(1000));
		uart_irq_rx_enable(uart_dev);
	}
#else
	printk("UART for nrf52 communication not defined in dts configuration!\n");
#endif

	uint8_t c = 0x42;

	while (1) {
		gpio_pin_set(dev, PIN, (int)led_is_on);
		led_is_on = !led_is_on;
		uart_poll_out(uart_dev, c);
		if (!uart_poll_in(uart_dev, &c)) {
			printk("Got: %c\n", c);
		}
		k_msleep(STATE_SLEEP);
	}
}
