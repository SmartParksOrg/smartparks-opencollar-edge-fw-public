/** @file uart_pm.h
 *
 * @brief Interface for power management of uart peripheral
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2022 Irnas. All rights reserved.
 */

#include "uart_pm.h"

#include <string.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(uart_pm, 4);

/* Bind to GPIO device */
const struct device *gpio_dev0 = DEVICE_DT_GET(DT_NODELABEL(gpio0));
const struct device *gpio_dev1 = DEVICE_DT_GET(DT_NODELABEL(gpio1));

/* Bind to UART device */
#if DT_NODE_HAS_STATUS(DT_NODELABEL(uart0), okay)
const struct device *uart_dev0 = DEVICE_DT_GET(DT_NODELABEL(uart0));
#else
const struct device *uart_dev0;
#endif // DT_NODE_EXISTS(DT_NODELABEL(uart0))

#if DT_NODE_HAS_STATUS(DT_NODELABEL(uart1), okay)
const struct device *uart_dev1 = DEVICE_DT_GET(DT_NODELABEL(uart1));
#else
const struct device *uart_dev1;
#endif // DT_NODE_EXISTS(DT_NODELABEL(uart1))

/** PRIVATE FUNCTIONS **/
static void uart_pm_pin_to_input(uint8_t pin)
{
	if (pin >= 32) {
		pin -= 32;
		gpio_pin_configure(gpio_dev1, pin, (GPIO_INPUT | GPIO_PULL_DOWN));
	} else {
		gpio_pin_configure(gpio_dev0, pin, (GPIO_INPUT | GPIO_PULL_DOWN));
	}
}

static void uart_pm_pin_to_output(uint8_t pin)
{
	if (pin >= 32) {
		pin -= 32;
		gpio_pin_configure(gpio_dev1, pin, GPIO_OUTPUT);
	} else {
		gpio_pin_configure(gpio_dev0, pin, GPIO_OUTPUT);
	}
}

/**
 * @brief Disable UART_0 peripheral.
 * Important note: if UART_0 is used as logging backend this function can fail if
 * LOG was called recently. Allow up to 2 seconds of sleep before calling this function!
 *
 */
static void uart_pm_disable_uart0(void)
{
	// Disable UART
	NRF_UARTE0->TASKS_STOPTX = 1;
	NRF_UARTE0->EVENTS_RXTO = 0;
	NRF_UARTE0->TASKS_STOPRX = 1;
	while (NRF_UARTE0->EVENTS_RXTO == 0) {
	}
	NRF_UARTE0->EVENTS_RXTO = 0;
	// disable whole UART 0 interface
	NRF_UARTE0->ENABLE = 0;

	// Put both rx and tx to input
	LOG_INF("We try to set pins TX: %d and RX: %d to input", NRF_UARTE0->PSEL.TXD,
		NRF_UARTE0->PSEL.RXD);
	uart_pm_pin_to_input(NRF_UARTE0->PSEL.TXD);
	uart_pm_pin_to_input(NRF_UARTE0->PSEL.RXD);
}

/**
 * @brief Disable UART_1 peripheral.
 * Important note: if UART_1 is used as logging backend this function can fail if
 * LOG was called recently. Allow up to 2 seconds of sleep before calling this function!
 *
 */
static void uart_pm_disable_uart1(void)
{
	// Disable UART
	NRF_UARTE1->TASKS_STOPTX = 1;
	NRF_UARTE1->EVENTS_RXTO = 0;
	NRF_UARTE1->TASKS_STOPRX = 1;
	while (NRF_UARTE1->EVENTS_RXTO == 0) {
	}
	NRF_UARTE1->EVENTS_RXTO = 0;
	// disable whole UART 0 interface
	NRF_UARTE1->ENABLE = 0;

	// Put both rx and tx to input
	LOG_INF("We try to set pins TX: %d and RX: %d to input", NRF_UARTE1->PSEL.TXD,
		NRF_UARTE1->PSEL.RXD);
	uart_pm_pin_to_input(NRF_UARTE1->PSEL.TXD);
	uart_pm_pin_to_input(NRF_UARTE1->PSEL.RXD);
}

/**
 * @brief Enable UART_0 peripheral.
 *
 */
static void uart_pm_enable_uart0(void)
{
	// Put both rx and tx to output
	LOG_INF("We try to set pins TX: %d and RX: %d to output", NRF_UARTE0->PSEL.TXD,
		NRF_UARTE0->PSEL.RXD);
	uart_pm_pin_to_output(NRF_UARTE0->PSEL.TXD);
	uart_pm_pin_to_output(NRF_UARTE0->PSEL.RXD);

	NRF_UARTE0->EVENTS_ENDTX = 0;
	NRF_UARTE0->EVENTS_TXDRDY = 0;
	NRF_UARTE0->EVENTS_TXSTARTED = 0;

	NRF_UARTE0->ENABLE = 0x00000008;
	NRF_UARTE0->TASKS_STARTTX = 1;
	NRF_UARTE0->TASKS_STARTRX = 1;

	NRF_UARTE0->ENABLE = 0x00000008;
	NRF_UARTE0->TASKS_STARTTX = 1;
	NRF_UARTE0->TASKS_STARTRX = 1;

	k_sleep(K_MSEC(500));
}

/**
 * @brief Enable UART_1 peripheral.
 *
 */
static void uart_pm_enable_uart1(void)
{
	// Put both rx and tx to output
	LOG_INF("We try to set pins TX: %d and RX: %d to output", NRF_UARTE1->PSEL.TXD,
		NRF_UARTE1->PSEL.RXD);
	uart_pm_pin_to_output(NRF_UARTE1->PSEL.TXD);
	uart_pm_pin_to_output(NRF_UARTE1->PSEL.RXD);

	NRF_UARTE1->EVENTS_ENDTX = 0;
	NRF_UARTE1->EVENTS_TXDRDY = 0;
	NRF_UARTE1->EVENTS_TXSTARTED = 0;

	NRF_UARTE1->ENABLE = 0x00000008;
	NRF_UARTE1->TASKS_STARTTX = 1;
	NRF_UARTE1->TASKS_STARTRX = 1;

	NRF_UARTE1->ENABLE = 0x00000008;
	NRF_UARTE1->TASKS_STARTTX = 1;
	NRF_UARTE1->TASKS_STARTRX = 1;

	k_sleep(K_MSEC(500));
}

/** END PRIVATE FUNCTIONS **/

/** PUBLIC FUNCTIONS **/

int uart_pm_disable(const char *name)
{
	if (uart_dev0) {
		if (strcmp(name, uart_dev0->name) == 0) {
			LOG_INF("We need to disable UART0.");
			uart_pm_disable_uart0();
			return 0;
		}
	}
	if (uart_dev1) {
		if (strcmp(name, uart_dev1->name) == 0) {
			LOG_INF("We need to disable UART1.");
			uart_pm_disable_uart1();
			return 0;
		}
	}

	return -EIO;
}

int uart_pm_enable(const char *name)
{
	if (uart_dev0) {
		if (strcmp(name, uart_dev0->name) == 0) {
			LOG_INF("We need to enable UART0.");
			uart_pm_enable_uart0();
			return 0;
		}
	}

	if (uart_dev1) {
		if (strcmp(name, uart_dev1->name) == 0) {
			LOG_INF("We need to enable UART1.");
			uart_pm_enable_uart1();
			return 0;
		}
	}

	return -EIO;
}
