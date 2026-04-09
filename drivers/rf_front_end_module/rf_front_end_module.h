/**
 * @file rf-front-end-module.h
 * @brief
 *
 * @copyright (c) 2023 Irnas. All rights reserved.
 *
 */

#ifndef RF_FRONT_END_H
#define RF_FRONT_END_H

#include <zephyr/kernel.h>

/**
 * @brief Front-end module operating modes enumeration
 */
enum rf_front_end_mode {
	RF_FRONT_END_MODE_SLEEP,
	RF_FRONT_END_MODE_BYPASS,
	RF_FRONT_END_MODE_RX_LNA,
	RF_FRONT_END_MODE_TX,
};

/**
 * @brief Antenna path selection enumeration
 */
enum rf_front_end_antenna_path {
	RF_FRONT_END_ANTENNA_PATH_1 = 0, /* ANT1 */
	RF_FRONT_END_ANTENNA_PATH_2 = 1, /* ANT2 */
};

/**
 * @brief Init front-end module.
 *
 * Configure gpios as outputs and set them to inactive. Set module to sleep mode.
 *
 * @return int 0 if successful, negative errno code passed from gpio_pin_set_dt or
 * gpio_pin_configure_dt if failure.
 */
int rf_front_end_module_init(void);

/**
 * @brief Change front-end module mode.
 *
 * @param[in] mode enumerated front end mode
 *
 * @retval 0 if successful.
 * @retval negative errno code passed from gpio_pin_set_dt if failure.
 * @retval -EINVAL if invalid mode is selected.
 */
int rf_front_end_module_set_mode(enum rf_front_end_mode mode);

/**
 * @brief Get current front-end module mode.
 *
 * @return enum rf_front_end_mode
 */
enum rf_front_end_mode rf_front_end_module_get_mode(void);

/**
 * @brief Set the input/output antenna path.
 *
 * Even though we already set the antenna path in set_mode function, this function can be used to
 * change the antenna path at any time.
 *
 * @param[in] antenna_path:
 * 	- RF_FRONT_END_ANTENNA_PATH_1, for ant1 antenna path
 *	- RF_FRONT_END_ANTENNA_PATH_2, for ant2 antenna path.
 *
 * @return int 0 if successful, negative errno code passed from gpio_pin_set_dt if failure.
 */
int rf_front_end_module_set_ant_path(enum rf_front_end_antenna_path antenna_path);

#endif /* RF_FRONT_END_H */
