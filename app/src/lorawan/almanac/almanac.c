#include <stdio.h>
#include <string.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/types.h>

#include "almanac.h"
#include "lr11xx_almanac.h"
#include "lr11xx_gnss.h"
#include "lr11xx_gnss_types.h"
#include "lr11xx_radio.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(almanac);

almanac_update_handler_t prv_almanac_update_handler = NULL;

/**
 * @brief Check current almanac status.
 *
 * @param[in] context
 * @return int almanac status error code
 */
static int prv_check_almanac_status(const void *context)
{
	static lr11xx_gnss_context_status_bytestream_t context_status_buffer;
	lr11xx_gnss_context_status_t context_status;

	/* Get gnss context status */
	int ret = lr11xx_gnss_get_context_status(context, context_status_buffer);
	if (ret) {
		LOG_ERR("Failed to get context status");
	}

	/* Parse status */
	ret = lr11xx_gnss_parse_context_status_buffer(context_status_buffer, &context_status);
	if (ret) {
		LOG_ERR("Failed to parse gnss context status");
	}
	LOG_INF("Almanac error code: %02X", context_status.error_code);

	return ret;
}

/**
 * @brief Get current almanac age.
 *
 * @param[in] context
 * @return age of almanac
 */
static uint16_t prv_get_almanac_age(const void *context)
{
	/* Get almanac age */
	uint16_t almanac_age = 0;
	int ret = lr11xx_gnss_get_almanac_age_for_satellite(context, 1, &almanac_age);
	if (ret) {
		LOG_ERR("Failed to get almanac age");
		return ret;
	}

	LOG_INF("Current almanac age: %d", almanac_age);

	return almanac_age;
}

void almanac_update_handler_register(almanac_update_handler_t handler)
{
	prv_almanac_update_handler = handler;
}

int almanac_replace_single_satellite_data(uint8_t *data, uint8_t sat_idx)
{
	if (sat_idx > LR11XX_GNSS_FULL_UPDATE_N_ALMANACS) {
		return -EINVAL;
	}

	memcpy(lr11xx_full_almanac + sat_idx * LR11XX_GNSS_SINGLE_ALMANAC_WRITE_SIZE, data,
	       LR11XX_GNSS_SINGLE_ALMANAC_WRITE_SIZE);

	return LR11XX_GNSS_SINGLE_ALMANAC_WRITE_SIZE;
}

int almanac_update(const void *context)
{
	int ret = 0;

	/* Get gnss context status */
	prv_check_almanac_status(context);

	/* Compare almanac age with age of available almanac */
	uint16_t available_age = (((uint16_t)lr11xx_full_almanac[2]) << 8) +
				 (((uint16_t)lr11xx_full_almanac[1]) << 0);

	uint16_t almanac_age = prv_get_almanac_age(context);

	LOG_INF("Available almanac age: %d, current age: %d", available_age, almanac_age);

	if (almanac_age < available_age) {
		LOG_INF("New Almanac available, start update!");
		ret = lr11xx_gnss_set_almanac_update(context, LR11XX_GNSS_GPS_MASK |
								      LR11XX_GNSS_BEIDOU_MASK);
		if (ret) {
			LOG_ERR("Set update almanac err!");
		} else {

			lr11xx_gnss_constellation_mask_t mask;
			ret = lr11xx_gnss_read_almanac_update(context, &mask);
			if (ret || mask != (LR11XX_GNSS_GPS_MASK | LR11XX_GNSS_BEIDOU_MASK)) {
				LOG_ERR("Almanac update mask error!");
			} else {

				ret = lr11xx_gnss_almanac_update(
					context, lr11xx_full_almanac,
					LR11XX_GNSS_FULL_UPDATE_N_ALMANACS + 1);
				if (ret) {
					LOG_ERR("Almanac update error!");
				} else {
					LOG_INF("Almanac update successful!");
					almanac_age = prv_get_almanac_age(context);
				}
			}
		}
	}

	/* If defined, call external handler */
	if (prv_almanac_update_handler) {
		prv_almanac_update_handler(ret, almanac_age);
	}

	return ret;
}
