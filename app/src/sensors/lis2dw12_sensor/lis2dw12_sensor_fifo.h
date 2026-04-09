/** @file lis2dw12_sensor_fifo.h
 *
 * @brief LIS2DW12 sensor FIFO handling functions
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2025 Irnas.  All rights reserved.
 */

#ifndef LIS2DW12_SENSOR_FIFO_H
#define LIS2DW12_SENSOR_FIFO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lis2dw12_reg.h"

#include <lis2dw12_sensor.h>

/**
 * @brief Set the FIFO buffer mode.
 *
 * @param[in] mode FIFO mode to set. The modes are (enumerated as specified in the datasheet):
 *  0. Bypass,
 *  1. FIFO,
 *  3. Continuous to FIFO,
 *  4. Bypass to Continuous,
 *  6. Continuous.
 *
 */
void lis2dw12_sensor_set_fifo_mode(lis2dw12_fmode_t mode);

/**
 * @brief Read samples from the FIFO.
 *
 * This function reads all available samples from the FIFO buffer into the provided buffer. The
 * provided buffer must be large enough to hold the samples. If there are more samples available
 * than can fit in the buffer, an error will be returned.
 *
 * @param[in] dev The device to read from.
 * @param[out] buf The buffer to store the samples in.
 * @param[in] len The length of the buffer.
 *
 * @retval number of samples read,
 * @retval -ENODATA if no samples are available,
 * @retval -ENOMEM if the provided buffer is not large enough to hold all available samples,
 * @retval -EINVAL if the device is not initialized or the buffer is NULL,
 * @retval -EIO if an error occurred.
 */
int lis2dw12_sensor_fifo_fetch(const struct device *dev, struct sensor_value *buf, size_t len);

/**
 * @brief Check FIFO mode and set it to bypass-to-continuous if necessary.
 *
 * @return int 0 on success, negative error code on failure.
 */
int lis2dw12_sensor_fifo_check_mode(void);

/**
 * @brief Set trigger for FIFO watermark detection.
 *
 * @param[in] dev sensor device pointer
 * @param[in] handler trigger handler function
 * @return int 0 on success, negative error code on failure.
 */
int lis2dw12_fifo_watermark_trigger_set(const struct device *dev, sensor_trigger_handler_t handler);

#ifdef __cplusplus
}
#endif

#endif /* LIS2DW12_SENSOR_FIFO_H */
