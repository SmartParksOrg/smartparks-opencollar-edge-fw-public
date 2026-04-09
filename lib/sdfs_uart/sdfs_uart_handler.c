/** @file sdfs_uart_handler.c
 *
 * @brief SDFS UART handler library
 *
 * SD card File system over uart handling library.
 *
 * NOTE: Only one instance of SDFS UART is supported at a time. By default, the library uses uart
 * polling functions for communication.
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2025 Irnas. All rights reserved.
 */

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>

#include <uart_pm.h>

#include <sdfs_uart_handler.h>

#define DT_DRV_COMPAT irnas_ak_sdfs_uart

/* Maximum number of iterative SDFS module checks during which we need to get at least one
 * response */
#define SDFS_UART_MODULE_CHECK_MAX_REPETITIONS 3

LOG_MODULE_REGISTER(sdfs_uart_handler);

static struct sdfs_uart_interface *prv_sdfs_uart_interface;

/* --- Private function declarations --- */

static void prv_sdfs_uart_resume_write(struct k_work *work);

/* --- Private data --- */

K_SEM_DEFINE(prv_pause_write_sem, 0, 1);
K_WORK_DELAYABLE_DEFINE(prv_resume_write_work, prv_sdfs_uart_resume_write);

/* --- Private function implementation --- */

/**
 * @brief Resume write work handler.
 *
 * @param[in] work Pointer to the work item.
 */
static void prv_sdfs_uart_resume_write(struct k_work *work)
{
	ARG_UNUSED(work);
	k_sem_give(&prv_pause_write_sem);
}

/**
 * @brief Receive function for fprot library.
 *
 * @param[out] data Pointer to the buffer to store received data.
 * @param[in] data_len Length of the buffer.
 *
 * @return Number of bytes received.
 */
static unsigned long prv_sdfs_uart_fprot_rx(unsigned char *data, unsigned long data_len)
{
	/* The fprot library does RX only 1 byte at a time. When the need arises we'll add support
	 * for multiple byte rx reading. Assert until then. */
	__ASSERT(data_len == 1,
		 "Data length must be 1 byte for my_fprot_rx! Multiple byte rx is not supported.");

	static size_t rx_received = 0;
	int ret = uart_poll_in(prv_sdfs_uart_interface->uart_dev, &data[rx_received]);
	if (ret == 0) {
		return 1;
	} else {
		return 0;
	}
}

/**
 * @brief Transmit function for fprot library.
 *
 * @param[in] data Pointer to the data to transmit.
 * @param[in] data_len Length of the data to transmit.
 */
static void prv_sdfs_uart_fprot_tx(unsigned char *data, unsigned long data_len)
{
	for (unsigned long i = 0; i < data_len; i++) {
		uart_poll_out(prv_sdfs_uart_interface->uart_dev,
			      data[i]); /* blocks until char sent */
	}
}

/**
 * @brief Default selected delay function for fprot library.
 *
 * You can provide your own delay function when initializing the SDFS UART library via
 * sdfs_uart_init().
 *
 * @param[in] ms Milliseconds to delay.
 */
static void prv_sdfs_uart_fprot_delay(unsigned long ms)
{
	k_sleep(K_MSEC(ms));
}

int sdfs_uart_init(const struct device *dev)
{
	struct sdfs_uart_interface *interface = dev->data;
	prv_sdfs_uart_interface = interface;

	if (!device_is_ready(prv_sdfs_uart_interface->uart_dev)) {
		LOG_ERR("UART device not ready");
		return -ENODEV;
	}

	/* Default uart settings for the AK-SDFS-UART SD card reader */
	struct uart_config cfg = {
		.baudrate = 9600,
		.parity = UART_CFG_PARITY_NONE,
		.stop_bits = UART_CFG_STOP_BITS_1,
		.data_bits = UART_CFG_DATA_BITS_8,
		.flow_ctrl = UART_CFG_FLOW_CTRL_NONE,
	};

	int err = uart_configure(prv_sdfs_uart_interface->uart_dev, &cfg);
	if (err) {
		LOG_ERR("Failed to configure UART: %d", err);
		return err;
	}

	int ret = sdfs_uart_manual_init(prv_sdfs_uart_fprot_tx, prv_sdfs_uart_fprot_rx,
					prv_sdfs_uart_fprot_delay,
					&prv_sdfs_uart_interface->reset_pin);

	switch (ret) {
	case FPROT_NO_ERROR:
		LOG_INF("Init SDFS UART module done, SD card detected");
		break;
	case FPROT_NO_CARD:
		LOG_WRN("Init SDFS UART module done, no SD card detected");
		return 0; /* Module initialized successfully, SD card is just not present. Not an
			     error for the init function. */
		break;
	case FPROT_RX_TIMEOUT:
		LOG_ERR("Init SDFS UART module failed, no response from module");
		return -ETIMEDOUT;
	case -EINVAL:
		LOG_ERR("Init SDFS UART module failed, invalid input parameters");
		break;
	default:
		LOG_ERR("Init SDFS UART module failed with error: %d (fprot error code)", ret);
		return ret;
	}

	return ret;
}

