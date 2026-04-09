#ifndef NVS_STORAGE_H__
#define NVS_STORAGE_H__

#include <zephyr/kernel.h>

#define STORAGE_unix_time          0x0100
#define STORAGE_latitude           0x0101
#define STORAGE_longitude          0x0102
#define STORAGE_altitude           0x0103
#define STORAGE_flash_offset       0x0104
#define STORAGE_flash_start_offset 0x0105
#define STORAGE_flash_block_left   0x0106
#define STORAGE_flash_block_offset 0x0107
#define STORAGE_flash_n_messages   0x0108
#define STORAGE_lorawan_ctx_id0    0x0109
#define STORAGE_lorawan_ctx_id1    0x010A
#define STORAGE_lorawan_ctx_id2    0x010B
#define STORAGE_lorawan_ctx_id3    0x010C
#define STORAGE_lorawan_ctx_id4    0x010D
#define STORAGE_lorawan_ctx_id5    0x010E

int nvs_storage_init(void);
int nvs_storage_clear(void);
int nvs_storage_write(uint16_t id, const void *data, size_t len);
int nvs_storage_read(uint16_t id, void *data, size_t len);
int nvs_storage_delete(uint16_t id);

#endif // NVS_STORAGE_H__
