/** @file satellite.h
 *
 * @brief Interface for satellite communication.
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2022 Irnas. All rights reserved.
 */
#ifndef SATELLITE_H
#define SATELLITE_H

#include <zephyr/kernel.h>
#include <zephyr/types.h>

/**
 * @brief Init satellite communication module. Configure UART communication.
 *
 * @return int - 0 on success, negative error code otherwise.
 */
int satellite_init(void);

/**
 * @brief Get current time interval for satellite communication.
 *
 * @return interval one (1) or two (2).
 */
uint8_t satellite_get_current_time_interval(void);

/**
 * @brief Check if satellite module is successfully initialized, supported for HW type and
 * enabled by the user.
 *
 * @return true
 * @return false
 */
bool satellite_is_supported(void);

/**
 * @brief Enable satellite module. Turn on GPIO and enable UART.
 *
 * @return int - 0 on success, negative error code otherwise.
 */
int satellite_enable(void);

/**
 * @brief Disable satellite module. Disable UART and turn off GPIO.
 *
 * @return int - 0 on success, negative error code otherwise.
 */
int satellite_disable(void);

/**
 * @brief Function to handle commands. Wait indefinitely for command and do not proceed beforehand.
 *
 */
void satellite_handle_commands(void);

/**
 * @brief Add new message to queue buffer. If message is to long, send buffer and reset it.
 *
 * @param msg new message
 * @param msg_size message size
 */
void satellite_add_message_to_send_buffer(uint8_t *msg, int msg_size, uint8_t msg_port);

/**
 * @brief Send local buffer via satellite module. Reset index.
 *
 * @return int return 0 on success, negative error code otherwise
 * @retval -EIO - sending buffer failed or satellite transmission is not supported.
 * @retval -ENETUNREACH - satellite signal strength is lower than minimum allowed.
 */
int satellite_send_buffer(void);

bool satellite_test(void);

#endif // SATELLITE_H
