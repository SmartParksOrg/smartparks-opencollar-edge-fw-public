/** @file flash_ext_partitions.h
 *
 * @brief Header file for flash external partitions.
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2025 Irnas.  All rights reserved.
 */

#ifndef FLASH_EXT_PARTITIONS_H
#define FLASH_EXT_PARTITIONS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <zephyr/drivers/flash.h>

enum flash_ext_partition {
	FLASH_EXT_PARTITION_MESSAGE_STORAGE = 0,
};

/**
 * @brief Check external flash partitions size and save it.
 *
 * @return int 0 on success, negative error code on failure.
 */
int flash_ext_partitions_init(void);

/**
 * @brief Read flash chip data and calculate size.
 *
 * This function assumes that all flash chip pages are the same size.
 * This function can be called independently of flash_ext_partitions_init().
 *
 * @return on success, calculated size of the flash chip in bytes.
 * @return on fail, negative error code.
 */
long flash_ext_get_size(void);

/**
 * @brief Get the start address of a specific partition in the external flash.
 *
 * @param[in] partition enum flash_ext_partition
 * @param[out] partition start offset
 * @return 0 on success, negative error code on failure.
 */
int flash_ext_get_partition_start_addr(enum flash_ext_partition partition, off_t *offset);

/**
 * @brief Get the size of a specific partition in the external flash.
 *
 * @param[in] partition enum flash_ext_partition
 * @param[out] size pointer to store the size of the partition
 *
 * @retval 0 on success,
 * @retval -EINVAL when non-existent partition is selected.
 */
int flash_ext_get_partition_size(enum flash_ext_partition partition, int *size);

#ifdef __cplusplus
}
#endif

#endif /* FLASH_EXT_PARTITIONS_H */
