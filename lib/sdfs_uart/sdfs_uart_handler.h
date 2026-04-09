/** @file sdfs_uart_handler.h
 *
 * @brief SDFS UART handler library
 *
 * SD card File system over uart handling library.
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2025 Irnas.  All rights reserved.
 */

#ifndef SDFS_UART_HANDLER_H
#define SDFS_UART_HANDLER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

#include <fprot.h>

struct sdfs_uart_interface {
	struct gpio_dt_spec reset_pin;
	const struct device *uart_dev; /* Only used with auto initialization */
};

/**
 * @brief Simplified initialization function for SDFS UART interface.
 *
 * This function calls the sdfs_uart_manual_init() function with default parameters.
 *
 * @param[in] dev Pointer to the sdfs device structure.
 *
 * @retval 0 on success. Functions were linked, SDFS module was responsive.
 * @retval -ENODEV if the uart interface is not ready.
 * @retval -ETIMEDOUT if the SDFS module did not respond during initialization.
 * @retval -EINVAL if the input parameters for initialization were invalid.
 * @retval other positive error code passed from sdfs_uart_manual_init if initialization failed due
 * to other reasons (see sdfs_uart_manual_init for details on error codes).
 */
int sdfs_uart_init(const struct device *dev);

/**
 * @brief Initialize SDFS UART handler.
 *
 * Call this function before any other function of this library.
 * Passes the UART TX, RX and delay functions to the fprot library, configures the reset pin
 * GPIO.
 *
 * @param [in] tx - pointer to uart TX function
 * @param [in] rx - pointer to uart RX function
 * @param [in] delay - pointer to delay function
 * @param [in] reset_pin - pointer to GPIO DT spec struct for the SDFS UART reset pin
 *
 * @retval -EINVAL One or more input parameters are NULL.
 * @retval 0 Success. Functions were linked, SDFS module was responsive and SD card was detected.
 * @retval FPROT_NO_CARD (17) Functions were linked, SDFS module was responsive but no SD
 * card was detected.
 * @retval FPROT_RX_TIMEOUT (30) Functions were linked but SDFS module did not respond.
 * @retval other positive error code passed from fprot_check if check failed due
 * to other reasons (see fprot_check for details).
 */
int sdfs_uart_manual_init(txdatafunc *tx, rxdatafunc *rx, delayfunc *delay,
			  const struct gpio_dt_spec *reset_pin);

/**
 * @brief Write data to SDFS UART (Appends data to the EOF only!)
 *
 * Perform the full writing process:
 * 1. Check if the SD card is present
 * 2. File open
 * 3. Check the available size
 * 4. Seek out the EOF
 * 5. Write the provided @a buf at the sought out location
 * 6. Close the file again.
 *
 * @param [in] file_name - File Name. Use this if you want to write to a
 * file with the specified name. If a file with this names does not exist, it will be
 * created. If file name is NULL, the default file name "log.txt" will be used.
 * @param [in] buf - data buffer
 * @param [in] len - data length (number of characters to write)
 *
 * @retval int 	- if positive, number of bytes written
 * @retval int 	- if negative, error code from fprot library (normal enumeration, negated)
 */
int sdfs_uart_file_append(char *file_name, void *buf, size_t len);

/**
 * @brief Reset the card reader.
 *
 * The card reader is reset by setting its respected reset pin (sdfs_uart_reset alias) for more than
 * 5 ms.
 *
 * @return int - 0 if success, negative error code passed from gpio_pin_set otherwise
 */
int sdfs_uart_reset(void);

/*!
 * @brief Helper function for setting the internal AK-SDFS-UART device date and time from a global
 * unix timestamp.
 *
 * Call this function to configure the date and time to be used with the file system (when creating
 * or modifying files).
 *
 * Note that the AK-SDFS-UART board will lose the date and time after power down or reset.
 *
 * @param unix_timestamp pointer to a Unix timestamp variable.
 *
 * @return FPROT_NO_ERROR(0) on success, otherwise an fprot error code (passed from fprot_send_cmd).
 */
unsigned char sdfs_uart_set_time_from_global_unix_timestamp(const time_t *unix_timestamp);

/**
 * @brief Pause SDFS UART write operations for the provided duration.
 *
 * This function pauses write operations by taking a semaphore. The write operations are resumed
 * after the specified delay using a delayed work item.
 *
 * @param [in] delay - duration to pause write operations
 *
 * @retval int 0 if success
 * @retval int negative error code if failed to take semaphore or schedule work (originates from
 * k_sem_take or k_work_schedule)
 */
int sdfs_uart_pause_write(k_timeout_t delay);

/**
 * @brief Enable uart power for the SDFS UART module.
 *
 * This function needs to be called before write operations to ensure proper UART communication, as
 * the SDFS UART library disables UART PM after initialization, to reduce power consumption.
 */
int sdfs_uart_pm_enable(void);

/**
 * @brief Disable uart power for the SDFS UART module.
 *
 * This function can be used to disable uart power. This is useful to save power when the SDFS UART
 * module is not in use, but it needs to be enabled again before write operations.
 */
int sdfs_uart_pm_disable(void);

#ifdef __cplusplus
}
#endif

#endif /* SDFS_UART_HANDLER_H */
