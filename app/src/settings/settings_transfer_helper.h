/** @file settings_transfer_helper.h
 *
 * @brief Settings transfer helper functions for transferring old settings to new settings
 * structure.
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2026 Irnas. All rights reserved.
 */

#ifndef SETTINGS_TRANSFER_HELPER_H
#define SETTINGS_TRANSFER_HELPER_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Transfers legacy user settings to the family-based settings structure.
 *
 * A legacy entry is deleted only after its new entry has been written and verified. Runtime-value
 * mappings are retained for documentation but are not transferred because values were not persisted
 * under their protocol IDs.
 *
 * @retval 0 One or more legacy settings were transferred successfully.
 * @retval -EALREADY No legacy settings were present.
 * @return A different negative error code if one or more entries failed to transfer.
 */
int settings_transfer(void);

#ifdef __cplusplus
}
#endif

#endif /* SETTINGS_TRANSFER_HELPER_H */
