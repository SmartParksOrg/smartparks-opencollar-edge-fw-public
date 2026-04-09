/** @file lp0_common.h
 *
 * @brief LP0 common definitions.
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2026 Irnas.  All rights reserved.
 */

#ifndef LP0_COMMON_H
#define LP0_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

enum lp0_mode {
	/* LP0 General modes */
	LP0_MODE_DISABLED = 0,
	LP0_MODE_SETUP = 1,
	LP0_MODE_DEFAULT = 2,
	/* LP0 Offload modes */
	LP0_MODE_DEVICE_DISCOVERY = 3,
	LP0_MODE_DEVICE_DATA_TRANSFER = 4,
	LP0_MODE_OFFLOAD_STATION = 5,
};

/* Time in ms to start RX windows before expected message arrival to account for synchronization
 * issues, and time to elongate RX windows by to account for synchronization issues. */
#define LP0_RX_WINDOW_SYNC_TIME_MS 5

#ifdef __cplusplus
}
#endif

#endif /* LP0_COMMON_H */
