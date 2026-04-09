/*
 * Copyright (c) 2020 Irnas d.o.o.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/console/console.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>

#ifdef CONFIG_UART_CONSOLE
#include <zephyr/shell/shell_uart.h>
#endif // CONFIG_UART_CONSOLE

#ifdef CONFIG_RTT_CONSOLE
#include <zephyr/shell/shell_rtt.h>
#endif // CONFIG_RTT_CONSOLE

static int cmd_test1(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_echo_set(shell, false);
	shell_print(shell, "Test 1");

	return 0;
}

static int cmd_test2(const struct shell *shell, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(shell, "Test 2");

	return 0;
}

void main(void)
{
	printk("Shell example\n");

#ifdef CONFIG_UART_CONSOLE
	shell_echo_set(shell_backend_uart_get_ptr(), false);
#endif // CONFIG_UART_CONSOLE
#ifdef CONFIG_RTT_CONSOLE
	shell_echo_set(shell_backend_rtt_get_ptr(), false);
#endif // CONFIG_RTT_CONSOLE
}

SHELL_CMD_REGISTER(TEST1, NULL, "Shell test 1", cmd_test1);
SHELL_CMD_REGISTER(TEST2, NULL, "Shell test 2", cmd_test2);
