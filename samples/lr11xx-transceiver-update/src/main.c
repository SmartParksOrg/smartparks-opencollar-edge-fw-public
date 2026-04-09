#include <stdio.h>
#include <string.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/types.h>

#include <lr11xx_firmware_update.h>

#include "apps_common.h"

LOG_MODULE_REGISTER(main);

#if CONFIG_UPDATE_LR1110
#include "lr1110_transceiver_0307.h"
#elif CONFIG_UPDATE_LR1120
#include "lr1120_transceiver_0101.h"
#else
#error "Unsupported update target. Select either CONFIG_UPDATE_LR1110 or CONFIG_UPDATE_LR1120."

/* Bellow line is here so that only the above error is printed */
#include "lr1110_transceiver_0307.h"
#endif

const struct device *context = DEVICE_DT_GET_ONE(irnas_lr11xx);

void main(void)
{
	apps_common_lr11xx_system_init(context);

	if (LR11XX_FIRMWARE_UPDATE_TO == LR1110_FIRMWARE_UPDATE_TO_TRX) {
		LOG_INF("Updating LR1110 to transceiver firmware 0x%04x ", LR11XX_FIRMWARE_VERSION);
	} else if (LR11XX_FIRMWARE_UPDATE_TO == LR1120_FIRMWARE_UPDATE_TO_TRX) {
		LOG_INF("Updating LR1120 to transceiver firmware 0x%04x ", LR11XX_FIRMWARE_VERSION);
	} else {
		LOG_ERR("Unsupported update type: 0x%04x ", LR11XX_FIRMWARE_VERSION);
		return;
	}

	lr11xx_bootloader_chip_eui_t chip_eui = {0};

	const lr11xx_fw_update_status_t status = lr11xx_update_firmware(
		context, LR11XX_FIRMWARE_UPDATE_TO, LR11XX_FIRMWARE_VERSION, lr11xx_firmware_image,
		(uint32_t)LR11XX_FIRMWARE_IMAGE_SIZE, chip_eui);

	switch (status) {
	case LR11XX_FW_UPDATE_OK:
		printk("OK: LR11XX firmware was updated successfully!\n");
		printk("ChipEUI: %02X %02X %02X %02X %02X %02X %02X %02X\n", chip_eui[0],
		       chip_eui[1], chip_eui[2], chip_eui[3], chip_eui[4], chip_eui[5], chip_eui[6],
		       chip_eui[7]);
		break;
	case LR11XX_FW_UPDATE_NOT_IN_PROD_MODE:
		printk("ERR: LR11XX did not enter bootloader mode!\n");
		break;
	case LR11XX_FW_UPDATE_UNSUPPORTED:
		printk("ERR: Selected firmware is not supported on this lr11xx chip! (You might "
		       "have selected a lr1110 firmware but have a lr1120 chip, or vice versa)\n");
		break;
	case LR11XX_FW_UPDATE_ERROR:
		printk("ERR: Firmware version after chip update does not match the selected "
		       "firmware.\n");
		break;
	}
}
