/** @file external_switch.h
 *
 * @brief This module handles support for external switch sensors.
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2025 Irnas.  All rights reserved.
 */

#ifndef EXTERNAL_SWITCH_H
#define EXTERNAL_SWITCH_H

#ifdef __cplusplus
extern "C" {
#endif

#include <zephyr/kernel.h>

enum external_switch_state {
	EXTERNAL_SWITCH_INACTIVE = 0,
	EXTERNAL_SWITCH_ACTIVE = 1,
};

/**
 * @brief Initialize external switch sensor.
 * IMPORTANT: The external switch detection can only be used mutually
 * exclusive with fence detection. If both are enabled, the fence detection
 * will be used as well as an error will be logged.
 *
 * @retval 0 on success, negative error code otherwise.
 * @retval -EALREADY if the external switch is already initialized.
 */
int external_switch_init(void);

/**
 * @brief De-initialize external switch sensor. Set prv_external_switch_initialized to false.
 *
 * @return 0 on success, negative error code otherwise.
 */
int external_switch_deinit(void);

/**
 * @brief Check if the pin configuration of the external switch GPIO and input pins match the
 * user settings configuration.
 *
 * @return 0 if the pin configurations are correct, negative error code otherwise.
 */
int external_switch_check_pin_configuration(void);

/**
 * @brief Check if external switch is active.
 *
 * @param[out] active Pointer to a boolean variable that will be set to true if the
 * external switch is active, false otherwise.
 * @param[out] duration Pointer to a variable that will be set to the duration the switch
 * has been active for in milliseconds.
 *
 * @retval -EINVAL if the active pointer is NULL.
 * @retval negative error code if failed.
 * @retval 0 if successful.
 */
int external_switch_active(enum external_switch_state *active, uint32_t *duration);

/**
 * @brief Get the number of impulses detected by the external switch.
 *
 * @retval The number of impulses detected in last interval.
 */
uint32_t external_switch_get_impulse_count(void);

/**
 * @brief Check if we need to report the external switch state.
 *
 * @param[out] send_report Pointer to a boolean variable that will be set to true if a
 * report should be sent, false otherwise.
 * @param[out] force_send Pointer to a boolean variable that will be set to true if the
 * report should be sent immediately, false otherwise.
 *
 * @retval -EINVAL if the send_report pointer is NULL.
 * @retval negative error code if failed.
 * @retval 0 if successful.
 */
int external_switch_send_report_check(bool *send_report, bool *force_send);

/**
 * @brief Set the last reported external switch state.
 *
 * @param state The external switch state that will be saved as the new last reported state.
 */
void external_switch_last_report_set(enum external_switch_state state);

#ifdef __cplusplus
}
#endif

#endif /* EXTERNAL_SWITCH_H */