int sdfs_uart_manual_init(txdatafunc *tx, rxdatafunc *rx, delayfunc *delay,
			  const struct gpio_dt_spec *reset_pin)
{
	if (tx == NULL || rx == NULL || delay == NULL || reset_pin == NULL) {
		LOG_ERR("sdfs_uart_manual_init: one or more input parameters are NULL");
		return -EINVAL;
	}

	/* Configure reset pin */
	prv_sdfs_uart_interface->reset_pin = *reset_pin;
	gpio_pin_configure_dt(&prv_sdfs_uart_interface->reset_pin,
			      GPIO_OUTPUT_INACTIVE | GPIO_ACTIVE_LOW);

	int err = uart_pm_enable(prv_sdfs_uart_interface->uart_dev->name);
	if (err) {
		LOG_ERR("Failed to enable UART PM: %d", err);
		return err;
	}

	/* Sleeping to allow proper pin initialization */
	k_sleep(K_MSEC(10));

	/* Link functions with fprot */
	int ret = 0;
	ret = fprot_init(tx, rx, delay);
	if (ret != FPROT_NO_ERROR) {
		LOG_ERR("fprot_init failed: %d (fprot error code)", ret);
		return -EINVAL;
	}

	/* Perform transaction X times to see if the module is operational */
	for (int i = 0; i < SDFS_UART_MODULE_CHECK_MAX_REPETITIONS; i++) {
		ret = fprot_check();
		if (ret != FPROT_NO_ERROR) {
			if (ret == FPROT_NO_CARD) {
				LOG_WRN("SDFS module initialized. No SD Card present! (Init try: "
					"%d/%d)",
					i + 1, SDFS_UART_MODULE_CHECK_MAX_REPETITIONS);
			} else {
				LOG_INF("SDFS module NOT detected");
			}
		} else {
			LOG_INF("SDFS module initialized, SD card detected.");
			break;
		}
	}

	/* Disable UART PM to save power, while it's not in use */
	err = uart_pm_disable(prv_sdfs_uart_interface->uart_dev->name);
	if (err) {
		LOG_ERR("Failed to disable UART PM: %d", err);
		return err;
	}

	/* Release the write semaphore to allow writing */
	k_sem_give(&prv_pause_write_sem);

	return ret;
}

unsigned char sdfs_uart_set_time_from_global_unix_timestamp(const time_t *unix_timestamp)
{
	struct tm tm_time;
	unsigned char ret;
	gmtime_r(unix_timestamp, &tm_time);

	k_sem_take(&prv_pause_write_sem, K_FOREVER);

	ret = fprot_set_time((unsigned char)tm_time.tm_mday, (unsigned char)(tm_time.tm_mon + 1),
			     (unsigned char)(tm_time.tm_year - 100), (unsigned char)tm_time.tm_hour,
			     (unsigned char)tm_time.tm_min, (unsigned char)tm_time.tm_sec);

	k_sem_give(&prv_pause_write_sem);

	if (ret != FPROT_NO_ERROR) {
		LOG_ERR("Failed to set time from unix timestamp: %d (fprot error code)", ret);
		return -ret;
	}

	return ret;
}

