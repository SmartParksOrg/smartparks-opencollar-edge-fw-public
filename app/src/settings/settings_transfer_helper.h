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
 * Numeric and boolean legacy values must satisfy the destination setting's min/max limits.
 * Out-of-range values are replaced with the compiled default. Legacy IDs 0x38 and 0x60 were reused
 * for unrelated settings and are always replaced with the compiled default when present, even if
 * their length or value would otherwise be accepted. Byte arrays are checked for length only.
 *
 * Transferred values and replacement defaults are written and verified before deleting the
 * legacy entry. Other entries with incompatible lengths are discarded without changing their
 * destinations. Once a legacy entry is deleted, later boots leave its destination unchanged.
 * Runtime-value mappings are not processed because values were not persisted at their protocol IDs.
 *
 * @retval 0 One or more legacy settings were transferred, defaulted, or discarded with no failures.
 * @retval -EALREADY No legacy settings were present.
 * @return A different negative error code if one or more entries could not be migrated or deleted.
 */
int settings_transfer(void);

#ifdef __cplusplus
}
#endif

#endif /* SETTINGS_TRANSFER_HELPER_H */
