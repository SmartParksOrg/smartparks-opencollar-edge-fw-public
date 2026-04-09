/** @file flash_ext_partitions.c
 *
 * @brief Header file for flash external partitions.
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2025 Irnas. All rights reserved.
 */

#include <flash_ext_partitions.h>

#include <zephyr/drivers/flash.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(flash_ext_partitions);

const struct device *flash_ext_dev = DEVICE_DT_GET(DT_ALIAS(external_flash));

static bool prv_flash_ext_initted = false;

struct flash_ext_partition_info {
	enum flash_ext_partition partition;
	off_t start_addr;
	size_t size;
};

/* External flash info */

struct flash_pages_info prv_flash_ext_page_info;

/* Flash partitions
 *
 * NOTE: The size of the last partition is set to the remaining size of the flash chip at runtime.
 *
 * 0x000000 - 0xFFFFFF: Message storage
 */
static struct flash_ext_partition_info flash_ext_partitions[] = {
	{
		.partition = FLASH_EXT_PARTITION_MESSAGE_STORAGE,
	},
};

/* Public functions */

long flash_ext_get_size(void)
{
	int err = flash_get_page_info_by_idx(flash_ext_dev, 0, &prv_flash_ext_page_info);
	if (err != 0) {
		LOG_ERR("Failed to get flash size: %d", err);
		return err;
	}

	int page_count = flash_get_page_count(flash_ext_dev);
	long size = prv_flash_ext_page_info.size * page_count;

	return size;
}

int flash_ext_partitions_init(void)
{
	long flash_size = flash_ext_get_size();
	if (flash_size < 0) {
		return flash_size;
	}

	/* Message storage partition
	 *
	 * Because we currently only use one partition we can start form 0. When new
	 * partitions are added, add the start_addr and size accordingly
	 */
	flash_ext_partitions[FLASH_EXT_PARTITION_MESSAGE_STORAGE].start_addr = 0;

	flash_ext_partitions[FLASH_EXT_PARTITION_MESSAGE_STORAGE].size =
		flash_size - flash_ext_partitions[FLASH_EXT_PARTITION_MESSAGE_STORAGE].start_addr;

	prv_flash_ext_initted = true;
	return 0;
}

int flash_ext_get_partition_start_addr(enum flash_ext_partition partition, off_t *offset)
{
	if (!prv_flash_ext_initted) {
		LOG_ERR("Flash partitions not initialized");
		return -EIO;
	}

	switch (partition) {
	case FLASH_EXT_PARTITION_MESSAGE_STORAGE:
		*offset = flash_ext_partitions[FLASH_EXT_PARTITION_MESSAGE_STORAGE].start_addr;
		break;
	default:
		LOG_ERR("Invalid partition: %d", partition);
		return -EINVAL;
	}
	return 0;
}

int flash_ext_get_partition_size(enum flash_ext_partition partition, int *size)
{
	if (!prv_flash_ext_initted) {
		LOG_ERR("Flash partitions not initialized");
		return -EIO;
	}

	switch (partition) {

	case FLASH_EXT_PARTITION_MESSAGE_STORAGE:
		*size = flash_ext_partitions[FLASH_EXT_PARTITION_MESSAGE_STORAGE].size;
		break;
	default:
		*size = 0;
		LOG_ERR("Invalid partition: %d", partition);
		return -EINVAL;
	}
	return 0;
}