int sdfs_uart_file_append(char *file_name, void *buf, size_t len)
{
	k_sem_take(&prv_pause_write_sem, K_FOREVER);

	int ret = fprot_check();
	if (ret != FPROT_NO_ERROR) {
		if (ret == FPROT_NO_CARD) {
			LOG_ERR("No SD Card present!");
		} else {
			LOG_ERR("Failed to check SD Card presence: %d (fprot error code)", ret);
		}
		goto write_error_exit;
	} else {
		LOG_DBG("SD Card present");
	}

	FPROT_FILE file;
	unsigned char options = FPROT_MODE_OPEN_ALWAYS | FPROT_MODE_W;
	ret = fprot_open(file_name, options, &file);
	if (ret != FPROT_NO_ERROR) {
		LOG_ERR("Failed to open file %s: %d (fprot error code)", file_name, ret);
		goto write_error_exit;
	}

	unsigned long capacity, free;
	ret = fprot_fat_info(&capacity, &free);
	if (ret != FPROT_NO_ERROR) {
		LOG_ERR("Failed to get FAT info: %d (fprot error code)", ret);
		goto write_error_exit;
	} else {
		LOG_DBG("SD Card capacity: %lu, free: %lu", capacity, free);
	}

	unsigned long pos, size;
	ret = fprot_file_info(file, &pos, &size);
	if (ret != FPROT_NO_ERROR) {
		LOG_ERR("Failed to get file info: %d (fprot error code)", ret);
		goto write_error_exit;
	} else {
		LOG_DBG("File size: %lu, pos: %lu", size, pos);
	}

	unsigned long new_pos = 0;
	ret = fprot_seek(file, size, &new_pos);
	if (ret != FPROT_NO_ERROR) {
		LOG_ERR("Failed to seek to end of file: %d (fprot error code)", ret);
		goto write_error_exit;
	} else {
		LOG_DBG("Sought position: %lu", new_pos);
	}

	unsigned long written_len = 0;
	ret = fprot_write(file, buf, len, &written_len);
	if (ret != FPROT_NO_ERROR) {
		LOG_ERR("Failed to write data to file %s: %d (fprot error code)", file_name, ret);
		goto write_error_exit;
	}

	/* Close the file to save the newly written data */
	ret = fprot_close(file);
	if (ret != FPROT_NO_ERROR) {
		LOG_ERR("Failed to close file %s: %d (fprot error code)", file_name, ret);
		goto write_error_exit;
	}

	k_sleep(K_MSEC(10));
	k_sem_give(&prv_pause_write_sem);
	return written_len;

	/* If an error occurred during write operation */
write_error_exit:
	k_sleep(K_MSEC(10));
	k_sem_give(&prv_pause_write_sem);
	return -ret;
}

int sdfs_uart_reset(void)
{
	int err = gpio_pin_set_dt(&prv_sdfs_uart_interface->reset_pin, 1);
	if (err) {
		LOG_ERR("Failed to set reset pin: %d", err);
		return err;
	}

	k_msleep(10); /* Sleep for a minimum of 5 ms */

	err = gpio_pin_set_dt(&prv_sdfs_uart_interface->reset_pin, 0);
	if (err) {
		LOG_ERR("Failed to unset reset pin: %d", err);
		return err;
	}

	return err;
}

int sdfs_uart_pause_write(k_timeout_t delay)
{
	int err = k_sem_take(&prv_pause_write_sem, K_FOREVER);
	if (err) {
		LOG_ERR("Failed to take pause_write semaphore: %d", err);
		return err;
	}

	err = k_work_schedule(&prv_resume_write_work, delay);
	if (err < 0) {
		LOG_ERR("Failed to schedule resume write work: %d", err);
		k_sem_give(&prv_pause_write_sem);
		return err;
	}

	return 0;
}

int sdfs_uart_pm_enable(void)
{
	int err = uart_pm_enable(prv_sdfs_uart_interface->uart_dev->name);
	return err;
}

int sdfs_uart_pm_disable(void)
{
	int err = uart_pm_disable(prv_sdfs_uart_interface->uart_dev->name);
	return err;
}

/**
 * @brief Dummy init function to be used with DEVICE_DT_DEFINE for initializing the SDFS UART module
 * device instance.
 *
 * This is done so we can use the device instance to store the sdfs_uart_interface struct with the
 * uart device and reset pin, which we need for the actual initialization of the SDFS UART module
 * in sdfs_uart_init.
 *
 * Instead of doing actual initialization in this function, we do it in sdfs_uart_init, which is
 * called manually in the application after the device is ready. This reduces boot time.
 */
__unused static int prv_sdfs_uart_dummy_init(const struct device *dev)
{
	/* Do nothing except disable UART PM */
	struct sdfs_uart_interface *iface = dev->data;
	uart_pm_disable(iface->uart_dev->name);
	return 0;
}

BUILD_ASSERT(CONFIG_SDFS_UART_MODULE_INIT_PRIORITY > CONFIG_PERIPHERAL_POWER_INIT_PRIORITY,
	     "SDFS UART module init priority must be higher than peripheral power init "
	     "priority");

#define SDFS_DEFINE(inst)                                                                          \
	static struct sdfs_uart_interface prv_sdfs_uart_interface_##inst = {                       \
		.uart_dev = DEVICE_DT_GET(DT_BUS(DT_INST(inst, DT_DRV_COMPAT))),                   \
		.reset_pin = GPIO_DT_SPEC_INST_GET(inst, reset_gpios),                             \
	};                                                                                         \
	DEVICE_DT_DEFINE(DT_INST(inst, DT_DRV_COMPAT), prv_sdfs_uart_dummy_init, NULL,             \
			 &prv_sdfs_uart_interface_##inst, NULL, POST_KERNEL,                       \
			 CONFIG_SDFS_UART_MODULE_INIT_PRIORITY, NULL)

DT_INST_FOREACH_STATUS_OKAY(SDFS_DEFINE);
