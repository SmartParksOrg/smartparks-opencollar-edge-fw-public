/** @file fence.h
 *
 * @brief Interface for fence module.
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2023 Irnas. All rights reserved.
 */
#ifndef FENCE_H
#define FENCE_H

#include <zephyr/kernel.h>
#include <zephyr/types.h>

/**
 * @brief Initialize fence module. Configures enable pin and ADC measurement for the channel. This
 * function can be called even if the fence module is not attached to the Ranger board.
 *
 * @retval 0 on success
 * @retval a value from gpio_pin_configure() on error
 * @retval -ENODEV if ADC device is not responsive
 * @retval A value from adc_channel_setup() or -ENOTSUP if information from
 * Devicetree is not valid.
 * @retval -EALREADY if the fence module is already initialized.
 */
int fence_init(void);

/**
 * @brief De-initialize fence module. Set prv_fence_initialized to false.
 *
 * @retval 0 on success
 * @retval -EALREADY if the fence module is already de-initialized.
 */
int fence_deinit(void);

/**
 * @brief Power on fence module. Wait for "no pulse" part of the signal and then perform
 * peak detection algorithm. Pulse data is copied to the output message together with its
 * length. Before exiting, module is powered off.
 *
 * @param[in] duration - measurement duration in seconds
 * @param[in] scaling - mV scaling factor in the form of factor * 10^5
 * @param[out] msg - data message
 * @param[out] msg_len - data message length
 *
 * @retval 0 on success
 * @retval -EIO - power on/off failed
 * @return int adc_read() error.
 */
int fence_measure(uint16_t duration, uint32_t scaling, uint8_t *msg, uint8_t *msg_len);

#endif // FENCE_H
