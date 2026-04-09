#include <string.h>
#include <zephyr/device.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/fs/nvs.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <pm_config.h>

#include "nvs_storage.h"

#define NVS_SECTOR_COUNT 5U

#define NVS_PARTITION_DEVICE DEVICE_DT_GET(DT_NODELABEL(PM_NVS_STORAGE_DEV))
#define NVS_PARTITION_OFFSET PM_NVS_STORAGE_OFFSET

LOG_MODULE_REGISTER(nvs, 3);

static struct nvs_fs fs;
struct flash_pages_info info;

int nvs_storage_init(void)
{
	// Settings
	fs.flash_device = NVS_PARTITION_DEVICE;
	if (!device_is_ready(fs.flash_device)) {
		LOG_ERR("Flash device %s is not ready", fs.flash_device->name);
		return -EIO;
	}
	fs.offset = NVS_PARTITION_OFFSET;
	int err = flash_get_page_info_by_offs(fs.flash_device, fs.offset, &info);
	if (err) {
		LOG_ERR("Flash info init failed");
		return err;
	}

	fs.sector_size = info.size;
	fs.sector_count = NVS_SECTOR_COUNT;

	LOG_WRN("Offset: %ld Sector size: %d Sector count: %d", fs.offset, fs.sector_size,
		fs.sector_count);
	err = nvs_mount(&fs);
	if (err) {
		LOG_ERR("NVS storage Init failed");
		return err;
	}

	LOG_INF("Calculated free NVS space: %d", nvs_calc_free_space(&fs));

	return 0;
}

int nvs_storage_clear(void)
{
	return nvs_clear(&fs);
}

int nvs_storage_write(uint16_t id, const void *data, size_t len)
{
	size_t write_len = nvs_write(&fs, id, data, len);
	if (write_len == 0) {
		LOG_DBG("Data already written!");
	} else if (write_len != len) {
		LOG_ERR("NVS write failed due to length issue, write: %d expected: %d", write_len,
			len);
		return -1;
	}
	return 0;
}

int nvs_storage_read(uint16_t id, void *data, size_t len)
{
	size_t read_len = nvs_read(&fs, id, data, len);
	if (read_len != len) {
		LOG_DBG("Got length: %d, should be: %d", read_len, len);
		return -1;
	}
	return len;
}

int nvs_storage_delete(uint16_t id)
{
	return nvs_delete(&fs, id);
}
