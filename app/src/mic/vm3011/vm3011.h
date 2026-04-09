/*
 * Copyright (c) 2017 IpTronix S.r.l.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __VM3011_H__
#define __VM3011_H__

#include "nrfx_pdm.h"
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/types.h>

// DATASHEET: https://media.digikey.com/pdf/Data%20Sheets/Vesper%20Tech/VM3011_DS.pdf

/**
 * @brief Handler that is triggered when DOUT pin is set high
 *
 */
typedef void (*vm3011_dout_handler_t)(const struct device *dev);

/**
 * @brief Clear the dout pin from the vm3011 microphone
 *
 * Use this function after the pin goes high and you have taken
 * the samples you have wanted to take from it.
 *
 * @param dev The microphone device
 * @return int 0 on success, negative error code otherwise
 */
int vm3011_clear_dout(const struct device *dev);

/**
 * @brief Configure the watchdog timer on vm3011
 *
 * Enable the Watch Dog Timer to disable the I2C logic
 * in the rare case when the host stops toggling the SCL
 * clock during an I2C transaction.
 *
 * @param dev The microphone device
 * @param wdt_dly The delay in ms (one of: 8, 16, 32, 64)
 * @param enable True to enable, false to disable
 * @return int 0 on success, negative error code otherwise
 */
int vm3011_wdt_configure(const struct device *dev, uint8_t wdt_dly, bool enable);

/**
 * @brief Set the WOS PGA Gain threshold minimum
 *
 * Max_PGA_Gain and Min_PGA_Gain limits are the maximum and minimum allowable gains of the WoS PGA,
 * which define the boundary for the control loop. The PGA Gain would not go higher or lower than
 * the values set in the registers when the ZPL adaptive threshold is being tracked. The bitfields
 * on these registers can be set to any value between ‘0000’ and ‘11111’ corresponding to threshold
 * range between 45 - 91.5 dBSPL as per the Table given in WOS_PGA_GAIN register above
 *
 * @param dev The microphone device
 * @param pga_gain_db The db gain to set (value from 45 to 91.5 in steps of 1.5)
 * @return int 0 on success, negative error code otherwise
 */
int vm3011_wos_pga_min_the_set(const struct device *dev, float pga_gain_db);

/**
 * @brief Set the WOS PGA Gain threshold maximum
 *
 * Max_PGA_Gain and Min_PGA_Gain limits are the maximum and minimum allowable gains of the WoS PGA,
 * which define the boundary for the control loop. The PGA Gain would not go higher or lower than
 * the values set in the registers when the ZPL adaptive threshold is being tracked. The bitfields
 * on these registers can be set to any value between ‘0000’ and ‘11111’ corresponding to threshold
 * range between 45 - 91.5 dBSPL as per the Table given in WOS_PGA_GAIN register above
 *
 * @param dev The microphone device
 * @param pga_gain_db The db gain to set (value from 45 to 91.5 in steps of 1.5)
 * @return int 0 on success, negative error code otherwise
 */
int vm3011_wos_pga_max_the_set(const struct device *dev, float pga_gain_db);

/**
 * @brief Set band pass filter nim and max frequency
 *
 * @param dev The microphone device
 * @param wos_lpf_freq The low pass frequency (0 -2 kHz, 1 - 4 kHz, 2 - 6 kHz, 3 - 8 kHz)
 * @param wos_hpf_freq The high pass frequency (0 -200 kHz, 1 - 300 kHz, 2 - 400 kHz, 3 - 800 kHz)
 * @return int 0 on success, negative error code otherwise
 */
int vm3011_bpf_set(const struct device *dev, uint8_t wos_lpf_freq, uint8_t wos_hpf_freq);

/**
 * @brief Set the number of fast mode counts
 *
 * FAST_MODE_COUNT can be programmed to increase the speed at which the ZPL feedback loop adapts to
 * a large change in background noise. The FAST MODE is triggered according to the FAST_MODE_CNT
 * setting described in the table below. For example, when FAST_MODE_CNT[1:0]=01, if the PGA Gain is
 * incremented two times in a row or decremented two times in a row, the FAST MODE will engage.
 *
 * @param dev The microphone device
 * @param fast_mode_cnt The fast mode count to set:
 *                      0 - disabled
 *                      1 - If two window comparator trips in a row in the same direction, the
 * clocks are sped up 16x 2 - If four window comparator trips in a row in the same direction, the
 * clocks are sped up 16x 3 - If six window comparator trips in a row in the same direction, the
 * clocks are sped up 16x
 * @return int 0 on success, negative error code otherwise
 */
int vm3011_fast_mode_cnt_set(const struct device *dev, uint8_t fast_mode_cnt);

/**
 * @brief Set the RMS switch
 *
 * WOS_RMS can be set to Low/High to switch the sampling interval of the comparator signal between 1
 * seconds and 0.5 seconds. This effectively changes the low pass corner frequency from 1Hz to 2Hz.
 *
 * @param dev The microphone device
 * @param value The value to set:
 *              0 - interval is 1 second
 *              1 - interval is 0.5 seconds
 * @return int 0 on success, negative error code otherwise
 */
int vm3011_wos_rms_set(const struct device *dev, uint8_t value);

/**
 * @brief
 *
 * WOS_THRESH can be used to program the margin to the threshold from 6 dBSPL to 18 dBSPL. The table
 *below outlines the different DOUT threshold values available. Essentially, this programs the
 *amount of margin above the average acoustic input level that is needed to trip the DOUT
 *comparator. For example, a code of 100 for WOS_THRESH will program the microphone to trigger at a
 *level 5x above the average acoustic noise level.
 *
 * @param dev The microphone device
 * @param wos_thresh The threshold to set:
 *                   1 -- 2x -- 6.0 dBSPL
 *	                 2 -- 3x -- 9.5 dBSPL
 *	                 3 -- 4x -- 12.0 dBSPL
 *	                 4 -- 5x -- 14.0 dBSPL
 *	                 5 -- 6x -- 15.5 dBSPL
 *	                 6 -- 7x -- 16.9 dBSPL
 *	                 7 -- 8x -- 18.0 dBSPL
 * @return int 0 on success, negative error code otherwise
 */
int vm3011_wos_dout_thresh_set(const struct device *dev, uint8_t wos_thresh);

/**
 * @brief Get the ambient sound level from the vm3011
 *
 * @param dev The microphone device
 * @param[out] amb_sound_lvl The average ambient sound level (in db)
 * @return int 0 on success, negative error code otherwise
 */
int vm3011_amb_sound_lvl_get(const struct device *dev, float *amb_sound_lvl);

#if defined(CONFIG_VM3011_INT)
/**
 * @brief Read the current state of the dout pin
 *
 * @param dev The microphone device
 * @return int The state of the pin (0 or 1),
 *         or a negative error code otherwise
 */
int vm3011_dout_get(const struct device *dev);

/**
 * @brief Register a dout handler
 *
 * @param dev The microphone device
 * @param handler the handler to be registered
 */
void vm3011_dout_set_handler(const struct device *dev, vm3011_dout_handler_t handler);

#endif

#endif /* __VM3011_H__ */
